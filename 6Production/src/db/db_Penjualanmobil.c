#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "db_penjualanmobil.h"
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

/* ============================================================
   FIXED: LoadAll dengan Deduplicate di Aplikasi (Solusi #1)
   - Query tetap pakai LEFT JOIN ke detail (seperti original)
   - Deduplicate di loop (skip row jika PenjualanMobilID sama)
   ============================================================ */
bool DbPenjualanMobil_LoadAll(void *dbcVoid,
                              Penjualanmobildata *out,
                              int outCap,
                              int *outCount)
{
    if (outCount)
        *outCount = 0;
    if (!dbcVoid || !out || outCap <= 0 || !outCount)
        return false;

    SQLHDBC dbc = (SQLHDBC)dbcVoid;
    
    // Query SAMA seperti original (dengan LEFT JOIN detail)
    const char *sql =
        "SELECT "
        "  p.PenjualanMobilID, "
        "  p.NoTransaksi, "
        "  d.MobilID, "
        "  p.KasirID, "
        "  p.SalesID, "
        "  p.PelangganID, "
        "  CAST(ISNULL(d.Qty,0) AS varchar(12)) AS JumlahProduk, "
        "  CONVERT(varchar(10), p.TanggalTransaksi, 23) AS TanggalTransaksi, "
        "  p.StatusPembayaran, "
        "  p.Total, "
        "  p.Uang, "
        "  p.Kembalian, "
        "  s.Nama AS SalesNama, "
        "  k.Nama AS KasirNama "
        "FROM dbo.PenjualanMobil p "
        "LEFT JOIN dbo.PenjualanMobilDetail d ON d.PenjualanMobilID = p.PenjualanMobilID "
        "LEFT JOIN dbo.Karyawan s ON s.KaryawanID = p.SalesID "
        "LEFT JOIN dbo.Karyawan k ON k.KaryawanID = p.KasirID "
        "ORDER BY p.PenjualanMobilID DESC";

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt)))
        return false;

    if (!SQL_SUCCEEDED(SQLExecDirect(stmt, (SQLCHAR *)sql, SQL_NTS)))
    {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    int n = 0;
    SQLRETURN fr;
    char lastPenjualanMobilID[32] = "";  // TRACK last ID untuk deduplicate
    
    while ((fr = SQLFetch(stmt)) != SQL_NO_DATA && SQL_SUCCEEDED(fr) && n < outCap)
    {
        Penjualanmobildata c;
        memset(&c, 0, sizeof(c));
        
        // Fetch semua data dari query
        SQLGetData(stmt, 1, SQL_C_CHAR, c.PenjualanMobilID, sizeof(c.PenjualanMobilID), NULL);
        SQLGetData(stmt, 2, SQL_C_CHAR, c.NoTransaksi, sizeof(c.NoTransaksi), NULL);
        SQLGetData(stmt, 3, SQL_C_CHAR, c.MobilID, sizeof(c.MobilID), NULL);
        SQLGetData(stmt, 4, SQL_C_CHAR, c.KasirID, sizeof(c.KasirID), NULL);
        SQLGetData(stmt, 5, SQL_C_CHAR, c.SalesID, sizeof(c.SalesID), NULL);
        SQLGetData(stmt, 6, SQL_C_CHAR, c.PelangganID, sizeof(c.PelangganID), NULL);
        SQLGetData(stmt, 7, SQL_C_CHAR, c.JumlahProduk, sizeof(c.JumlahProduk), NULL);
        SQLGetData(stmt, 8, SQL_C_CHAR, c.TanggalTransaksi, sizeof(c.TanggalTransaksi), NULL);
        SQLGetData(stmt, 9, SQL_C_CHAR, c.StatusPembayaran, sizeof(c.StatusPembayaran), NULL);
        SQLGetData(stmt, 10, SQL_C_CHAR, c.Total, sizeof(c.Total), NULL);
        SQLGetData(stmt, 11, SQL_C_CHAR, c.Uang, sizeof(c.Uang), NULL);
        SQLGetData(stmt, 12, SQL_C_CHAR, c.Kembalian, sizeof(c.Kembalian), NULL);
        SQLGetData(stmt, 13, SQL_C_CHAR, c.SalesNama, sizeof(c.SalesNama), NULL);
        SQLGetData(stmt, 14, SQL_C_CHAR, c.KasirNama, sizeof(c.KasirNama), NULL);
        
        // ============================================================
        // DEDUPLICATE LOGIC:
        // Hanya insert jika PenjualanMobilID BERBEDA dari yang sebelumnya
        // Ini mencegah duplicate rows saat 1 transaksi punya 2+ detail
        // ============================================================
        if (strcmp(c.PenjualanMobilID, lastPenjualanMobilID) != 0)
        {
            out[n++] = c;
            strncpy(lastPenjualanMobilID, c.PenjualanMobilID, sizeof(lastPenjualanMobilID) - 1);
            lastPenjualanMobilID[sizeof(lastPenjualanMobilID) - 1] = '\0';
        }
        // Jika PenjualanMobilID sama dengan row sebelumnya → SKIP (ignore duplicate)
    }
    
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    *outCount = n;
    return true;
}

/* ============================================================
   FIXED: Insert dengan Harga dari tabel Mobil
   - Header insert ke PenjualanMobil
   - Detail insert ke PenjualanMobilDetail
   - Harga ambil dari tabel Mobil (bukan calculate)
   ============================================================ */
bool DbPenjualanMobil_Insert(void *dbcVoid,
                             const char *NoTransaksi,
                             const char *MobilID,
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
    char noTrE[64], mobilE[64], kasirE[64], salesE[64], pelangganE[64];
    char qtyE[32], totalE[32], uangE[32];

    EscapeSql(NoTransaksi ? NoTransaksi : "", noTrE, sizeof(noTrE));
    EscapeSql(MobilID ? MobilID : "", mobilE, sizeof(mobilE));
    EscapeSql(KasirID ? KasirID : "", kasirE, sizeof(kasirE));
    EscapeSql(SalesID ? SalesID : "", salesE, sizeof(salesE));
    EscapeSql(PelangganID ? PelangganID : "", pelangganE, sizeof(pelangganE));
    EscapeSql(JumlahProduk ? JumlahProduk : "0", qtyE, sizeof(qtyE));
    EscapeSql(Total ? Total : "0", totalE, sizeof(totalE));
    EscapeSql(Uang ? Uang : "0", uangE, sizeof(uangE));

    // FIXED: Ambil Harga dari tabel Mobil, bukan calculate dari Total/Qty
    char sql[2000];
    snprintf(sql, sizeof(sql),
        "DECLARE @newId TABLE (PenjualanMobilID VARCHAR(7)); "
        "INSERT INTO dbo.PenjualanMobil (NoTransaksi, SalesID, KasirID, PelangganID, StatusPembayaran, Total, Uang) "
        "OUTPUT inserted.PenjualanMobilID INTO @newId(PenjualanMobilID) "
        "VALUES ('%s','%s','%s','%s','Berhasil',%s,%s); "
        "INSERT INTO dbo.PenjualanMobilDetail (PenjualanMobilID, MobilID, Qty, Harga) "
        "SELECT ni.PenjualanMobilID, m.MobilID, %s, m.Harga "
        "FROM @newId ni, dbo.Mobil m "
        "WHERE m.MobilID = '%s'",
        noTrE, salesE, kasirE, pelangganE, totalE, uangE,
        qtyE, mobilE);

    return ExecSQL(dbc, sql);
}

/* ============================================================
   CreateNoTransaksi - Generate nomor transaksi unik
   ============================================================ */
bool DbPenjualanMobil_CreateNoTransaksi(void *dbcVoid,
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

/* ============================================================
   FIXED: Update - HANYA update Header, jangan update Detail di sini
   - MobilID & JumlahProduk TIDAK BOLEH di-update di tabel PenjualanMobil
   - Karena field itu ada di tabel PenjualanMobilDetail
   - Buat function terpisah untuk update detail jika diperlukan
   ============================================================ */
bool DbPenjualanMobil_Update(void *dbcVoid,
                             const char *PenjualanMobilID,
                             const char *NoTransaksi,
                             const char *KasirID,
                             const char *SalesID,
                             const char *PelangganID,
                             const char *Total,
                             const char *Uang)
{
    if (!dbcVoid || !PenjualanMobilID)
        return false;

    SQLHDBC dbc = (SQLHDBC)dbcVoid;
    char idE[32], noTrE[64], kasirE[64], salesE[64], pelangganE[64];
    char totalE[32], uangE[32];

    EscapeSql(PenjualanMobilID, idE, sizeof(idE));
    EscapeSql(NoTransaksi ? NoTransaksi : "", noTrE, sizeof(noTrE));
    EscapeSql(KasirID ? KasirID : "", kasirE, sizeof(kasirE));
    EscapeSql(SalesID ? SalesID : "", salesE, sizeof(salesE));
    EscapeSql(PelangganID ? PelangganID : "", pelangganE, sizeof(pelangganE));
    EscapeSql(Total ? Total : "0", totalE, sizeof(totalE));
    EscapeSql(Uang ? Uang : "0", uangE, sizeof(uangE));

    // FIXED: HANYA update header fields, jangan MobilID & JumlahProduk
    char sql[1000];
    snprintf(sql, sizeof(sql),
        "UPDATE dbo.PenjualanMobil "
        "SET NoTransaksi='%s', KasirID='%s', SalesID='%s', "
        "PelangganID='%s', Total=%s, Uang=%s "
        "WHERE PenjualanMobilID='%s'",
        noTrE, kasirE, salesE, pelangganE, totalE, uangE, idE);

    return ExecSQL(dbc, sql);
}

bool DbPenjualanMobil_Delete(void *dbcVoid,
                             const char *PenjualanMobilID)
{
    if (!dbcVoid || !PenjualanMobilID)
        return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char idE[32];
    EscapeSql(PenjualanMobilID, idE, sizeof(idE));

    char sql[256];
    snprintf(sql, sizeof(sql),
             "DELETE FROM dbo.PenjualanMobil WHERE PenjualanMobilID='%s'",
             idE);

    return ExecSQL(dbc, sql);
}
