#ifndef DB_CARDATA_H
#define DB_CARDATA_H

#include <stdbool.h>

typedef struct {
    char MobilID[16];
    char NamaMobil[50];
    char TahunProduksi[5];
    char Harga[20];
    char Stok[10];
} Car;

bool DbCarData_LoadAll(void *dbc, Car *out, int outCap, int *outCount);

bool DbCarData_Insert(void *dbc,
                      const char *NamaMobil,
                      const char *TahunProduksi,
                      const char *Harga,
                      const char *Stok);

bool DbCarData_Update(void *dbc,
                      const char *MobilID,
                      const char *NamaMobil,
                      const char *TahunProduksi,
                      const char *Harga,
                      const char *Stok);

bool DbCarData_Delete(void *dbc, const char *MobilID);

// Kurangi stok (dipakai saat transaksi penjualan berhasil).
// Return false jika stok tidak cukup / query gagal.
bool DbCarData_DecreaseStock(void *dbc, const char *MobilID, int qty);

#endif
