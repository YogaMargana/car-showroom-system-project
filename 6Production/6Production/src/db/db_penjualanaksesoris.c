#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "db_penjualanaksesoris.h"

#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <stdio.h>
#include <string.h>

static void EscapeSql(const char *src, char *dst, size_t dstSize)
{
    size_t j = 0;
    for (size_t i = 0; src && src[i] && j + 1 < dstSize; i++)
    {
        if (src[i] == '\'')
        {
            if (j + 2 < dstSize)
            {
                dst[j++] = '\'';
                dst[j++] = '\'';
            }
            else
                break;
        }
        else
            dst[j++] = src[i];
    }
    dst[j] = '\0';
}

static bool ExecSQL(SQLHDBC dbc, const char *sql)
{
    if (!dbc || !sql)
        return false;

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt)))
        return false;

    SQLRETURN r = SQLExecDirect(stmt, (SQLCHAR *)sql, SQL_NTS);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return SQL_SUCCEEDED(r);
}

bool DbPenjualanAksesoris_LoadAll(void *dbcVoid,
                                  PenjualanAksesorisdata *out,
                                  int outCap,
                                  int *outCount)
{
    if (outCount) *outCount = 0;
    if (!dbcVoid || !out || outCap <= 0 || !outCount) return false;

    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    const char *sql =
        "SELECT "
        "  p.PenjualanAksesorisID, "
        "  p.NoTransaksi, "
        "  CONVERT(varchar(10), p.TanggalTransaksi, 23) AS TanggalTransaksi, "
        "  d.AksesorisID, "
        "  a.NamaAksesoris AS AksesorisNama, "
        "  p.KasirID, "
        "  p.SalesID, "
        "  p.PelangganID, "
        "  pg.Nama AS PelangganNama, "
        "  CAST(ISNULL(d.Qty,0) AS varchar(12)) AS JumlahProduk, "
        "  p.StatusPembayaran, "
        "  CONVERT(varchar(32), p.Total) AS Total, "
        "  CONVERT(varchar(32), p.Uang) AS Uang, "
        "  CONVERT(varchar(32), p.Kembalian) AS Kembalian, "
        "  s.Nama AS SalesNama, "
        "  k.Nama AS KasirNama "
        "FROM dbo.PenjualanAksesoris p "
        "LEFT JOIN dbo.PenjualanAksesorisDetail d ON d.PenjualanAksesorisID = p.PenjualanAksesorisID "
        "LEFT JOIN dbo.Aksesoris a ON a.AksesorisID = d.AksesorisID "
        "LEFT JOIN dbo.Pelanggan pg ON pg.PelangganID = p.PelangganID "
        "LEFT JOIN dbo.Karyawan s ON s.KaryawanID = p.SalesID "
        "LEFT JOIN dbo.Karyawan k ON k.KaryawanID = p.KasirID "
        "ORDER BY p.TanggalTransaksi DESC, p.PenjualanAksesorisID DESC;";


    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt)))
        return false;

    if (!SQL_SUCCEEDED(SQLExecDirect(stmt, (SQLCHAR *)sql, SQL_NTS)))
    {
        // DEBUG biar tau error apa kalau terjadi lagi
        SQLCHAR state[6] = {0}, msg[256] = {0};
        SQLINTEGER native = 0;
        SQLSMALLINT len = 0;
        if (SQLGetDiagRec(SQL_HANDLE_STMT, stmt, 1, state, &native, msg, sizeof(msg), &len) == SQL_SUCCESS)
            printf("[DbPenjualanAksesoris_LoadAll] ODBC Error [%s] %ld: %s\n", state, (long)native, msg);

        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    int n = 0;
    SQLRETURN fr;
    while ((fr = SQLFetch(stmt)) != SQL_NO_DATA && SQL_SUCCEEDED(fr) && n < outCap)
    {
        PenjualanAksesorisdata c;
        memset(&c, 0, sizeof(c));

        SQLGetData(stmt, 1,  SQL_C_CHAR, c.PenjualanAksesorisID, sizeof(c.PenjualanAksesorisID), NULL);
        SQLGetData(stmt, 2,  SQL_C_CHAR, c.NoTransaksi,          sizeof(c.NoTransaksi),          NULL);
        SQLGetData(stmt, 3,  SQL_C_CHAR, c.TanggalTransaksi,     sizeof(c.TanggalTransaksi),     NULL);
        SQLGetData(stmt, 4,  SQL_C_CHAR, c.AksesorisID,          sizeof(c.AksesorisID),          NULL);
        SQLGetData(stmt, 5,  SQL_C_CHAR, c.AksesorisNama,        sizeof(c.AksesorisNama),        NULL);
        SQLGetData(stmt, 6,  SQL_C_CHAR, c.KasirID,              sizeof(c.KasirID),              NULL);
        SQLGetData(stmt, 7,  SQL_C_CHAR, c.SalesID,              sizeof(c.SalesID),              NULL);
        SQLGetData(stmt, 8,  SQL_C_CHAR, c.PelangganID,          sizeof(c.PelangganID),          NULL);
        SQLGetData(stmt, 9,  SQL_C_CHAR, c.PelangganNama,        sizeof(c.PelangganNama),        NULL);
        SQLGetData(stmt, 10, SQL_C_CHAR, c.JumlahProduk,         sizeof(c.JumlahProduk),         NULL);
        SQLGetData(stmt, 11, SQL_C_CHAR, c.StatusPembayaran,     sizeof(c.StatusPembayaran),     NULL);
        SQLGetData(stmt, 12, SQL_C_CHAR, c.Total,                sizeof(c.Total),                NULL);
        SQLGetData(stmt, 13, SQL_C_CHAR, c.Uang,                 sizeof(c.Uang),                 NULL);
        SQLGetData(stmt, 14, SQL_C_CHAR, c.Kembalian,            sizeof(c.Kembalian),            NULL);
        SQLGetData(stmt, 15, SQL_C_CHAR, c.SalesNama,            sizeof(c.SalesNama),            NULL);
        SQLGetData(stmt, 16, SQL_C_CHAR, c.KasirNama,            sizeof(c.KasirNama),            NULL);

        out[n++] = c;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    *outCount = n;
    return true;
}

bool DbPenjualanAksesoris_Insert(void *dbcVoid,
                             const char *NoTransaksi,
                             const char *AksesorisID,
                             const char *KasirID,
                             const char *SalesID,
                             const char *PelangganID,
                             const char *JumlahProduk,
                             const char *Total,
                             const char *Uang)
{
    if (!dbcVoid)
        return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char noTrE[64],AksesorisE[64], kasirE[64], salesE[64], pelangganE[64];
    char qtyE[32], totalE[32], uangE[32];

    EscapeSql(NoTransaksi ? NoTransaksi : "", noTrE, sizeof(noTrE));
    EscapeSql(AksesorisID ? AksesorisID : "", AksesorisE, sizeof(AksesorisE));
    EscapeSql(KasirID ? KasirID : "", kasirE, sizeof(kasirE));
    EscapeSql(SalesID ? SalesID : "", salesE, sizeof(salesE));
    EscapeSql(PelangganID ? PelangganID : "", pelangganE, sizeof(pelangganE));
    EscapeSql(JumlahProduk ? JumlahProduk : "0", qtyE, sizeof(qtyE));
    EscapeSql(Total ? Total : "0", totalE, sizeof(totalE));
    EscapeSql(Uang ? Uang : "0", uangE, sizeof(uangE));

    // Skema DB baru:
    // - Header: dbo.PenjualaAksesoris (tanpaAksesorisID/JumlahProduk)
    // - Detail: dbo.PenjualaAksesorisDetail AksesorisID, Qty, Harga)
    // Trigger stok ada di detail, jadi jangan update stok manual di aplikasi.

    char sql[1800];
    snprintf(sql, sizeof(sql),
             "DECLARE @newId TABLE (PenjualanakAksesorisID VARCHAR(7)); "
             "INSERT INTO dbo.PenjualanakAksesoris (NoTransaksi, SalesID, KasirID, PelangganID, StatusPembayaran, Total, Uang) "
             "OUTPUT inserted.PenjualanakAksesorisID INTO @newId(PenjualanakAksesorisID) "
             "VALUES ('%s','%s','%s','%s','Berhasil',%s,%s); "
             "INSERT INTO dbo.PenjualanakAksesorisDetail (PenjualanakAksesorisID, akAksesorisID, Qty, Harga) "
             "SELECT PenjualanakAksesorisID, '%s', %s, (CASE WHEN %s > 0 THEN (%s / %s) ELSE 0 END) "
             "FROM @newId;",
             noTrE, salesE, kasirE, pelangganE, totalE, uangE,
             AksesorisE, qtyE, qtyE, totalE, qtyE);

    return ExecSQL(dbc, sql);
}

bool DbPenjualanAksesoris_CreateNoTransaksi(void *dbcVoid,
                                        char *outNo,
                                        int outSize)
{
    if (!dbcVoid || !outNo || outSize <= 0)
        return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    const char *sql = "SELECT NEXT VALUE FOR dbo.SeqNoTransaksi";

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt)))
        return false;

    if (!SQL_SUCCEEDED(SQLExecDirect(stmt, (SQLCHAR *)sql, SQL_NTS)))
    {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    int nextNo = 1;
    if (SQLFetch(stmt) != SQL_NO_DATA)
        SQLGetData(stmt, 1, SQL_C_SLONG, &nextNo, 0, NULL);

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    snprintf(outNo, (size_t)outSize, "TRX%05d", nextNo);
    return true;
}

bool DbPenjualanAksesoris_Update(void *dbcVoid,
                             const char *PenjualanAksesorisID,
                             const char *NoTransaksi,
                             const char *AksesorisID,
                             const char *KasirID,
                             const char *SalesID,
                             const char *PelangganID,
                             const char *JumlahProduk,
                             const char *Total,
                             const char *Uang)
{
    if (!dbcVoid || !PenjualanAksesorisID)
        return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char idE[32];
    EscapeSql(PenjualanAksesorisID, idE, sizeof(idE));

    char noTrE[64],AksesorisE[64], kasirE[64], salesE[64], pelangganE[64];
    char jmlE[32], totalE[32], uangE[32];

    EscapeSql(NoTransaksi ? NoTransaksi : "", noTrE, sizeof(noTrE));
    EscapeSql(AksesorisID ? AksesorisID : "", AksesorisE, sizeof(AksesorisE));
    EscapeSql(KasirID ? KasirID : "", kasirE, sizeof(kasirE));
    EscapeSql(SalesID ? SalesID : "", salesE, sizeof(salesE));
    EscapeSql(PelangganID ? PelangganID : "", pelangganE, sizeof(pelangganE));
    EscapeSql(JumlahProduk ? JumlahProduk : "", jmlE, sizeof(jmlE));
    EscapeSql(Total ? Total : "", totalE, sizeof(totalE));
    EscapeSql(Uang ? Uang : "", uangE, sizeof(uangE));

    char sql[1400];
    snprintf(sql, sizeof(sql),
             "UPDATE dbo.PenjualanakAksesoris "
             "SET NoTransaksi='%s', akAksesorisID='%s', KasirID='%s', "
             "SalesID='%s', PelangganID='%s', JumlahProduk=%s, Total=%s, Uang=%s "
             "WHERE PenjualanakAksesorisID='%s'",
             noTrE, AksesorisE, kasirE, salesE, pelangganE,
             jmlE, totalE, uangE, idE);

    return ExecSQL(dbc, sql);
}

bool DbPenjualanAksesoris_Delete(void *dbcVoid,
                             const char *PenjualanAksesorisID)
{
    if (!dbcVoid || !PenjualanAksesorisID)
        return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char idE[32];
    EscapeSql(PenjualanAksesorisID, idE, sizeof(idE));

    char sql[256];
    snprintf(sql, sizeof(sql),
             "DELETE FROM dbo.PenjualanakAksesoris WHERE PenjualanakAksesorisID='%s'",
             idE);

    return ExecSQL(dbc, sql);
}
