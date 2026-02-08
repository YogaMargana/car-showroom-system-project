#pragma once

#include <stddef.h>
#include <stdbool.h>

// Minimal ZIP writer (store-only, no compression) for generating .xlsx (OpenXML is a ZIP container).
// Supports adding entries from memory buffers.

typedef struct ZipWriter ZipWriter;

// Opens a new zip file for writing. Returns NULL on failure.
ZipWriter *ZipWriter_Open(const char *zipPath);

// Adds a file entry to the zip.
// - zipEntryName uses forward slashes, e.g. "xl/workbook.xml".
// - data can be NULL if size==0.
// Returns false on failure.
bool ZipWriter_AddFile(ZipWriter *zw, const char *zipEntryName, const void *data, size_t size);

// Finalizes the central directory and closes the zip.
// Returns false on failure.
bool ZipWriter_Close(ZipWriter *zw);
