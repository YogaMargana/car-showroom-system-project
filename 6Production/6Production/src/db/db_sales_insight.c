#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "db_sales_insight.h"

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

int DbSalesInsight_LoadTopSalesByRevenue(void *dbcVoid, const char *fromYMD, const char *toYMD,
                                        SalesTopSalesRow *out, int cap)
{
    if (!dbcVoid || !out || cap <= 0) return -1;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    SQLHSTMT stmt = NULL;
    SQLRETURN rc = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    if (!SQL_SUCCEEDED(rc)) return -1;

    const char *sql = "{CALL dbo.usp_SalesReport_TopSales(?,?,?)}";
    rc = SQLPrepare(stmt, (SQLCHAR*)sql, SQL_NTS);
    if (!SQL_SUCCEEDED(rc)) { SQLFreeHandle(SQL_HANDLE_STMT, stmt); return -1; }

    SQLLEN ind1 = SQL_NTS, ind2 = SQL_NTS;
    SQLINTEGER topN = cap;
    SQLLEN ind3 = 0;

    if (!fromYMD || fromYMD[0] == '\0') ind1 = SQL_NULL_DATA;
    if (!toYMD   || toYMD[0]   == '\0') ind2 = SQL_NULL_DATA;

    rc = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 10, 0,
                          (SQLPOINTER)fromYMD, 0, &ind1);
    if (!SQL_SUCCEEDED(rc)) { SQLFreeHandle(SQL_HANDLE_STMT, stmt); return -1; }

    rc = SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 10, 0,
                          (SQLPOINTER)toYMD, 0, &ind2);
    if (!SQL_SUCCEEDED(rc)) { SQLFreeHandle(SQL_HANDLE_STMT, stmt); return -1; }

    rc = SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0,
                          &topN, 0, &ind3);
    if (!SQL_SUCCEEDED(rc)) { SQLFreeHandle(SQL_HANDLE_STMT, stmt); return -1; }

    rc = SQLExecute(stmt);
    if (!SQL_SUCCEEDED(rc)) { SQLFreeHandle(SQL_HANDLE_STMT, stmt); return -1; }

    char salesID[16] = {0};
    char salesName[96] = {0};
    SQLINTEGER txCount = 0;
    double revenue = 0.0;

    SQLLEN cb1 = 0, cb2 = 0, cb3 = 0, cb4 = 0;
    SQLBindCol(stmt, 1, SQL_C_CHAR,   salesID,   sizeof(salesID),   &cb1);
    SQLBindCol(stmt, 2, SQL_C_CHAR,   salesName, sizeof(salesName), &cb2);
    SQLBindCol(stmt, 3, SQL_C_LONG,   &txCount,  0,                &cb3);
    SQLBindCol(stmt, 4, SQL_C_DOUBLE, &revenue,  0,                &cb4);

    int n = 0;
    SQLRETURN fr;
    while ((fr = SQLFetch(stmt)) != SQL_NO_DATA && SQL_SUCCEEDED(fr) && n < cap) {
        SafeCopy(out[n].SalesID,   sizeof(out[n].SalesID),   (cb1 == SQL_NULL_DATA) ? "" : salesID);
        SafeCopy(out[n].SalesName, sizeof(out[n].SalesName), (cb2 == SQL_NULL_DATA) ? "" : salesName);
        out[n].TxCount = (cb3 == SQL_NULL_DATA) ? 0 : (int)txCount;
        out[n].Revenue = (cb4 == SQL_NULL_DATA) ? 0.0 : revenue;
        n++;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return n;
}


static SQLRETURN GetMoneyAsDouble(SQLHSTMT stmt, int col, double *outVal)
{
    if (!outVal) return SQL_ERROR;
    *outVal = 0.0;
    SQLLEN ind = 0;
    double v = 0.0;

    SQLRETURN r = SQLGetData(stmt, col, SQL_C_DOUBLE, &v, 0, &ind);
    if (SQL_SUCCEEDED(r) && ind != SQL_NULL_DATA) *outVal = v;
    return r;
}

static int GetInt(SQLHSTMT stmt, int col, int *outVal)
{
    if (!outVal) return 0;
    SQLLEN ind = 0;
    int v = 0;
    SQLGetData(stmt, col, SQL_C_SLONG, &v, 0, &ind);
    if (ind == SQL_NULL_DATA) v = 0;
    *outVal = v;
    return 1;
}

static void GetStr(SQLHSTMT stmt, int col, char *dst, int cap)
{
    SQLLEN ind = 0;
    char buf[256]; buf[0] = '\0';
    SQLGetData(stmt, col, SQL_C_CHAR, buf, sizeof(buf), &ind);
    if (ind == SQL_NULL_DATA) buf[0] = '\0';
    SafeCopy(dst, cap, buf);
}

/* =========================
   QUERIES (dipisah)
   ========================= */

static const char *Q_SUMMARY =
    "SELECT "

    /* TOTAL JENIS MOBIL TERJUAL */
    "  (SELECT COUNT(DISTINCT pmd.MobilID) "
    "   FROM vw_PenjualanMobil_Report v "
    "   JOIN PenjualanMobilDetail pmd "
    "     ON pmd.PenjualanMobilID = v.PenjualanMobilID "
    "  ) AS TotalMobilTerjual, "

    /* TOTAL JENIS AKSESORIS TERJUAL */
    "  (SELECT COUNT(DISTINCT pad.AksesorisID) "
    "   FROM vw_PenjualanAksesoris_Report v "
    "   JOIN PenjualanAksesorisDetail pad "
    "     ON pad.PenjualanAksesorisID = v.PenjualanAksesorisID "
    "  ) AS TotalAksesorisTerjual, "

    /* OMZET MOBIL */
    "  (SELECT ISNULL(SUM(TotalTransaksi),0) "
    "   FROM vw_PenjualanMobil_Report) AS OmzetMobil, "

    /* OMZET AKSESORIS */
    "  (SELECT ISNULL(SUM(TotalTransaksi),0) "
    "   FROM vw_PenjualanAksesoris_Report) AS OmzetAksesoris, "

    /* TOTAL TRANSAKSI */
    "  ( "
    "    (SELECT COUNT(*) FROM vw_PenjualanMobil_Report) "
    "    + "
    "    (SELECT COUNT(*) FROM vw_PenjualanAksesoris_Report) "
    "  ) AS SuccessTx;";

static const char *Q_MONTHLY =
    ";WITH MonthlyCar AS ( "
    "  SELECT "
    "    CONVERT(char(7), TanggalTransaksi, 120) AS Bulan, "
    "    COUNT(*) AS TxnCar, "
    "    SUM(TotalTransaksi) AS OmzetCar "
    "  FROM vw_PenjualanMobil_Report "
    "  GROUP BY CONVERT(char(7), TanggalTransaksi, 120) "
    "), "
    "MonthlyAcc AS ( "
    "  SELECT "
    "    CONVERT(char(7), TanggalTransaksi, 120) AS Bulan, "
    "    COUNT(*) AS TxnAcc, "
    "    SUM(TotalTransaksi) AS OmzetAcc "
    "  FROM vw_PenjualanAksesoris_Report "
    "  GROUP BY CONVERT(char(7), TanggalTransaksi, 120) "
    "), "
    "Months AS ( "
    "  SELECT Bulan FROM MonthlyCar "
    "  UNION "
    "  SELECT Bulan FROM MonthlyAcc "
    ") "
    "SELECT "
    "  m.Bulan, "
    "  ISNULL(c.TxnCar,0) + ISNULL(a.TxnAcc,0) AS JumlahTransaksi, "
    "  ISNULL(c.OmzetCar,0) + ISNULL(a.OmzetAcc,0) AS Omzet "
    "FROM Months m "
    "LEFT JOIN MonthlyCar c ON c.Bulan = m.Bulan "
    "LEFT JOIN MonthlyAcc a ON a.Bulan = m.Bulan "
    "ORDER BY m.Bulan DESC;";


static const char *Q_TOP_TX =
    ";WITH EmpCar AS ( "
    "  SELECT NamaSales AS Employee, COUNT(*) AS TxnCount "
    "  FROM vw_PenjualanMobil_Report "
    "  GROUP BY NamaSales "
    "), "
    "EmpAcc AS ( "
    "  SELECT NamaSales AS Employee, COUNT(*) AS TxnCount "
    "  FROM vw_PenjualanAksesoris_Report "
    "  GROUP BY NamaSales "
    "), "
    "EmpAll AS ( "
    "  SELECT Employee, SUM(TxnCount) AS TxnCount "
    "  FROM (SELECT * FROM EmpCar UNION ALL SELECT * FROM EmpAcc) x "
    "  GROUP BY Employee "
    ") "
    "SELECT TOP (5) Employee, TxnCount AS JumlahTransaksi "
    "FROM EmpAll "
    "ORDER BY TxnCount DESC, Employee ASC;";

static const char *Q_TOP_REVENUE =
    ";WITH EmpCar AS ( "
    "  SELECT NamaSales AS Employee, SUM(TotalTransaksi) AS TotalSales "
    "  FROM vw_PenjualanMobil_Report "
    "  GROUP BY NamaSales "
    "), "
    "EmpAcc AS ( "
    "  SELECT NamaSales AS Employee, SUM(TotalTransaksi) AS TotalSales "
    "  FROM vw_PenjualanAksesoris_Report "
    "  GROUP BY NamaSales "
    "), "
    "EmpAll AS ( "
    "  SELECT Employee, SUM(TotalSales) AS TotalSales "
    "  FROM (SELECT * FROM EmpCar UNION ALL SELECT * FROM EmpAcc) x "
    "  GROUP BY Employee "
    ") "
    "SELECT TOP (5) Employee, TotalSales AS TotalPenjualan "
    "FROM EmpAll "
    "ORDER BY TotalSales DESC, Employee ASC;";

static const char *Q_BEST_CAR =
    "SELECT TOP (5) "
    "  ISNULL(m.NamaMobil,'(Unknown Car)') AS NamaMobil, "
    "  SUM(pmd.Qty) AS UnitTerjual, "
    "  SUM(CAST(pmd.Subtotal AS money)) AS TotalPenjualan "
    "FROM dbo.PenjualanMobil pm "
    "JOIN dbo.PenjualanMobilDetail pmd ON pmd.PenjualanMobilID = pm.PenjualanMobilID "
    "LEFT JOIN dbo.Mobil m ON m.MobilID = pmd.MobilID "
    "WHERE pm.StatusPembayaran IN ('Berhasil') "
    "GROUP BY ISNULL(m.NamaMobil,'(Unknown Car)') "
    "ORDER BY UnitTerjual DESC, NamaMobil ASC;";

static const char *Q_BEST_ACC =
    "SELECT TOP (5) "
    "  ISNULL(a.NamaAksesoris,'(Unknown Accessory)') AS NamaAksesoris, "
    "  SUM(pad.Qty) AS QtyTerjual, "
    "  SUM(CAST(pad.Subtotal AS money)) AS TotalPenjualan "
    "FROM dbo.PenjualanAksesoris pa "
    "JOIN dbo.PenjualanAksesorisDetail pad ON pad.PenjualanAksesorisID = pa.PenjualanAksesorisID "
    "LEFT JOIN dbo.Aksesoris a ON a.AksesorisID = pad.AksesorisID "
    "WHERE pa.StatusPembayaran IN ('Berhasil') "
    "GROUP BY ISNULL(a.NamaAksesoris,'(Unknown Accessory)') "
    "ORDER BY QtyTerjual DESC, NamaAksesoris ASC;";

/* =========================
   EXEC HELPERS
   ========================= */

static bool ExecOneRow(void *dbcVoid, const char *sql, SQLHSTMT *outStmt)
{
    if (!dbcVoid || !sql || !outStmt) return false;
    *outStmt = NULL;

    SQLHDBC hdbc = (SQLHDBC)dbcVoid;
    SQLHSTMT stmt = NULL;

    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &stmt))) return false;

    SQLRETURN r = SQLExecDirectA(stmt, (SQLCHAR*)sql, SQL_NTS);
    if (!SQL_SUCCEEDED(r)) { SQLFreeHandle(SQL_HANDLE_STMT, stmt); return false; }

    r = SQLFetch(stmt);
    if (r == SQL_NO_DATA) { SQLFreeHandle(SQL_HANDLE_STMT, stmt); return false; }
    if (!SQL_SUCCEEDED(r)) { SQLFreeHandle(SQL_HANDLE_STMT, stmt); return false; }

    *outStmt = stmt;
    return true;
}

static int ExecMany(void *dbcVoid, const char *sql, SQLHSTMT *outStmt)
{
    if (!dbcVoid || !sql || !outStmt) return -1;
    *outStmt = NULL;

    SQLHDBC hdbc = (SQLHDBC)dbcVoid;
    SQLHSTMT stmt = NULL;

    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &stmt))) return -1;

    SQLRETURN r = SQLExecDirectA(stmt, (SQLCHAR*)sql, SQL_NTS);
    if (!SQL_SUCCEEDED(r)) { SQLFreeHandle(SQL_HANDLE_STMT, stmt); return -1; }

    *outStmt = stmt;
    return 0;
}

/* =========================
   PUBLIC API
   ========================= */

bool DbSalesInsight_LoadSummary(void *dbcVoid, SalesInsightSummary *outSum)
{
    if (!outSum) return false;
    memset(outSum, 0, sizeof(*outSum));
    if (!dbcVoid) return false;

    SQLHSTMT stmt = NULL;
    if (!ExecOneRow(dbcVoid, Q_SUMMARY, &stmt)) return false;

    GetInt(stmt, 1, &outSum->totalCarSold);
    GetInt(stmt, 2, &outSum->totalAccSold);
    GetMoneyAsDouble(stmt, 3, &outSum->omzetCar);
    GetMoneyAsDouble(stmt, 4, &outSum->omzetAcc);
    GetInt(stmt, 5, &outSum->successTx);

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return true;
}

int DbSalesInsight_LoadMonthly(void *dbcVoid, SalesInsightMonth *outArr, int cap)
{
    if (!dbcVoid || !outArr || cap <= 0) return -1;
    memset(outArr, 0, sizeof(outArr[0]) * (size_t)cap);

    SQLHSTMT stmt = NULL;
    if (ExecMany(dbcVoid, Q_MONTHLY, &stmt) != 0) return -1;

    int n = 0;
    while (n < cap) {
        SQLRETURN r = SQLFetch(stmt);
        if (r == SQL_NO_DATA) break;
        if (!SQL_SUCCEEDED(r)) break;

        GetStr(stmt, 1, outArr[n].month, (int)sizeof(outArr[n].month));
        GetInt(stmt, 2, &outArr[n].tx);
        GetMoneyAsDouble(stmt, 3, &outArr[n].omzet);
        n++;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return n;
}

static int LoadEmpList(void *dbcVoid, const char *sql, SalesInsightEmp *outArr, int cap, bool hasTotal)
{
    if (!dbcVoid || !outArr || cap <= 0) return -1;
    memset(outArr, 0, sizeof(outArr[0]) * (size_t)cap);

    SQLHSTMT stmt = NULL;
    if (ExecMany(dbcVoid, sql, &stmt) != 0) return -1;

    int n = 0;
    while (n < cap) {
        SQLRETURN r = SQLFetch(stmt);
        if (r == SQL_NO_DATA) break;
        if (!SQL_SUCCEEDED(r)) break;

        GetStr(stmt, 1, outArr[n].name, (int)sizeof(outArr[n].name));
        if (!hasTotal) {
            GetInt(stmt, 2, &outArr[n].tx);
            outArr[n].total = 0.0;
        } else {
            outArr[n].tx = 0;
            GetMoneyAsDouble(stmt, 2, &outArr[n].total);
        }
        n++;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return n;
}

static int LoadProdList(void *dbcVoid, const char *sql, SalesInsightProd *outArr, int cap)
{
    if (!dbcVoid || !outArr || cap <= 0) return -1;
    memset(outArr, 0, sizeof(outArr[0]) * (size_t)cap);

    SQLHSTMT stmt = NULL;
    if (ExecMany(dbcVoid, sql, &stmt) != 0) return -1;

    int n = 0;
    while (n < cap) {
        SQLRETURN r = SQLFetch(stmt);
        if (r == SQL_NO_DATA) break;
        if (!SQL_SUCCEEDED(r)) break;

        GetStr(stmt, 1, outArr[n].item, (int)sizeof(outArr[n].item));
        GetInt(stmt, 2, &outArr[n].qty);
        GetMoneyAsDouble(stmt, 3, &outArr[n].total);
        n++;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return n;
}

int DbSalesInsight_LoadBestCars(void *dbcVoid, SalesInsightProd *outArr, int cap)
{
    return LoadProdList(dbcVoid, Q_BEST_CAR, outArr, cap);
}

int DbSalesInsight_LoadBestAccessories(void *dbcVoid, SalesInsightProd *outArr, int cap)
{
    return LoadProdList(dbcVoid, Q_BEST_ACC, outArr, cap);
}

int DbSalesInsight_LoadTopSalesByTx(void *dbcVoid, SalesInsightEmp *outArr, int cap)
{
    return LoadEmpList(dbcVoid, Q_TOP_TX, outArr, cap, false);
}

int DbSalesInsight_LoadTopSalesByRevenueEmp(void *dbcVoid, SalesInsightEmp *outArr, int cap)
{
    return LoadEmpList(dbcVoid, Q_TOP_REVENUE, outArr, cap, true);
}