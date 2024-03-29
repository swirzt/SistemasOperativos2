/// Simple test routines for the file system.
///
/// We implement:
///
/// Copy
///     Copy a file from UNIX to Nachos.
/// Print
///     Cat the contents of a Nachos file.
/// Perftest
///     A stress test for the Nachos file system read and write a really
///     really large file in tiny chunks (will not work on baseline system!)
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#include "file_system.hh"
#include "lib/utility.hh"
#include "machine/disk.hh"
#include "machine/statistics.hh"
#include "threads/thread.hh"
#include "threads/system.hh"

#include <stdio.h>
#include <string.h>

static const unsigned TRANSFER_SIZE = 10; // Make it small, just to be
                                          // difficult.

/// Copy the contents of the UNIX file `from` to the Nachos file `to`.
void Copy(const char *from, const char *to)
{
    ASSERT(from != nullptr);
    ASSERT(to != nullptr);

    // Open UNIX file.
    FILE *fp = fopen(from, "r");
    if (fp == nullptr)
    {
        printf("Copy: could not open input file %s\n", from);
        return;
    }

    // Figure out length of UNIX file.
    fseek(fp, 0, 2);
    int fileLength = ftell(fp);
    fseek(fp, 0, 0);

    DEBUG('f', "Copying file %s, size %u, to file %s\n",
          from, fileLength, to);

    // Create a Nachos file of the same length.
    if (!fileSystem->Create(to))
    { // Create Nachos file.
        printf("Copy: could not create output file %s\n", to);
        fclose(fp);
        return;
    }

    OpenFile *openFile = fileSystem->Open(to);
    ASSERT(openFile != nullptr);

    // Copy the data in `TRANSFER_SIZE` chunks.
    char *buffer = new char[TRANSFER_SIZE];
    int amountRead;
    while ((amountRead = fread(buffer, sizeof(char),
                               TRANSFER_SIZE, fp)) > 0)
        openFile->Write(buffer, amountRead);
    delete[] buffer;

    // Close the UNIX and the Nachos files.
    fileSystem->Close(openFile);
    fclose(fp);
}

/// Print the contents of the Nachos file `name`.
void Print(const char *name)
{
    ASSERT(name != nullptr);

    OpenFile *openFile = fileSystem->Open(name);
    if (openFile == nullptr)
    {
        fprintf(stderr, "Print: unable to open file %s\n", name);
        return;
    }

    char *buffer = new char[TRANSFER_SIZE];
    int amountRead;
    while ((amountRead = openFile->Read(buffer, TRANSFER_SIZE)) > 0)
    {
        for (unsigned i = 0; i < (unsigned)amountRead; i++)
        {
            putchar(buffer[i]);
        }
    }

    delete[] buffer;
    fileSystem->Close(openFile); // close the Nachos file
}

/// Performance test
///
/// Stress the Nachos file system by creating a large file, writing it out a
/// bit at a time, reading it back a bit at a time, and then deleting the
/// file.
///
/// Implemented as three separate routines:
/// * `FileWrite` -- write the file.
/// * `FileRead` -- read the file.
/// * `PerformanceTest` -- overall control, and print out performance #'s.

static const char FILE_NAME[] = "TestFile";
static const char CONTENTS[] = "1234567890";
static const unsigned CONTENT_SIZE = sizeof CONTENTS - 1;
static const unsigned FILE_SIZE = CONTENT_SIZE * 5000;

static void
FileWrite()
{
    printf("Sequential write of %u byte file, in %u byte chunks\n",
           FILE_SIZE, CONTENT_SIZE);

    if (!fileSystem->Create(FILE_NAME))
    {
        fprintf(stderr, "Perf test: cannot create %s\n", FILE_NAME);
        return;
    }

    OpenFile *openFile = fileSystem->Open(FILE_NAME);
    if (openFile == nullptr)
    {
        fprintf(stderr, "Perf test: unable to open %s\n", FILE_NAME);
        return;
    }

    for (unsigned i = 0; i < FILE_SIZE; i += CONTENT_SIZE)
    {
        int numBytes = openFile->Write(CONTENTS, CONTENT_SIZE);
        if (numBytes < 10)
        {
            fprintf(stderr, "Perf test: unable to write %s\n", FILE_NAME);
            break;
        }
    }

    fileSystem->Close(openFile);
}

static void
FileRead()
{
    printf("Sequential read of %u byte file, in %u byte chunks\n",
           FILE_SIZE, CONTENT_SIZE);

    OpenFile *openFile = fileSystem->Open(FILE_NAME);
    if (openFile == nullptr)
    {
        fprintf(stderr, "Perf test: unable to open file %s\n", FILE_NAME);
        return;
    }

    char *buffer = new char[CONTENT_SIZE];
    for (unsigned i = 0; i < FILE_SIZE; i += CONTENT_SIZE)
    {
        int numBytes = openFile->Read(buffer, CONTENT_SIZE);
        if (numBytes < 10 || strncmp(buffer, CONTENTS, CONTENT_SIZE))
        {
            printf("Perf test: unable to read %s\n", FILE_NAME);
            break;
        }
    }

    delete[] buffer;
    fileSystem->Close(openFile);
}

void PerformanceTest()
{
    printf("Starting file system performance test:\n");
    stats->Print();
    FileWrite();
    FileRead();
    if (!fileSystem->Remove(FILE_NAME))
    {
        printf("Perf test: unable to remove %s\n", FILE_NAME);
        return;
    }
    stats->Print();
}

static const char CONTENTS_SYNC[] = "Escribo!";
static const unsigned CONTENT_SIZE_SYNC = sizeof CONTENTS_SYNC - 1;

static void Escribo(void *n_)
{
    OpenFile *openFile = fileSystem->Open("test");
    char *escribir = new char[16];
    sprintf(escribir, "Soy el numero %u", *((unsigned *)n_));
    DEBUG('f', "Escribiendo %s\n", escribir);
    int len = strlen(escribir);
    openFile->WriteAt(escribir, len, *((unsigned *)n_) * len);
    printf("Soy %u y ya termine de escribir de largo %d\n", *((unsigned *)n_), len);
    fileSystem->Close(openFile);
    printf("Soy %u y ya cerre el archivo \n", *((unsigned *)n_));
}

void ConcurrentFSTest()
{
    const int CANT_THREADS = 3;
    List<Thread *> *listaHijos = new List<Thread *>;
    // Launch a new thread for each turnstile.
    fileSystem->Create("test");
    for (unsigned i = 0; i < CANT_THREADS; i++)
    {
        printf("Iniciando hilos para escribir %u.\n", i);
        char *name = new char[16];
        sprintf(name, "Escritor %u", i);
        unsigned *n = new unsigned;
        *n = i;
        DEBUG('t', "Empezando fork %ld\n", i);
        Thread *t = new Thread(name, true, 0);

        t->Fork(Escribo, (void *)n);
        listaHijos->Append(t);
    }

    for (int i = 0; i < activeThreads->getSize(); i++)
    {
        // Print element of activeThreads
        Thread *temp = activeThreads->Get(i);
        DEBUG('t', "Threaasdasdd %s %p \n", temp->GetName(), temp);
    }

    while (!listaHijos->IsEmpty())
    {
        Thread *t = listaHijos->Pop();
        DEBUG('t', "Voy a joinear el hilo %s\n", t->GetName());
        t->Join();
    }
    interrupt->Halt();
    return;
}
