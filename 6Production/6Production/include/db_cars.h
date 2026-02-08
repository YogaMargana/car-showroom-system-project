#ifndef DB_CARS_H
#define DB_CARS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CarData
{
    char MobilID[16];
    char NamaMobil[64];
    char TipeMobil[64];
    char Stok[16];
    char TahunProduksi[16];
    char Harga[32];
    int  IsActive;  // 1 = active, 0 = inactive
} CarData;

bool DbCars_LoadAll(void *dbcVoid, CarData *out, int outCap, int *outCount);

bool DbCars_Insert(void *dbcVoid,
                   const char *tipeMobil,
                   const char *nama,
                   const char *stok,
                   const char *tahunProduksi,
                   const char *harga);

bool DbCars_Update(void *dbcVoid,
                   const char *mobilId,
                   const char *tipeMobil,
                   const char *nama,
                   const char *stok,
                   const char *tahunProduksi,
                   const char *harga);

/* Deactive / Active */
bool DbCars_Deactivate(void *dbcVoid, const char *mobilId);
bool DbCars_Activate(void *dbcVoid, const char *mobilId);

/* Backward compatible: Delete = Deactivate */
bool DbCars_Delete(void *dbcVoid, const char *mobilId);

/* Update stok (optional) */
bool DbCarData_UpdateStock(void *dbcVoid, const char *mobilId, int newStock);

#ifdef __cplusplus
}
#endif

#endif
