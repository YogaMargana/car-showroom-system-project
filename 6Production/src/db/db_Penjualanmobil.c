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

bool DbPenjualanMobil_LoadAll(void *dbcVoid, Penjualanmobildata *out, int outCap, int *outCount)
{
    if (outCount)
        *outCount = 0;
    if (!dbcVoid || !out || outCap <= 0 || !outCount)
        return false;

    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    const char *sql =
        "SELECT "
        "  p.PenjualanMobilID, "
        "  p.NoTransaksi, "
        "  p.MobilID, "
        "  p.SalesID, "
        "  p.KasirID, "
        "  p.PelangganID, "
        "  p.Qty, "
        "  CONVERT(varchar(10), p.TanggalTransaksi, 23) AS TanggalTransaksi, "
        "  p.StatusPembayaran, "
        "  p.Total, "
        "  p.Uang, "
        "  p.Kembalian, "
        "  s.Nama AS SalesNama, "
        "  k.Nama AS KasirNama "
        "FROM dbo.PenjualanMobil p "
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
    while ((fr = SQLFetch(stmt)) != SQL_NO_DATA && SQL_SUCCEEDED(fr) && n < outCap)
    {
        Penjualanmobildata c;
        memset(&c, 0, sizeof(c));

        SQLGetData(stmt, 1, SQL_C_CHAR, c.PenjualanMobilID, sizeof(c.PenjualanMobilID), NULL);
        SQLGetData(stmt, 2, SQL_C_CHAR, c.NoTransaksi, sizeof(c.NoTransaksi), NULL);
        SQLGetData(stmt, 3, SQL_C_CHAR, c.MobilID, sizeof(c.MobilID), NULL);
        SQLGetData(stmt, 4, SQL_C_CHAR, c.SalesID, sizeof(c.SalesID), NULL);
        SQLGetData(stmt, 5, SQL_C_CHAR, c.KasirID, sizeof(c.KasirID), NULL);
        SQLGetData(stmt, 6, SQL_C_CHAR, c.PelangganID, sizeof(c.PelangganID), NULL);
        SQLGetData(stmt, 7, SQL_C_CHAR, c.Qty, sizeof(c.Qty), NULL);
        SQLGetData(stmt, 8, SQL_C_CHAR, c.TanggalTransaksi, sizeof(c.TanggalTransaksi), NULL);
        SQLGetData(stmt, 9, SQL_C_CHAR, c.StatusPembayaran, sizeof(c.StatusPembayaran), NULL);
        SQLGetData(stmt, 10, SQL_C_CHAR, c.Total, sizeof(c.Total), NULL);
        SQLGetData(stmt, 11, SQL_C_CHAR, c.Uang, sizeof(c.Uang), NULL);
        SQLGetData(stmt, 12, SQL_C_CHAR, c.Kembalian, sizeof(c.Kembalian), NULL);
        SQLGetData(stmt, 13, SQL_C_CHAR, c.SalesNama, sizeof(c.SalesNama), NULL);
        SQLGetData(stmt, 14, SQL_C_CHAR, c.KasirNama, sizeof(c.KasirNama), NULL);

        out[n++] = c;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    *outCount = n;
    return true;
}

bool DbPenjualanMobil_Insert(void *dbcVoid,
                             const char *NoTransaksi,
                             const char *MobilID,
                             const char *SalesID,
                             const char *KasirID,
                             const char *PelangganID,
                             const char *Qty,
                             const char *Total,
                             const char *Uang)
{
    if (!dbcVoid)
        return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char noTrE[64], mobilE[64], salesE[64], kasirE[64], pelangganE[64];
    char qtyE[32], totalE[32], uangE[32];

    EscapeSql(NoTransaksi ? NoTransaksi : "", noTrE, sizeof(noTrE));
    EscapeSql(MobilID ? MobilID : "", mobilE, sizeof(mobilE));
    EscapeSql(SalesID ? SalesID : "", salesE, sizeof(salesE));
    EscapeSql(KasirID ? KasirID : "", kasirE, sizeof(kasirE));
    EscapeSql(PelangganID ? PelangganID : "", pelangganE, sizeof(pelangganE));
    EscapeSql(Qty ? Qty : "", qtyE, sizeof(qtyE));
    EscapeSql(Total ? Total : "", totalE, sizeof(totalE));
    EscapeSql(Uang ? Uang : "", uangE, sizeof(uangE));

    // Tidak kirim TanggalTransaksi (DEFAULT), StatusPembayaran & Kembalian (computed)
    char sql[1200];
    snprintf(sql, sizeof(sql),
             "INSERT INTO dbo.PenjualanMobil "
             "(NoTransaksi, MobilID, SalesID, KasirID, PelangganID, Qty, Total, Uang) "
             "VALUES ('%s','%s','%s','%s','%s',%s,%s,%s)",
             noTrE, mobilE, salesE, kasirE, pelangganE, qtyE, totalE, uangE);

    return ExecSQL(dbc, sql);
}

bool DbPenjualanMobil_CreateNoTransaksi(void *dbcVoid, char *outNo, int outSize)
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

    snprintf(outNo, (size_t)outSize, "%d", nextNo);
    return true;
}

bool DbPenjualanMobil_Update(void *dbcVoid,
                             const char *PenjualanMobilID,
                             const char *NoTransaksi,
                             const char *MobilID,
                             const char *SalesID,
                             const char *KasirID,
                             const char *PelangganID,
                             const char *Qty,
                             const char *Total,
                             const char *Uang)
{
    if (!dbcVoid || !PenjualanMobilID)
        return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char idE[32];
    EscapeSql(PenjualanMobilID, idE, sizeof(idE));

    char noTrE[64], mobilE[64], salesE[64], kasirE[64], pelangganE[64];
    char qtyE[32], totalE[32], uangE[32];

    EscapeSql(NoTransaksi ? NoTransaksi : "", noTrE, sizeof(noTrE));
    EscapeSql(MobilID ? MobilID : "", mobilE, sizeof(mobilE));
    EscapeSql(SalesID ? SalesID : "", salesE, sizeof(salesE));
    EscapeSql(KasirID ? KasirID : "", kasirE, sizeof(kasirE));
    EscapeSql(PelangganID ? PelangganID : "", pelangganE, sizeof(pelangganE));
    EscapeSql(Qty ? Qty : "", qtyE, sizeof(qtyE));
    EscapeSql(Total ? Total : "", totalE, sizeof(totalE));
    EscapeSql(Uang ? Uang : "", uangE, sizeof(uangE));

    // ✅ FIX: UPDATE (bukan INSERT)
    char sql[1400];
    snprintf(sql, sizeof(sql),
             "UPDATE dbo.PenjualanMobil "
             "SET NoTransaksi='%s', MobilID='%s', SalesID='%s', "
             "KasirID='%s', PelangganID='%s', Qty=%s, Total=%s, Uang=%s "
             "WHERE PenjualanMobilID='%s'",
             noTrE, mobilE, salesE, kasirE, pelangganE,
             qtyE, totalE, uangE, idE);

    return ExecSQL(dbc, sql);
}

bool DbPenjualanMobil_Delete(void *dbcVoid, const char *PenjualanMobilID)
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
