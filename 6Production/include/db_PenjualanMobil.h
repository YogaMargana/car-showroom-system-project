#ifndef DB_PENJUALANMOBIL_H
#define DB_PENJUALANMOBIL_H

#include <stdbool.h>

typedef struct
{
    char PenjualanMobilID[20];
    char NoTransaksi[50];
    char MobilID[20];
    char SalesID[20];
    char KasirID[20];
    char PelangganID[20];

    char Qty[10];
    char TanggalTransaksi[20];
    char StatusPembayaran[50];

    char Total[20];
    char Uang[20];
    char Kembalian[20];

    char SalesNama[100];
    char KasirNama[100];
} Penjualanmobildata;

bool DbPenjualanMobil_LoadAll(void *dbcVoid, Penjualanmobildata *out, int outCap, int *outCount);

bool DbPenjualanMobil_Insert(void *dbcVoid,
                             const char *NoTransaksi,
                             const char *MobilID,
                             const char *SalesID,
                             const char *KasirID,
                             const char *PelangganID,
                             const char *Qty,
                             const char *Total,
                             const char *Uang);

bool DbPenjualanMobil_CreateNoTransaksi(void *dbcVoid, char *outNo, int outSize);

bool DbPenjualanMobil_Update(void *dbcVoid,
                             const char *PenjualanMobilID,
                             const char *NoTransaksi,
                             const char *MobilID,
                             const char *SalesID,
                             const char *KasirID,
                             const char *PelangganID,
                             const char *Qty,
                             const char *Total,
                             const char *Uang);

bool DbPenjualanMobil_Delete(void *dbcVoid, const char *PenjualanMobilID);

#endif
