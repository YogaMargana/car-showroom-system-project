#ifndef DB_AccessorisS_H
#define DB_AccessorisS_H

#include <stdbool.h>

typedef struct
{
    char AksesorisID[8];      // minimal 7 (6 char + '\0'), saya buat 8 agar aman
    char NamaAksesoris[64];   // lebih aman dari 30 (varchar(30) butuh ruang null-terminator)
    int  Stok;
    int  Harga;               // boleh tetap int dulu (walau kolom SQL adalah money)
} Accessoris;

bool DbAccesoriss_LoadAll(void *dbc, Accessoris *out, int outCap, int *outCount);

bool DbAccesoriss_Insert(void *dbc,
                         const char *NamaAksesoris,
                         int Stok,
                         int Harga);

bool DbAccesoriss_Update(void *dbc,
                         const char *AksesorisID,
                         const char *NamaAksesoris,
                         int Stok,
                         int Harga);

bool DbAccesoriss_Delete(void *dbc, const char *AksesorisID);

#endif
