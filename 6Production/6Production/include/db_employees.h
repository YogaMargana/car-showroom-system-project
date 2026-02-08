#ifndef DB_EMPLOYEES_H
#define DB_EMPLOYEES_H

#include <stdbool.h>

typedef struct {
    char KaryawanID[16];
    char Nama[50];
    char Posisi[20];
    char NoHP[15];
    char Email[50];
    char Username[50];
    char Password[255];
} Employee;

bool DbEmployees_LoadAll(void *dbc, Employee *out, int outCap, int *outCount);

bool DbEmployees_Insert(void *dbc,
                        const char *nama,
                        const char *posisi,
                        const char *nohp,
                        const char *email,
                        const char *username,
                        const char *password);

bool DbEmployees_Update(void *dbc,
                        const char *karyawanId,
                        const char *nama,
                        const char *posisi,
                        const char *nohp,
                        const char *email,
                        const char *username,
                        const char *password);

bool DbEmployees_Delete(void *dbc, const char *karyawanId);

#endif
