#ifndef DB_TESTDRIVE_H
#define DB_TESTDRIVE_H

#include <stdbool.h>

typedef struct {
    char TestDriveID[16];
    char MobilID[16];
    char KaryawanID[16];
    char PelangganID[16];
    char Tanggal[16];
    char Status[16];
} TestDrive;

bool DbTestDrive_LoadAll(void *dbc, TestDrive *out, int outCap, int *outCount);
bool DbTestDrive_Insert(void *dbc,
                        const char *mobilId,
                        const char *karyawanId,
                        const char *pelangganId,
                        const char *tanggal,
                        const char *status);
bool DbTestDrive_Update(void *dbc,
                        const char *testDriveId,
                        const char *mobilId,
                        const char *karyawanId,
                        const char *pelangganId,
                        const char *tanggal,
                        const char *status);
bool DbTestDrive_Delete(void *dbc, const char *testDriveId);

#endif
