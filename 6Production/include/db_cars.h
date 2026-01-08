#ifndef DB_CARS_H
#define DB_CARS_H

#include <stdbool.h>

typedef struct {
    char MobilID[16];
    char NamaMobil[32];
    char TipeMobil[50];
    char Stok[8];
    char TahunProduksi[8];
    char Harga[32];
} CarData;

bool DbCars_LoadAll(void *dbc, CarData *out, int outCap, int *outCount);
bool DbCars_Insert(void *dbc,
                   const char *tipeMobil,
                   const char *nama,
                   const char *stok,
                   const char *tahunProduksi,
                   const char *harga);
bool DbCars_Update(void *dbc,
                   const char *mobilId,
                   const char *tipeMobil,
                   const char *nama,
                   const char *stok,
                   const char *tahunProduksi,
                   const char *harga);
bool DbCars_Delete(void *dbc, const char *mobilId);

#endif
