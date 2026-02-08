#include "db_PenjualanMobil.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <sql.h>
#include <sqlext.h>

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
            else break;
        }
        else dst[j++] = src[i];
    }
    dst[j] = '\0';
}

static bool ExecSQL(SQLHDBC dbc, const char *sql)
{
    if (!dbc || !sql) return false;

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt)))
        return false;

    SQLRETURN r = SQLExecDirectA(stmt, (SQLCHAR*)sql, SQL_NTS);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return SQL_SUCCEEDED(r);
}

static void CopyStr(char *dst, int dstSize, const char *src)
{
    if (!dst || dstSize <= 0) return;
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, (size_t)dstSize - 1);
    dst[dstSize - 1] = '\0';
}

static void OdbcPrintDiag(const char *tag, SQLSMALLINT handleType, SQLHANDLE handle)
{
    SQLCHAR state[6] = {0};
    SQLINTEGER nativeErr = 0;
    SQLCHAR msg[512] = {0};
    SQLSMALLINT msgLen = 0;

    fprintf(stderr, "[ODBC] %s\n", tag ? tag : "(null)");

    for (SQLSMALLINT i = 1;; i++)
    {
        SQLRETURN rc = SQLGetDiagRecA(handleType, handle, i, state, &nativeErr, msg, (SQLSMALLINT)sizeof(msg), &msgLen);
        if (rc == SQL_NO_DATA) break;
        if (SQL_SUCCEEDED(rc))
        {
            fprintf(stderr, "  %s (%ld): %s\n", state, (long)nativeErr, msg);
        }
        else break;
    }
}

bool DbPenjualanMobil_LoadAll(void *dbcVoid,
                              Penjualanmobildata *out,
                              int outCap,
                              int *outCount)
{
    if (outCount) *outCount = 0;
    if (!dbcVoid || !out || outCap <= 0) return false;

    SQLHDBC dbc = (SQLHDBC)dbcVoid;
    SQLHSTMT stmt = SQL_NULL_HSTMT;

    SQLRETURN rc = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    if (!SQL_SUCCEEDED(rc))
    {
        OdbcPrintDiag("SQLAllocHandle(STMT) failed", SQL_HANDLE_DBC, dbc);
        return false;
    }

    const char *sql =
    "SELECT "
    " pm.PenjualanMobilID, "
    " pm.NoTransaksi, "
    " CONVERT(VARCHAR(10), pm.TanggalTransaksi, 120) AS TanggalTransaksi, "
    " d.MobilID, "
    " m.NamaMobil AS NamaMobil, "
    " pm.PelangganID, "
    " pg.Nama AS PelangganNama, "
    " ks.Nama AS KasirNama, "
    " sl.Nama AS SalesNama, "
    " CAST(d.Qty AS VARCHAR(10)) AS Qty, "
    " CAST(pm.Total AS VARCHAR(32)) AS Total, "
    " CAST(pm.Uang  AS VARCHAR(32)) AS Uang, "
    " CAST(pm.Kembalian AS VARCHAR(32)) AS Kembalian, "
    " pm.StatusPembayaran "
    "FROM dbo.PenjualanMobil pm "
    "JOIN dbo.PenjualanMobilDetail d ON d.PenjualanMobilID = pm.PenjualanMobilID "
    "JOIN dbo.Mobil m ON m.MobilID = d.MobilID "
    "JOIN dbo.Pelanggan pg ON pg.PelangganID = pm.PelangganID "
    "JOIN dbo.Karyawan ks ON ks.KaryawanID = pm.KasirID "
    "JOIN dbo.Karyawan sl ON sl.KaryawanID = pm.SalesID "
    "ORDER BY pm.TanggalTransaksi DESC, pm.PenjualanMobilID DESC, d.MobilID;";


    rc = SQLExecDirectA(stmt, (SQLCHAR*)sql, SQL_NTS);
    if (!SQL_SUCCEEDED(rc))
    {
        OdbcPrintDiag("SQLExecDirect failed (LoadAll)", SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    // Buffer untuk bind kolom (sesuaikan ukuran aman)
    char PenjualanMobilID[64] = {0};
    char NoTransaksi[64] = {0};
    char TanggalTransaksi[32] = {0};
    char MobilID[32] = {0};
    char NamaMobil[128] = {0};
    char PelangganID[32] = {0};
    char PelangganNama[128] = {0};
    char KasirNama[128] = {0};
    char SalesNama[128] = {0};
    char JumlahProduk[32] = {0};
    char Total[64] = {0};
    char Uang[64] = {0};
    char Kembalian[64] = {0};
    char StatusPembayaran[64] = {0};

    SQLLEN ind = 0;

    // Urutan bind HARUS sama dengan urutan SELECT (1..14)
    SQLBindCol(stmt, 1,  SQL_C_CHAR, PenjualanMobilID, sizeof(PenjualanMobilID), &ind);
    SQLBindCol(stmt, 2,  SQL_C_CHAR, NoTransaksi,      sizeof(NoTransaksi),      &ind);
    SQLBindCol(stmt, 3,  SQL_C_CHAR, TanggalTransaksi, sizeof(TanggalTransaksi), &ind);
    SQLBindCol(stmt, 4,  SQL_C_CHAR, MobilID,          sizeof(MobilID),          &ind);
    SQLBindCol(stmt, 5,  SQL_C_CHAR, NamaMobil,        sizeof(NamaMobil),        &ind);
    SQLBindCol(stmt, 6,  SQL_C_CHAR, PelangganID,      sizeof(PelangganID),      &ind);
    SQLBindCol(stmt, 7,  SQL_C_CHAR, PelangganNama,    sizeof(PelangganNama),    &ind);
    SQLBindCol(stmt, 8,  SQL_C_CHAR, KasirNama,        sizeof(KasirNama),        &ind);
    SQLBindCol(stmt, 9,  SQL_C_CHAR, SalesNama,        sizeof(SalesNama),        &ind);
    SQLBindCol(stmt, 10, SQL_C_CHAR, JumlahProduk,     sizeof(JumlahProduk),     &ind);
    SQLBindCol(stmt, 11, SQL_C_CHAR, Total,            sizeof(Total),            &ind);
    SQLBindCol(stmt, 12, SQL_C_CHAR, Uang,             sizeof(Uang),             &ind);
    SQLBindCol(stmt, 13, SQL_C_CHAR, Kembalian,        sizeof(Kembalian),        &ind);
    SQLBindCol(stmt, 14, SQL_C_CHAR, StatusPembayaran, sizeof(StatusPembayaran), &ind);

    int n = 0;
    while (n < outCap)
    {
        rc = SQLFetch(stmt);
        if (rc == SQL_NO_DATA) break;

        if (!SQL_SUCCEEDED(rc))
        {
            OdbcPrintDiag("SQLFetch failed (LoadAll)", SQL_HANDLE_STMT, stmt);
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            return false;
        }

        // Isi struct output
        Penjualanmobildata *row = &out[n];
        memset(row, 0, sizeof(*row));

        CopyStr(row->PenjualanMobilID, sizeof(row->PenjualanMobilID), PenjualanMobilID);
        CopyStr(row->NoTransaksi,      sizeof(row->NoTransaksi),      NoTransaksi);
        CopyStr(row->TanggalTransaksi, sizeof(row->TanggalTransaksi), TanggalTransaksi);

        CopyStr(row->MobilID,          sizeof(row->MobilID),          MobilID);
        CopyStr(row->NamaMobil,        sizeof(row->NamaMobil),        NamaMobil);

        CopyStr(row->PelangganID,      sizeof(row->PelangganID),      PelangganID);
        CopyStr(row->PelangganNama,    sizeof(row->PelangganNama),    PelangganNama);

        CopyStr(row->KasirNama,        sizeof(row->KasirNama),        KasirNama);
        CopyStr(row->SalesNama,        sizeof(row->SalesNama),        SalesNama);

        CopyStr(row->JumlahProduk,     sizeof(row->JumlahProduk),     JumlahProduk);
        CopyStr(row->Total,            sizeof(row->Total),            Total);
        CopyStr(row->Uang,             sizeof(row->Uang),             Uang);
        CopyStr(row->Kembalian,        sizeof(row->Kembalian),        Kembalian);
        CopyStr(row->StatusPembayaran, sizeof(row->StatusPembayaran), StatusPembayaran);

        n++;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    if (outCount) *outCount = n;
    return true;
}

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
    if (!dbcVoid || !MobilID || !JumlahProduk) return false;

    long long total = Total ? strtoll(Total, NULL, 10) : 0;
    long long uang  = Uang ? strtoll(Uang, NULL, 10) : 0;
    long long qty   = JumlahProduk ? strtoll(JumlahProduk, NULL, 10) : 0;
    if (qty <= 0) return false;

    PenjualanMobilItem item;
    item.MobilID = MobilID;
    item.Qty     = (int)qty;
    item.Harga   = (total > 0) ? (total / qty) : 0;

    return DbPenjualanMobil_InsertCart(dbcVoid,
                                       NoTransaksi,
                                       KasirID,
                                       SalesID,
                                       PelangganID,
                                       total,
                                       uang,
                                       &item,
                                       1);
}

bool DbPenjualanMobil_InsertCart(void *dbcVoid,
                                 const char *NoTransaksi,
                                 const char *KasirID,
                                 const char *SalesID,
                                 const char *PelangganID,
                                 long long Total,
                                 long long Uang,
                                 const PenjualanMobilItem *items,
                                 int itemCount)
{
    if (!dbcVoid || !items || itemCount <= 0) return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char noTrE[64], kasirE[64], salesE[64], pelangganE[64];
    EscapeSql(NoTransaksi ? NoTransaksi : "", noTrE, sizeof(noTrE));
    EscapeSql(KasirID ? KasirID : "", kasirE, sizeof(kasirE));
    EscapeSql(SalesID ? SalesID : "", salesE, sizeof(salesE));
    EscapeSql(PelangganID ? PelangganID : "", pelangganE, sizeof(pelangganE));

    char sql[32768];
    int w = snprintf(sql, sizeof(sql),
                     "DECLARE @newId TABLE (PenjualanMobilID VARCHAR(7)); "
                     "INSERT INTO dbo.PenjualanMobil (NoTransaksi, SalesID, KasirID, PelangganID, StatusPembayaran, Total, Uang) "
                     "OUTPUT inserted.PenjualanMobilID INTO @newId(PenjualanMobilID) "
                     "VALUES ('%s','%s','%s','%s','Berhasil',%lld,%lld); "
                     "INSERT INTO dbo.PenjualanMobilDetail (PenjualanMobilID, MobilID, Qty, Harga) ",
                     noTrE, salesE, kasirE, pelangganE, Total, Uang);

    if (w < 0 || w >= (int)sizeof(sql)) return false;

    bool first = true;
    for (int i = 0; i < itemCount; i++)
    {
        if (!items[i].MobilID || items[i].Qty <= 0) continue;

        char mobilE[64];
        EscapeSql(items[i].MobilID, mobilE, sizeof(mobilE));

        int wrote;
        if (first)
        {
            wrote = snprintf(sql + w, sizeof(sql) - (size_t)w,
                             "SELECT PenjualanMobilID, '%s', %d, %lld FROM @newId ",
                             mobilE, items[i].Qty, items[i].Harga);
            first = false;
        }
        else
        {
            wrote = snprintf(sql + w, sizeof(sql) - (size_t)w,
                             "UNION ALL SELECT PenjualanMobilID, '%s', %d, %lld FROM @newId ",
                             mobilE, items[i].Qty, items[i].Harga);
        }

        if (wrote < 0 || wrote >= (int)sizeof(sql) - w) return false;
        w += wrote;
    }

    if (first) return false;
    if (w <= 0 || w >= (int)sizeof(sql)) return false;
    return ExecSQL(dbc, sql);
}

bool DbPenjualanMobil_CreateNoTransaksi(void *dbcVoid,
                                        char *outNo,
                                        int outSize)
{
    if (!dbcVoid || !outNo || outSize <= 0) return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    const char *sql = "SELECT NEXT VALUE FOR dbo.SeqNoTransaksi";
    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt)))
        return false;

    if (!SQL_SUCCEEDED(SQLExecDirectA(stmt, (SQLCHAR*)sql, SQL_NTS)))
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

bool DbPenjualanMobil_Update(void *dbcVoid,
                             const char *PenjualanMobilID,
                             const char *NoTransaksi,
                             const char *MobilID,
                             const char *KasirID,
                             const char *SalesID,
                             const char *PelangganID,
                             const char *JumlahProduk,
                             const char *Total,
                             const char *Uang)
{
    if (!dbcVoid || !PenjualanMobilID) return false;
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

    (void)MobilID;
    (void)JumlahProduk;

    char sql[900];
    snprintf(sql, sizeof(sql),
             "UPDATE dbo.PenjualanMobil "
             "SET NoTransaksi='%s', KasirID='%s', SalesID='%s', PelangganID='%s', Total=%s, Uang=%s "
             "WHERE PenjualanMobilID='%s'",
             noTrE, kasirE, salesE, pelangganE, totalE, uangE, idE);

    return ExecSQL(dbc, sql);
}

bool DbPenjualanMobil_Delete(void *dbcVoid,
                             const char *PenjualanMobilID)
{
    if (!dbcVoid || !PenjualanMobilID) return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char idE[32];
    EscapeSql(PenjualanMobilID, idE, sizeof(idE));

    char sql[256];
    snprintf(sql, sizeof(sql),
             "DELETE FROM dbo.PenjualanMobil WHERE PenjualanMobilID='%s'",
             idE);

    return ExecSQL(dbc, sql);
}
