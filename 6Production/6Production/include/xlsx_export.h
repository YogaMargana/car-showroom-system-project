#pragma once

#include <stdbool.h>

// Simple .xlsx exporter (OpenXML). Creates a workbook with a single sheet.
// Cells can be written as string or number.

typedef struct XlsxBook XlsxBook;

// Creates an .xlsx file at path with a single worksheet named sheetName.
// Returns NULL on failure.
XlsxBook *Xlsx_Create(const char *xlsxPath, const char *sheetName);

// Write a string cell at (row, col), 0-based.
bool Xlsx_WriteString(XlsxBook *b, int row, int col, const char *utf8);

// Write a number cell at (row, col), 0-based.
bool Xlsx_WriteNumber(XlsxBook *b, int row, int col, double v);

// Finalize & close the workbook.
bool Xlsx_Close(XlsxBook *b);
