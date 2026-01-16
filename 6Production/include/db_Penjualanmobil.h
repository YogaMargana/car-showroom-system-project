#ifndef DB_PENJUALANMOBIL_H
#define DB_PENJUALANMOBIL_H

#include <stdbool.h>
#include "cart_manager.h"

typedef struct
{
    char PenjualanMobilID[20];
    char NoTransaksi[50];
    char MobilID[20];
    char KasirID[20];        // kolom KasirID di tabel
    char SalesID[20];
    char PelangganID[20];

    char JumlahProduk[10];
    char TanggalTransaksi[20];
    char StatusPembayaran[50];

    char Total[20];
    char Uang[20];
    char Kembalian[20];

    char SalesNama[100];
    char KasirNama[100];
} Penjualanmobildata;

bool DbPenjualanMobil_LoadAll(void *dbcVoid,
                              Penjualanmobildata *out,
                              int outCap,
                              int *outCount);

bool DbPenjualanMobil_Insert(void *dbcVoid,
                             const char *NoTransaksi,
                             const char *MobilID,
                             const char *KasirID,
                             const char *SalesID,
                             const char *PelangganID,
                             const char *JumlahProduk,
                             const char *Total,
                             const char *Uang);

bool DbPenjualanMobil_CreateNoTransaksi(void *dbcVoid,
                                        char *outNo,
                                        int outSize);

bool DbPenjualanMobil_Update(void *dbcVoid,
                             const char *PenjualanMobilID,
                             const char *NoTransaksi,
                            //  const char *MobilID,
                             const char *KasirID,
                             const char *SalesID,
                             const char *PelangganID,
                            //  const char *JumlahProduk,
                             const char *Total,
                             const char *Uang);

bool DbPenjualanMobil_Delete(void *dbcVoid,
                             const char *PenjualanMobilID);
                             
bool DbPenjualanMobil_InsertBatch(
    void *dbc,
    const char *kasirID,
    const char *salesID,
    const char *pelangganID,
    CartItemData *items,   // ← Parameter ke-5
    int itemCount,         // ← Parameter ke-6
    long long total,       // ← Parameter ke-7
    long long uang,        // ← Parameter ke-8
    char *outNoTransaksi,
    int outNoTransaksiSize
);
#endif
