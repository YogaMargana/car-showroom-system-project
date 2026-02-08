#include "db_testdrive_report.h"

#include <string.h>
#include <stdio.h>
#include <windows.h>
#include <sql.h>
#include <sqlext.h>

static void SafeCopy(char *dst, int cap, const char *src)
{
    if (!dst || cap <= 0) return;
    if (!src) src = "";
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}

static bool ExecQuery(SQLHDBC dbc, SQLHSTMT *outStmt, const char *sql)
{
    SQLHSTMT st = NULL;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &st))) return false;

    // NOTE: SQLExecDirect can return SQL_SUCCESS_WITH_INFO (still success).
    if (!SQL_SUCCEEDED(SQLExecDirect(st, (SQLCHAR*)sql, SQL_NTS))) {
        SQLFreeHandle(SQL_HANDLE_STMT, st);
        return false;
    }
    *outStmt = st;
    return true;
}

static void CloseStmt(SQLHSTMT st)
{
    if (st) SQLFreeHandle(SQL_HANDLE_STMT, st);
}

bool DbTestDriveReport_LoadAll(void *dbcVoid, TestDriveRow *out, int outCap, int *outCount)
{
    if (outCount) *outCount = 0;
    if (!dbcVoid || !out || outCap <= 0) return false;

    SQLHDBC dbc = (SQLHDBC)dbcVoid;
    SQLHSTMT st = NULL;

    const char *sql =
        "SELECT "
        "  CONVERT(varchar(10), td.Tanggal_TestDrive, 23) AS Tanggal, "
        "  td.TestDriveID, "
        "  ISNULL(m.NamaMobil,'(Unknown Car)') AS Mobil, "
        "  ISNULL(p.Nama,'(Unknown Customer)') AS Customer, "
        "  ISNULL(k.Nama,'(Unknown Sales)') AS Sales, "
        "  td.Status "
        "FROM dbo.TestDrive td "
        "LEFT JOIN dbo.Mobil m ON m.MobilID = td.MobilID "
        "LEFT JOIN dbo.Pelanggan p ON p.PelangganID = td.PelangganID "
        "LEFT JOIN dbo.Karyawan k ON k.KaryawanID = td.KaryawanID "
        "ORDER BY td.Tanggal_TestDrive DESC, td.TestDriveID DESC;";

    if (!ExecQuery(dbc, &st, sql)) return false;

    int n = 0;
    while (SQL_SUCCEEDED(SQLFetch(st)) && n < outCap) {
        char c1[32]={0}, c2[32]={0}, c3[128]={0}, c4[128]={0}, c5[128]={0}, c6[32]={0};
        SQLLEN ind=0;

        SQLGetData(st, 1, SQL_C_CHAR, c1, sizeof(c1), &ind);
        SQLGetData(st, 2, SQL_C_CHAR, c2, sizeof(c2), &ind);
        SQLGetData(st, 3, SQL_C_CHAR, c3, sizeof(c3), &ind);
        SQLGetData(st, 4, SQL_C_CHAR, c4, sizeof(c4), &ind);
        SQLGetData(st, 5, SQL_C_CHAR, c5, sizeof(c5), &ind);
        SQLGetData(st, 6, SQL_C_CHAR, c6, sizeof(c6), &ind);

        SafeCopy(out[n].Tanggal,     (int)sizeof(out[n].Tanggal),     c1);
        SafeCopy(out[n].TestDriveID, (int)sizeof(out[n].TestDriveID), c2);
        SafeCopy(out[n].Mobil,       (int)sizeof(out[n].Mobil),       c3);
        SafeCopy(out[n].Customer,    (int)sizeof(out[n].Customer),    c4);
        SafeCopy(out[n].Sales,       (int)sizeof(out[n].Sales),       c5);
        SafeCopy(out[n].Status,      (int)sizeof(out[n].Status),      c6);
        n++;
    }

    CloseStmt(st);
    if (outCount) *outCount = n;
    return true;
}

// ============================================================
// INSIGHTS (CALL STORED PROCEDURE) - TEST DRIVE
// ============================================================

static void SafeCopyTD(char *dst, int cap, const char *src)
{
    if (!dst || cap <= 0) return;
    if (!src) src = "";
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}

static bool OdbcPrep(SQLHDBC dbc, SQLHSTMT *outStmt, const char *sql)
{
    if (!dbc || !outStmt || !sql) return false;

    SQLHSTMT st = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &st))) return false;

    if (!SQL_SUCCEEDED(SQLPrepare(st, (SQLCHAR*)sql, SQL_NTS))) {
        SQLFreeHandle(SQL_HANDLE_STMT, st);
        return false;
    }

    *outStmt = st;
    return true;
}

static void OdbcClose(SQLHSTMT st)
{
    if (st) SQLFreeHandle(SQL_HANDLE_STMT, st);
}

static int StrEqI_Ansi(const char *a, const char *b)
{
    if (!a) a = "";
    if (!b) b = "";
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
        a++; b++;
    }
    return (*a == '\0' && *b == '\0');
}

static int StrEqI_ColName(const char *col, const char *expect)
{
    // Normalize some common aliases
    return StrEqI_Ansi(col, expect);
}

bool DbTestDriveInsight_LoadSummary(void *dbcVoid, TestDriveInsightSummary *out,
                                   const char *fromYmd, const char *toYmd)
{
    if (!dbcVoid || !out) return false;

    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char a[64], b[64], sql[512];
    if (fromYmd && fromYmd[0]) snprintf(a, sizeof(a), "CONVERT(date,'%s')", fromYmd);
    else strcpy(a, "NULL");

    if (toYmd && toYmd[0]) snprintf(b, sizeof(b), "CONVERT(date,'%s')", toYmd);
    else strcpy(b, "NULL");

    snprintf(sql, sizeof(sql),
        "EXEC dbo.usp_TestDrive_ReportSummary @FromDate=%s, @ToDate=%s;",
        a, b
    );

    SQLHSTMT st = NULL;
    if (!ExecQuery(dbc, &st, sql)) return false;

    // Bajigur / 6Production kadang beda urutan kolom (ada yang pakai TotalOngoing).
    // Biar aman: map berdasarkan NAMA kolom hasil SP, bukan posisi.
    SQLSMALLINT colCount = 0;
    SQLNumResultCols(st, &colCount);

    int idxFiltered  = -1;
    int idxScheduled = -1;
    int idxFinished  = -1;
    int idxCanceled  = -1; // kalau SP masih pakai TotalOngoing, kita treat sebagai Dibatalkan sesuai requirement
    int idxConverted = -1;
    int idxRate      = -1;

    for (SQLUSMALLINT i = 1; i <= (SQLUSMALLINT)colCount; i++) {
        SQLCHAR colName[64] = {0};
        SQLSMALLINT nameLen = 0;
        SQLDescribeCol(st, i, colName, (SQLSMALLINT)sizeof(colName), &nameLen, NULL, NULL, NULL, NULL);

        const char *cn = (const char*)colName;

        if (StrEqI_ColName(cn, "TotalFiltered")) idxFiltered = (int)i;
        else if (StrEqI_ColName(cn, "TotalScheduled")) idxScheduled = (int)i;
        else if (StrEqI_ColName(cn, "TotalFinished")) idxFinished = (int)i;
        else if (StrEqI_ColName(cn, "TotalCanceled")) idxCanceled = (int)i;
        else if (StrEqI_ColName(cn, "TotalOngoing")) idxCanceled = (int)i; // IMPORTANT: ongoing -> dibatalkan
        else if (StrEqI_ColName(cn, "TotalConverted")) idxConverted = (int)i;
        else if (StrEqI_ColName(cn, "ConversionRatePct")) idxRate = (int)i;
    }

    // fallback kalau nama kolom ga ketemu (positional)
    if (idxFiltered  < 0) idxFiltered  = 1;
    if (idxScheduled < 0) idxScheduled = 2;
    if (idxRate      < 0) idxRate      = 6;

    // kalau SP masih output: TotalFiltered, TotalScheduled, TotalOngoing, TotalFinished, TotalConverted, ConversionRatePct
    // maka finished ada di kolom 4.
    if (idxFinished < 0 && colCount >= 4) idxFinished = 4;

    // kalau SP output: ... TotalCanceled ...
    // maka canceled biasanya kolom 4, finished kolom 3.
    if (idxCanceled < 0 && colCount >= 4) {
        // best guess: col3 = finished, col4 = canceled
        idxCanceled = 4;
        if (idxFinished == 4) idxFinished = 3;
    }

    if (idxConverted < 0 && colCount >= 5) idxConverted = 5;

    SQLLEN ind = 0;
    int totalFiltered = 0, scheduled = 0, finished = 0, canceled = 0, converted = 0;
    double rate = 0.0;

    if (!SQL_SUCCEEDED(SQLFetch(st))) {
        CloseStmt(st);
        return false;
    }

    // NOTE: SQLGetData for integers: buffer length can be 0 for fixed-size types.
    if (idxFiltered > 0)  SQLGetData(st, (SQLUSMALLINT)idxFiltered,  SQL_C_LONG,   &totalFiltered, 0, &ind);
    if (idxScheduled > 0) SQLGetData(st, (SQLUSMALLINT)idxScheduled, SQL_C_LONG,   &scheduled,     0, &ind);
    if (idxFinished > 0)  SQLGetData(st, (SQLUSMALLINT)idxFinished,  SQL_C_LONG,   &finished,      0, &ind);
    if (idxCanceled > 0)  SQLGetData(st, (SQLUSMALLINT)idxCanceled,  SQL_C_LONG,   &canceled,      0, &ind);
    if (idxConverted > 0) SQLGetData(st, (SQLUSMALLINT)idxConverted, SQL_C_LONG,   &converted,     0, &ind);
    if (idxRate > 0)      SQLGetData(st, (SQLUSMALLINT)idxRate,      SQL_C_DOUBLE, &rate,          0, &ind);

    CloseStmt(st);

    memset(out, 0, sizeof(*out));
    out->totalScheduled    = scheduled;
    out->totalFinished     = finished;
    out->totalCanceled     = canceled;
    out->totalConverted    = converted;
    out->conversionRatePct = rate;

    return true;
}

int DbTestDriveInsight_LoadMostTestDrivenCars(void *dbcVoid, TestDriveMostCarRow *out, int cap,
                                             const char *fromYmd, const char *toYmd,
                                             const char *search, int topN)
{
    if (!dbcVoid || !out || cap <= 0) return -1;
    if (topN <= 0) topN = cap;

    SQLHDBC dbc = (SQLHDBC)dbcVoid;
    SQLHSTMT st = SQL_NULL_HSTMT;

    // SP: dbo.usp_TestDrive_MostTestDrivenCar(@FromDate, @ToDate, @Search, @TopN)
    const char *sqlCall = "{CALL dbo.usp_TestDrive_MostTestDrivenCar(?,?,?,?)}";
    if (!OdbcPrep(dbc, &st, sqlCall)) return -1;

    SQLLEN ind1 = SQL_NTS, ind2 = SQL_NTS, ind3 = SQL_NTS, ind4 = 0;

    if (!fromYmd || fromYmd[0] == '\0') ind1 = SQL_NULL_DATA;
    if (!toYmd   || toYmd[0]   == '\0') ind2 = SQL_NULL_DATA;
    if (!search  || search[0]  == '\0') ind3 = SQL_NULL_DATA;

    SQLINTEGER top = (SQLINTEGER)topN;

    if (!SQL_SUCCEEDED(SQLBindParameter(st, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 10, 0,
                                        (SQLPOINTER)fromYmd, 0, &ind1))) { OdbcClose(st); return -1; }

    if (!SQL_SUCCEEDED(SQLBindParameter(st, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 10, 0,
                                        (SQLPOINTER)toYmd, 0, &ind2))) { OdbcClose(st); return -1; }

    if (!SQL_SUCCEEDED(SQLBindParameter(st, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 100, 0,
                                        (SQLPOINTER)search, 0, &ind3))) { OdbcClose(st); return -1; }

    if (!SQL_SUCCEEDED(SQLBindParameter(st, 4, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0,
                                        &top, 0, &ind4))) { OdbcClose(st); return -1; }

    if (!SQL_SUCCEEDED(SQLExecute(st))) { OdbcClose(st); return -1; }

    // Result: MobilID, NamaMobil, TotalTestDrive, FinishedCount
    char mobilID[16] = {0};
    char nama[128] = {0};
    SQLINTEGER total = 0, fin = 0;
    SQLLEN cb1 = 0, cb2 = 0, cb3 = 0, cb4 = 0;

    SQLBindCol(st, 1, SQL_C_CHAR, mobilID, sizeof(mobilID), &cb1);
    SQLBindCol(st, 2, SQL_C_CHAR, nama,    sizeof(nama),    &cb2);
    SQLBindCol(st, 3, SQL_C_LONG, &total,  0,               &cb3);
    SQLBindCol(st, 4, SQL_C_LONG, &fin,    0,               &cb4);

    int n = 0;
    SQLRETURN fr;
    while ((fr = SQLFetch(st)) != SQL_NO_DATA && SQL_SUCCEEDED(fr) && n < cap) {
        SafeCopyTD(out[n].MobilID, sizeof(out[n].MobilID), (cb1 == SQL_NULL_DATA) ? "" : mobilID);
        SafeCopyTD(out[n].Mobil,   sizeof(out[n].Mobil),   (cb2 == SQL_NULL_DATA) ? "" : nama);
        out[n].totalTestDrive = (cb3 == SQL_NULL_DATA) ? 0 : (int)total;
        out[n].finishedCount  = (cb4 == SQL_NULL_DATA) ? 0 : (int)fin;
        n++;
    }

    OdbcClose(st);
    return n;
}

int DbTestDriveInsight_LoadSalesPerformance(void *dbcVoid, TestDriveSalesPerfRow *out, int cap,
                                           const char *fromYmd, const char *toYmd, int topN)
{
    if (!dbcVoid || !out || cap <= 0) return -1;
    if (topN <= 0) topN = cap;

    SQLHDBC dbc = (SQLHDBC)dbcVoid;
    SQLHSTMT st = SQL_NULL_HSTMT;

    // SP: dbo.usp_TestDrive_SalesPerformance(@FromDate, @ToDate, @TopN)
    const char *sqlCall = "{CALL dbo.usp_TestDrive_SalesPerformance(?,?,?)}";
    if (!OdbcPrep(dbc, &st, sqlCall)) return -1;

    SQLLEN ind1 = SQL_NTS, ind2 = SQL_NTS, ind3 = 0;
    if (!fromYmd || fromYmd[0] == '\0') ind1 = SQL_NULL_DATA;
    if (!toYmd   || toYmd[0]   == '\0') ind2 = SQL_NULL_DATA;

    SQLINTEGER top = (SQLINTEGER)topN;

    if (!SQL_SUCCEEDED(SQLBindParameter(st, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 10, 0,
                                        (SQLPOINTER)fromYmd, 0, &ind1))) { OdbcClose(st); return -1; }

    if (!SQL_SUCCEEDED(SQLBindParameter(st, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 10, 0,
                                        (SQLPOINTER)toYmd, 0, &ind2))) { OdbcClose(st); return -1; }

    if (!SQL_SUCCEEDED(SQLBindParameter(st, 3, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0,
                                        &top, 0, &ind3))) { OdbcClose(st); return -1; }

    if (!SQL_SUCCEEDED(SQLExecute(st))) { OdbcClose(st); return -1; }

    // Result columns (sesuai SSMS):
    // 1 SalesID
    // 2 SalesName
    // 3 TotalTestDrive
    // 4 FinishedTestDrive
    // 5 CanceledTestDrive
    // 6 ConvertedCount
    // 7 ConversionRatePct
    // 8 RevenueFromConverted

    char salesID[16] = {0};
    char salesName[96] = {0};

    SQLINTEGER totalTD = 0;
    SQLINTEGER finished = 0;
    SQLINTEGER canceled = 0;
    SQLINTEGER converted = 0;
    double pct = 0.0;
    double rev = 0.0;

    SQLLEN cb1 = 0, cb2 = 0, cb3 = 0, cb4 = 0, cb5 = 0, cb6 = 0, cb7 = 0, cb8 = 0;

    SQLBindCol(st, 1, SQL_C_CHAR,   salesID,   sizeof(salesID),   &cb1);
    SQLBindCol(st, 2, SQL_C_CHAR,   salesName, sizeof(salesName), &cb2);
    SQLBindCol(st, 3, SQL_C_LONG,   &totalTD,  0,                 &cb3);
    SQLBindCol(st, 4, SQL_C_LONG,   &finished, 0,                 &cb4);
    SQLBindCol(st, 5, SQL_C_LONG,   &canceled, 0,                 &cb5);
    SQLBindCol(st, 6, SQL_C_LONG,   &converted,0,                 &cb6);
    SQLBindCol(st, 7, SQL_C_DOUBLE, &pct,      0,                 &cb7);
    SQLBindCol(st, 8, SQL_C_DOUBLE, &rev,      0,                 &cb8);

    int n = 0;
    SQLRETURN fr;
    while ((fr = SQLFetch(st)) != SQL_NO_DATA && SQL_SUCCEEDED(fr) && n < cap) {
        SafeCopyTD(out[n].SalesID,   sizeof(out[n].SalesID),   (cb1 == SQL_NULL_DATA) ? "" : salesID);
        SafeCopyTD(out[n].SalesName, sizeof(out[n].SalesName), (cb2 == SQL_NULL_DATA) ? "" : salesName);

        out[n].totalTestDrive        = (cb3 == SQL_NULL_DATA) ? 0 : (int)totalTD;
        out[n].finishedTestDrive     = (cb4 == SQL_NULL_DATA) ? 0 : (int)finished;
        out[n].canceledTestDrive     = (cb5 == SQL_NULL_DATA) ? 0 : (int)canceled;
        out[n].convertedCount        = (cb6 == SQL_NULL_DATA) ? 0 : (int)converted;
        out[n].conversionRatePct     = (cb7 == SQL_NULL_DATA) ? 0.0 : pct;
        out[n].revenueFromConverted  = (cb8 == SQL_NULL_DATA) ? 0.0 : rev;

        n++;
    }

    OdbcClose(st);
    return n;
}
