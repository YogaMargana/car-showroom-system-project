#include "db_testdrive_report.h"
#include <stdio.h>
#include <string.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sql.h>
#include <sqlext.h>

// ===== helper kecil =====
static void SafeCopy(char *dst, int cap, const char *src) {
    if (!dst || cap <= 0) return;
    if (!src) src = "";
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}

static bool ExecQuery(SQLHDBC dbc, SQLHSTMT *outStmt, const char *sql) {
    SQLHSTMT st = NULL;
    if (SQLAllocHandle(SQL_HANDLE_STMT, dbc, &st) != SQL_SUCCESS) return false;
    if (SQLExecDirect(st, (SQLCHAR*)sql, SQL_NTS) != SQL_SUCCESS) {
        SQLFreeHandle(SQL_HANDLE_STMT, st);
        return false;
    }
    *outStmt = st;
    return true;
}

static void CloseStmt(SQLHSTMT st) {
    if (st) SQLFreeHandle(SQL_HANDLE_STMT, st);
}

// =========================
// Page 1: LoadAll History
// =========================
// Kamu bisa ganti query ini jadi SP kalau mau.
// Ini version langsung SELECT biar gampang.
bool DbTestDriveReport_LoadAll(void *dbcVoid, TestDriveRow *out, int outCap, int *outCount)
{
    if (outCount) *outCount = 0;
    if (!dbcVoid || !out || outCap <= 0) return false;

    SQLHDBC dbc = (SQLHDBC)dbcVoid;
    SQLHSTMT st = NULL;

    const char *sql =
        "SELECT "
        "  CONVERT(varchar(10), td.TanggalTestDrive, 23) AS Tanggal, "
        "  td.TestDriveID, "
        "  ISNULL(m.NamaMobil,'(Unknown Car)') AS Mobil, "
        "  ISNULL(p.Nama,'(Unknown Customer)') AS Customer, "
        "  ISNULL(k.Nama,'(Unknown Sales)') AS Sales, "
        "  td.Status "
        "FROM dbo.TestDrive td "
        "LEFT JOIN dbo.Mobil m ON m.MobilID = td.MobilID "
        "LEFT JOIN dbo.Pelanggan p ON p.PelangganID = td.PelangganID "
        "LEFT JOIN dbo.Karyawan k ON k.KaryawanID = td.SalesID "
        "ORDER BY td.TanggalTestDrive DESC, td.TestDriveID DESC;";

    if (!ExecQuery(dbc, &st, sql)) return false;

    // columns: 1 Tanggal, 2 TestDriveID, 3 Mobil, 4 Customer, 5 Sales, 6 Status
    int n = 0;
    while (SQLFetch(st) == SQL_SUCCESS && n < outCap) {
        char c1[32]={0}, c2[32]={0}, c3[128]={0}, c4[128]={0}, c5[128]={0}, c6[32]={0};
        SQLLEN ind=0;

        SQLGetData(st, 1, SQL_C_CHAR, c1, sizeof(c1), &ind);
        SQLGetData(st, 2, SQL_C_CHAR, c2, sizeof(c2), &ind);
        SQLGetData(st, 3, SQL_C_CHAR, c3, sizeof(c3), &ind);
        SQLGetData(st, 4, SQL_C_CHAR, c4, sizeof(c4), &ind);
        SQLGetData(st, 5, SQL_C_CHAR, c5, sizeof(c5), &ind);
        SQLGetData(st, 6, SQL_C_CHAR, c6, sizeof(c6), &ind);

        SafeCopy(out[n].Tanggal,    sizeof(out[n].Tanggal),    c1);
        SafeCopy(out[n].TestDriveID,sizeof(out[n].TestDriveID),c2);
        SafeCopy(out[n].Mobil,      sizeof(out[n].Mobil),      c3);
        SafeCopy(out[n].Customer,   sizeof(out[n].Customer),   c4);
        SafeCopy(out[n].Sales,      sizeof(out[n].Sales),      c5);
        SafeCopy(out[n].Status,     sizeof(out[n].Status),     c6);
        n++;
    }

    CloseStmt(st);
    if (outCount) *outCount = n;
    return true;
}

// =========================
// Page 2: Insights
// =========================

static void NormalizeFromTo(char *fromOut, int fcap, char *toOut, int tcap, const char *fromYmd, const char *toYmd)
{
    // biar gampang: kalau kosong, pass NULL di SQL.
    SafeCopy(fromOut, fcap, fromYmd ? fromYmd : "");
    SafeCopy(toOut,   tcap, toYmd   ? toYmd   : "");
}

bool DbTestDriveInsight_LoadSummary(void *dbcVoid, TestDriveInsightSummary *out, const char *fromYmd, const char *toYmd)
{
    if (!dbcVoid || !out) return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char fromBuf[16], toBuf[16];
    NormalizeFromTo(fromBuf, sizeof(fromBuf), toBuf, sizeof(toBuf), fromYmd, toYmd);

    // call SP (pakai string). Kalau kamu punya SQLPrepare+bind param, lebih bagus.
    char sql[512];
    snprintf(sql, sizeof(sql),
        "EXEC dbo.usp_TestDrive_ReportSummary "
        "  @FromDate = %s, "
        "  @ToDate   = %s;",
        (fromBuf[0] ? "?" : "NULL"),
        (toBuf[0]   ? "?" : "NULL")
    );

    // versi cepat: embed literal kalau ada
    if (fromBuf[0] || toBuf[0]) {
        snprintf(sql, sizeof(sql),
            "EXEC dbo.usp_TestDrive_ReportSummary "
            "  @FromDate = %s, "
            "  @ToDate   = %s;",
            (fromBuf[0] ? "CONVERT(date,'" : "NULL"),
            (toBuf[0]   ? "CONVERT(date,'" : "NULL")
        );
        // biar simpel, rebuild yang benar:
        // (ini tetap aman karena input kamu dari textbox YYYY-MM-DD)
        snprintf(sql, sizeof(sql),
            "EXEC dbo.usp_TestDrive_ReportSummary "
            "  @FromDate = %s, "
            "  @ToDate   = %s;",
            (fromBuf[0] ? (char[64]){0} : "NULL"),
            (toBuf[0]   ? (char[64]){0} : "NULL")
        );
    }

    // supaya ga ribet literal builder, pakai cara final di bawah:
    {
        char a[64], b[64];
        if (fromBuf[0]) snprintf(a, sizeof(a), "CONVERT(date,'%s')", fromBuf); else strcpy(a, "NULL");
        if (toBuf[0])   snprintf(b, sizeof(b), "CONVERT(date,'%s')", toBuf);   else strcpy(b, "NULL");
        snprintf(sql, sizeof(sql),
            "EXEC dbo.usp_TestDrive_ReportSummary @FromDate=%s, @ToDate=%s;", a, b);
    }

    SQLHSTMT st=NULL;
    if (!ExecQuery(dbc, &st, sql)) return false;

    // expected columns:
    // TotalScheduled, TotalOngoing, TotalCanceled, TotalFinished, TotalConverted, ConversionRatePct
    SQLLEN ind=0;
    int a=0,b=0,c=0,d=0,e=0;
    double pct=0;

    if (SQLFetch(st) == SQL_SUCCESS) {
        SQLGetData(st, 1, SQL_C_LONG,   &a, 0, &ind);
        SQLGetData(st, 2, SQL_C_LONG,   &b, 0, &ind);
        SQLGetData(st, 3, SQL_C_LONG,   &c, 0, &ind);
        SQLGetData(st, 4, SQL_C_LONG,   &d, 0, &ind);
        SQLGetData(st, 5, SQL_C_LONG,   &e, 0, &ind);
        SQLGetData(st, 6, SQL_C_DOUBLE, &pct, 0, &ind);
    } else {
        CloseStmt(st);
        return false;
    }

    CloseStmt(st);

    out->totalScheduled = a;
    out->totalOngoing = b;
    out->totalCanceled = c;
    out->totalFinished = d;
    out->totalConverted = e;
    out->conversionRatePct = pct;
    return true;
}

int DbTestDriveInsight_LoadMostTestDrivenCars(void *dbcVoid, TestDriveMostCarRow *out, int cap, const char *fromYmd, const char *toYmd, int topN)
{
    if (!dbcVoid || !out || cap <= 0) return -1;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char fromBuf[16], toBuf[16];
    NormalizeFromTo(fromBuf, sizeof(fromBuf), toBuf, sizeof(toBuf), fromYmd, toYmd);
    if (topN <= 0) topN = 5;

    char a[64], b[64], sql[512];
    if (fromBuf[0]) snprintf(a, sizeof(a), "CONVERT(date,'%s')", fromBuf); else strcpy(a, "NULL");
    if (toBuf[0])   snprintf(b, sizeof(b), "CONVERT(date,'%s')", toBuf);   else strcpy(b, "NULL");

    snprintf(sql, sizeof(sql),
        "EXEC dbo.usp_TestDrive_MostTestDrivenCar @FromDate=%s, @ToDate=%s, @TopN=%d;",
        a, b, topN);

    SQLHSTMT st=NULL;
    if (!ExecQuery(dbc, &st, sql)) return -1;

    // expected columns: MobilID, NamaMobil, TotalTestDrive, FinishedCount
    int n=0;
    while (SQLFetch(st) == SQL_SUCCESS && n < cap) {
        char nama[128]={0};
        int total=0, fin=0;
        SQLLEN ind=0;

        SQLGetData(st, 2, SQL_C_CHAR, nama, sizeof(nama), &ind);
        SQLGetData(st, 3, SQL_C_LONG, &total, 0, &ind);
        SQLGetData(st, 4, SQL_C_LONG, &fin, 0, &ind);

        SafeCopy(out[n].Mobil, sizeof(out[n].Mobil), nama);
        out[n].totalTestDrive = total;
        out[n].finishedCount = fin;
        n++;
    }

    CloseStmt(st);
    return n;
}

int DbTestDriveInsight_LoadSalesPerformance(void *dbcVoid, TestDriveSalesPerfRow *out, int cap, const char *fromYmd, const char *toYmd, int topN)
{
    if (!dbcVoid || !out || cap <= 0) return -1;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char fromBuf[16], toBuf[16];
    NormalizeFromTo(fromBuf, sizeof(fromBuf), toBuf, sizeof(toBuf), fromYmd, toYmd);
    if (topN <= 0) topN = 10;

    char a[64], b[64], sql[512];
    if (fromBuf[0]) snprintf(a, sizeof(a), "CONVERT(date,'%s')", fromBuf); else strcpy(a, "NULL");
    if (toBuf[0])   snprintf(b, sizeof(b), "CONVERT(date,'%s')", toBuf);   else strcpy(b, "NULL");

    snprintf(sql, sizeof(sql),
        "EXEC dbo.usp_TestDrive_SalesPerformance @FromDate=%s, @ToDate=%s, @TopN=%d;",
        a, b, topN);

    SQLHSTMT st=NULL;
    if (!ExecQuery(dbc, &st, sql)) return -1;

    // expected columns:
    // SalesID, SalesName, TotalTestDrive, FinishedTestDrive, CanceledTestDrive,
    // ConvertedCount, ConversionRatePct, RevenueFromConverted
    int n=0;
    while (SQLFetch(st) == SQL_SUCCESS && n < cap) {
        char salesID[16]={0}, salesName[96]={0};
        int t=0, f=0, c=0, conv=0;
        double pct=0, rev=0;
        SQLLEN ind=0;

        SQLGetData(st, 1, SQL_C_CHAR, salesID, sizeof(salesID), &ind);
        SQLGetData(st, 2, SQL_C_CHAR, salesName, sizeof(salesName), &ind);
        SQLGetData(st, 3, SQL_C_LONG, &t, 0, &ind);
        SQLGetData(st, 4, SQL_C_LONG, &f, 0, &ind);
        SQLGetData(st, 5, SQL_C_LONG, &c, 0, &ind);
        SQLGetData(st, 6, SQL_C_LONG, &conv, 0, &ind);
        SQLGetData(st, 7, SQL_C_DOUBLE, &pct, 0, &ind);
        SQLGetData(st, 8, SQL_C_DOUBLE, &rev, 0, &ind);

        SafeCopy(out[n].SalesID, sizeof(out[n].SalesID), salesID);
        SafeCopy(out[n].SalesName, sizeof(out[n].SalesName), salesName);
        out[n].totalTestDrive = t;
        out[n].finishedTestDrive = f;
        out[n].canceledTestDrive = c;
        out[n].convertedCount = conv;
        out[n].conversionRatePct = pct;
        out[n].revenueFromConverted = rev;
        n++;
    }

    CloseStmt(st);
    return n;
}
