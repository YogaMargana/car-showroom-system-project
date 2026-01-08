#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "db_sales_report.h"

#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <string.h>
#include <stdio.h>

static void SafeCopy(char *dst, int dstSize, const char *src) {
    if (!dst || dstSize <= 0) return;
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, (size_t)dstSize - 1);
    dst[dstSize - 1] = '\0';
}

static SQLRETURN GetMoneyAsDouble(SQLHSTMT stmt, int col, double *outVal)
{
    if (!outVal) return SQL_ERROR;
    *outVal = 0.0;

    // Money bisa diambil sebagai double langsung
    SQLLEN ind = 0;
    double v = 0.0;
    SQLRETURN r = SQLGetData(stmt, col, SQL_C_DOUBLE, &v, 0, &ind);
    if (SQL_SUCCEEDED(r) && ind != SQL_NULL_DATA) {
        *outVal = v;
    }
    return r;
}

static const char *kSalesUnionQuery_NoPaging =
    "SELECT "
    "  CONVERT(VARCHAR(10), x.[Date], 23) AS [Date], "
    "  x.[ID], "
    "  x.[Type], "
    "  x.[Item], "
    "  x.[Customer], "
    "  x.[Employee], "
    "  x.[Status], "
    "  x.[Qty], "
    "  CAST(x.[Total] AS MONEY) AS [Total] "
    "FROM ( "
    "  -- CAR SALES "
    "  SELECT "
    "    pm.TanggalTransaksi AS [Date], "
    "    pm.PenjualanMobilID AS [ID], "
    "    'Car' AS [Type], "
    "    ISNULL(m.NamaMobil, '(Unknown Car)') AS [Item], "
    "    p.Nama AS [Customer], "
    "    k.Nama AS [Employee], "
    "    pm.StatusPembayaran AS [Status], "
    "    1 AS [Qty], "
    "    pm.Total AS [Total] "
    "  FROM dbo.PenjualanMobil pm "
    "  LEFT JOIN dbo.Mobil m ON m.ID = pm.MobilID "
    "  JOIN dbo.Pelanggan p ON p.PelangganID = pm.PelangganID "
    "  JOIN dbo.Karyawan  k ON k.KaryawanID  = pm.KaryawanID "
    ""
    "  UNION ALL "
    ""
    "  -- ACCESSORY SALES (1 row per detail item) "
    "  SELECT "
    "    pa.TanggalTransaksi AS [Date], "
    "    pa.PenjualanAksesorisID AS [ID], "
    "    'Accessory' AS [Type], "
    "    ISNULL(a.NamaAksesoris, '(Unknown Accessory)') AS [Item], "
    "    p2.Nama AS [Customer], "
    "    k2.Nama AS [Employee], "
    "    pa.StatusPembayaran AS [Status], "
    "    pad.Qty AS [Qty], "
    "    pad.Subtotal AS [Total] "
    "  FROM dbo.PenjualanAksesoris pa "
    "  JOIN dbo.PenjualanAksesorisDetail pad ON pad.PenjualanAksesorisID = pa.PenjualanAksesorisID "
    "  LEFT JOIN dbo.Aksesoris a ON a.AksesorisID = pad.AksesorisID "
    "  JOIN dbo.Pelanggan p2 ON p2.PelangganID = pa.PelangganID "
    "  JOIN dbo.Karyawan  k2 ON k2.KaryawanID  = pa.KaryawanID "
    ") x "
    "ORDER BY x.[Date] DESC, x.[ID] DESC;";

static const char *kSalesUnionQuery_Paging =
    "DECLARE @offset INT = ?; "
    "DECLARE @fetch  INT = ?; "
    "SELECT "
    "  CONVERT(VARCHAR(10), x.[Date], 23) AS [Date], "
    "  x.[ID], "
    "  x.[Type], "
    "  x.[Item], "
    "  x.[Customer], "
    "  x.[Employee], "
    "  x.[Status], "
    "  x.[Qty], "
    "  CAST(x.[Total] AS MONEY) AS [Total] "
    "FROM ( "
    "  SELECT "
    "    pm.TanggalTransaksi AS [Date], "
    "    pm.PenjualanMobilID AS [ID], "
    "    'Car' AS [Type], "
    "    ISNULL(m.NamaMobil, '(Unknown Car)') AS [Item], "
    "    p.Nama AS [Customer], "
    "    k.Nama AS [Employee], "
    "    pm.StatusPembayaran AS [Status], "
    "    1 AS [Qty], "
    "    pm.Total AS [Total] "
    "  FROM dbo.PenjualanMobil pm "
    "  LEFT JOIN dbo.Mobil m ON m.ID = pm.MobilID "
    "  JOIN dbo.Pelanggan p ON p.PelangganID = pm.PelangganID "
    "  JOIN dbo.Karyawan  k ON k.KaryawanID  = pm.KaryawanID "
    ""
    "  UNION ALL "
    ""
    "  SELECT "
    "    pa.TanggalTransaksi AS [Date], "
    "    pa.PenjualanAksesorisID AS [ID], "
    "    'Accessory' AS [Type], "
    "    ISNULL(a.NamaAksesoris, '(Unknown Accessory)') AS [Item], "
    "    p2.Nama AS [Customer], "
    "    k2.Nama AS [Employee], "
    "    pa.StatusPembayaran AS [Status], "
    "    pad.Qty AS [Qty], "
    "    pad.Subtotal AS [Total] "
    "  FROM dbo.PenjualanAksesoris pa "
    "  JOIN dbo.PenjualanAksesorisDetail pad ON pad.PenjualanAksesorisID = pa.PenjualanAksesorisID "
    "  LEFT JOIN dbo.Aksesoris a ON a.AksesorisID = pad.AksesorisID "
    "  JOIN dbo.Pelanggan p2 ON p2.PelangganID = pa.PelangganID "
    "  JOIN dbo.Karyawan  k2 ON k2.KaryawanID  = pa.KaryawanID "
    ") x "
    "ORDER BY x.[Date] DESC, x.[ID] DESC "
    "OFFSET @offset ROWS FETCH NEXT @fetch ROWS ONLY;";

bool DbSalesReport_LoadAll(void *dbcVoid, SalesReportRow *out, int outCap, int *outCount)
{
    if (outCount) *outCount = 0;
    if (!dbcVoid || !out || outCap <= 0) return false;

    SQLHDBC hdbc = (SQLHDBC)dbcVoid;
    SQLHSTMT stmt = NULL;

    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &stmt))) return false;

    // === query kamu yang sudah ada (JANGAN DIUBAH kalau sudah benar) ===
    const char *query =
        "SELECT ..."; // <- biarin query kamu yang sekarang

    SQLRETURN ret = SQLExecDirectA(stmt, (SQLCHAR*)query, SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    int count = 0;
    while (count < outCap) {
        ret = SQLFetch(stmt);
        if (ret == SQL_NO_DATA) break;
        if (!SQL_SUCCEEDED(ret)) break;

        SalesReportRow *r = &out[count];
        memset(r, 0, sizeof(*r));

        // ... isi r->Tanggal, r->TransaksiID, dll (kode kamu yang sekarang) ...

        count++;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    if (outCount) *outCount = count;
    return true;
}

int DbSalesReport_Count(void *dbcPtr)
{
    if (!dbcPtr) return -1;

    SQLHDBC hdbc = (SQLHDBC)dbcPtr;
    SQLHSTMT stmt = NULL;

    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &stmt))) return -1;

    const char *sql =
        "SELECT COUNT(*) "
        "FROM ( "
        "  SELECT pm.PenjualanMobilID AS ID "
        "  FROM dbo.PenjualanMobil pm "
        "  UNION ALL "
        "  SELECT pa.PenjualanAksesorisID AS ID "
        "  FROM dbo.PenjualanAksesoris pa "
        "  JOIN dbo.PenjualanAksesorisDetail pad ON pad.PenjualanAksesorisID = pa.PenjualanAksesorisID "
        ") x;";

    if (!SQL_SUCCEEDED(SQLExecDirectA(stmt, (SQLCHAR*)sql, SQL_NTS))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return -1;
    }

    int total = 0;
    if (SQLFetch(stmt) == SQL_SUCCESS) {
        SQLGetData(stmt, 1, SQL_C_SLONG, &total, 0, NULL);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return total;
}

int DbSalesReport_LoadPage(void *dbcPtr,
                           int page, int pageSize,
                           SalesReportRow *outRows, int maxRows)
{
    if (!dbcPtr || !outRows || maxRows <= 0) return -1;
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 10;

    int offset = (page - 1) * pageSize;

    SQLHDBC hdbc = (SQLHDBC)dbcPtr;
    SQLHSTMT stmt = NULL;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &stmt))) return -1;

    if (!SQL_SUCCEEDED(SQLPrepareA(stmt, (SQLCHAR*)kSalesUnionQuery_Paging, SQL_NTS))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return -1;
    }

    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &offset, 0, NULL);
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &pageSize, 0, NULL);

    if (!SQL_SUCCEEDED(SQLExecute(stmt))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return -1;
    }

    int count = 0;
    while (count < maxRows) {
        SQLRETURN ret = SQLFetch(stmt);
        if (ret == SQL_NO_DATA) break;
        if (!SQL_SUCCEEDED(ret)) break;

        SalesReportRow *r = &outRows[count];
        memset(r, 0, sizeof(*r));

        SQLLEN ind = 0;
        char buf[256];

        memset(buf, 0, sizeof(buf)); SQLGetData(stmt, 1, SQL_C_CHAR, buf, sizeof(buf), &ind); SafeCopy(r->Tanggal, (int)sizeof(r->Tanggal), buf);
        memset(buf, 0, sizeof(buf)); SQLGetData(stmt, 2, SQL_C_CHAR, buf, sizeof(buf), &ind); SafeCopy(r->TransaksiID, (int)sizeof(r->TransaksiID), buf);
        memset(buf, 0, sizeof(buf)); SQLGetData(stmt, 3, SQL_C_CHAR, buf, sizeof(buf), &ind); SafeCopy(r->Type, (int)sizeof(r->Type), buf);
        memset(buf, 0, sizeof(buf)); SQLGetData(stmt, 4, SQL_C_CHAR, buf, sizeof(buf), &ind); SafeCopy(r->Item, (int)sizeof(r->Item), buf);
        memset(buf, 0, sizeof(buf)); SQLGetData(stmt, 5, SQL_C_CHAR, buf, sizeof(buf), &ind); SafeCopy(r->Customer, (int)sizeof(r->Customer), buf);
        memset(buf, 0, sizeof(buf)); SQLGetData(stmt, 6, SQL_C_CHAR, buf, sizeof(buf), &ind); SafeCopy(r->Employee, (int)sizeof(r->Employee), buf);
        memset(buf, 0, sizeof(buf)); SQLGetData(stmt, 7, SQL_C_CHAR, buf, sizeof(buf), &ind); SafeCopy(r->Status, (int)sizeof(r->Status), buf);

        int qty = 0;
        SQLGetData(stmt, 8, SQL_C_SLONG, &qty, 0, &ind);
        r->Qty = (ind == SQL_NULL_DATA) ? 0 : qty;

        GetMoneyAsDouble(stmt, 9, &r->Total);

        count++;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return count;
}