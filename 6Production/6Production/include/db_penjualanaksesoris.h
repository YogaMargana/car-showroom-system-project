#ifndef DB_PENJUALANAKSESORIS_H
#define DB_PENJUALANAKSESORIS_H

#include <stdbool.h>

typedef struct
{
    char PenjualanAksesorisID[20];
    char NoTransaksi[50];
    char AksesorisID[20];
    char AksesorisNama[100];   // untuk tampilan
    char KasirID[20];        // kolom KasirID di tabel
    char SalesID[20];
    char PelangganID[20];
    char PelangganNama[100];   // untuk tampilan

    char JumlahProduk[10];
    char TanggalTransaksi[20];
    char StatusPembayaran[50];

    char Total[20];
    char Uang[20];
    char Kembalian[20];

    char SalesNama[100];
    char KasirNama[100];
} PenjualanAksesorisdata;

bool DbPenjualanAksesoris_LoadAll(void *dbcVoid,
                              PenjualanAksesorisdata *out,
                              int outCap,
                              int *outCount);

bool DbPenjualanAksesoris_Insert(void *dbcVoid,
                             const char *NoTransaksi,
                             const char *AksesorisID,
                             const char *KasirID,
                             const char *SalesID,
                             const char *PelangganID,
                             const char *JumlahProduk,
                             const char *Total,
                             const char *Uang);

bool DbPenjualanAksesoris_CreateNoTransaksi(void *dbcVoid,
                                        char *outNo,
                                        int outSize);

bool DbPenjualanAksesoris_Update(void *dbcVoid,
                             const char *PenjualanAksesorisID,
                             const char *NoTransaksi,
                             const char *AksesorisID,
                             const char *KasirID,
                             const char *SalesID,
                             const char *PelangganID,
                             const char *JumlahProduk,
                             const char *Total,
                             const char *Uang);

bool DbPenjualanAksesoris_Delete(void *dbcVoid,
                             const char *PenjualanAksesorisID);

#endif
