#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "db_cars.h"

#include <windows.h>
#include <sql.h>
#include <sqlext.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* ========= helpers ========= */

static void EscapeSql(const char *src, char *dst, size_t dstSize)
{
    if (!dst || dstSize == 0) return;
    size_t j = 0;
    if (!src) { dst[0] = '\0'; return; }

    for (size_t i = 0; src[i] && j + 1 < dstSize; i++) {
        if (src[i] == '\'') {
            if (j + 2 < dstSize) {
                dst[j++] = '\'';
                dst[j++] = '\'';
            } else break;
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

static void DigitsOnly(const char *src, char *dst, size_t dstSize, const char *fallback)
{
    if (!dst || dstSize == 0) return;
    size_t j = 0;

    if (src) {
        for (size_t i = 0; src[i] && j + 1 < dstSize; i++) {
            if (isdigit((unsigned char)src[i])) dst[j++] = src[i];
        }
    }

    if (j == 0 && fallback) {
        strncpy(dst, fallback, dstSize - 1);
        dst[dstSize - 1] = '\0';
    } else {
        dst[j] = '\0';
    }
}

static void PrintOdbcErrorAll(SQLSMALLINT handleType, SQLHANDLE handle)
{
    SQLSMALLINT i = 1;
    SQLCHAR state[6] = {0};
    SQLCHAR msg[512] = {0};
    SQLINTEGER native = 0;
    SQLSMALLINT len = 0;

    while (SQLGetDiagRec(handleType, handle, i, state, &native, msg, (SQLSMALLINT)sizeof(msg), &len) == SQL_SUCCESS)
    {
        printf("ODBC Error [%s] %ld: %s\n", state, (long)native, msg);
        i++;
    }
}

static bool ExecSQLRows(SQLHDBC dbc, const char *sql, SQLLEN *outRows)
{
    if (outRows) *outRows = 0;
    if (!dbc || !sql) return false;

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;

    SQLRETURN r = SQLExecDirect(stmt, (SQLCHAR*)sql, SQL_NTS);
    if (!SQL_SUCCEEDED(r)) {
        PrintOdbcErrorAll(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    SQLLEN rows = 0;
    SQLRowCount(stmt, &rows);
    if (outRows) *outRows = rows;

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return true;
}

static bool ExecSQL(SQLHDBC dbc, const char *sql)
{
    return ExecSQLRows(dbc, sql, NULL);
}

/* ========= API ========= */

bool DbCars_LoadAll(void *dbcVoid, CarData *out, int outCap, int *outCount)
{
    if (outCount) *outCount = 0;
    if (!dbcVoid || !out || outCap <= 0 || !outCount) return false;

    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    /* ✅ URUTAN: TERBARU -> TERLAMA */
    const char *sql =
        "SELECT MobilID, NamaMobil, TipeMobil, "
        "CAST(Stok AS VARCHAR(16)) AS Stok, "
        "CAST(TahunProduksi AS VARCHAR(16)) AS TahunProduksi, "
        "CAST(Harga AS VARCHAR(32)) AS Harga, "
        "CAST(IsActive AS INT) AS IsActive "
        "FROM dbo.Mobil "
        "ORDER BY MobilID DESC";

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;

    SQLRETURN er = SQLExecDirect(stmt, (SQLCHAR*)sql, SQL_NTS);
    if (!SQL_SUCCEEDED(er)) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    int n = 0;
    SQLRETURN fr;
    while ((fr = SQLFetch(stmt)) != SQL_NO_DATA && SQL_SUCCEEDED(fr) && n < outCap) {
        CarData c;
        memset(&c, 0, sizeof(c));

        SQLGetData(stmt, 1, SQL_C_CHAR, c.MobilID,       sizeof(c.MobilID),       NULL);
        SQLGetData(stmt, 2, SQL_C_CHAR, c.NamaMobil,     sizeof(c.NamaMobil),     NULL);
        SQLGetData(stmt, 3, SQL_C_CHAR, c.TipeMobil,     sizeof(c.TipeMobil),     NULL);
        SQLGetData(stmt, 4, SQL_C_CHAR, c.Stok,          sizeof(c.Stok),          NULL);
        SQLGetData(stmt, 5, SQL_C_CHAR, c.TahunProduksi, sizeof(c.TahunProduksi), NULL);
        SQLGetData(stmt, 6, SQL_C_CHAR, c.Harga,         sizeof(c.Harga),         NULL);

        SQLINTEGER isActive = 1;
        SQLLEN ind = 0;
        SQLGetData(stmt, 7, SQL_C_LONG, &isActive, sizeof(isActive), &ind);
        c.IsActive = (ind == SQL_NULL_DATA) ? 1 : (isActive != 0);

        out[n++] = c;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    *outCount = n;
    return true;
}


bool DbCars_Insert(void *dbcVoid,
                   const char *tipeMobil,
                   const char *nama,
                   const char *stok,
                   const char *tahunProduksi,
                   const char *harga)
{
    if (!dbcVoid) return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char tipeE[64], namaE[64];
    EscapeSql(tipeMobil ? tipeMobil : "", tipeE, sizeof(tipeE));
    EscapeSql(nama ? nama : "", namaE, sizeof(namaE));

    char stokN[16], tahunN[16], hargaN[32];
    DigitsOnly(stok, stokN, sizeof(stokN), "0");
    DigitsOnly(tahunProduksi, tahunN, sizeof(tahunN), "0");
    DigitsOnly(harga, hargaN, sizeof(hargaN), "0");

    char sql[1024];
    snprintf(sql, sizeof(sql),
             "INSERT INTO dbo.Mobil (TipeMobil, NamaMobil, Stok, TahunProduksi, Harga, IsActive) "
             "VALUES ('%s','%s',%s,%s,%s,1)",
             tipeE, namaE, stokN, tahunN, hargaN);

    return ExecSQL(dbc, sql);
}

bool DbCars_Update(void *dbcVoid,
                   const char *mobilId,
                   const char *tipeMobil,
                   const char *nama,
                   const char *stok,
                   const char *tahunProduksi,
                   const char *harga)
{
    if (!dbcVoid || !mobilId) return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char idE[32], tipeE[64], namaE[64];
    EscapeSql(mobilId, idE, sizeof(idE));
    EscapeSql(tipeMobil ? tipeMobil : "", tipeE, sizeof(tipeE));
    EscapeSql(nama ? nama : "", namaE, sizeof(namaE));

    char stokN[16], tahunN[16], hargaN[32];
    DigitsOnly(stok, stokN, sizeof(stokN), "0");
    DigitsOnly(tahunProduksi, tahunN, sizeof(tahunN), "0");
    DigitsOnly(harga, hargaN, sizeof(hargaN), "0");

    char sql[1200];
    snprintf(sql, sizeof(sql),
             "UPDATE dbo.Mobil SET "
             "TipeMobil='%s', NamaMobil='%s', Stok=%s, TahunProduksi=%s, Harga=%s "
             "WHERE MobilID='%s'",
             tipeE, namaE, stokN, tahunN, hargaN, idE);

    return ExecSQL(dbc, sql);
}

bool DbCars_Deactivate(void *dbcVoid, const char *mobilId)
{
    if (!dbcVoid || !mobilId) return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char idE[32];
    EscapeSql(mobilId, idE, sizeof(idE));

    char sql[256];
    snprintf(sql, sizeof(sql),
             "UPDATE dbo.Mobil SET IsActive = 0 "
             "WHERE MobilID='%s' AND IsActive = 1",
             idE);

    SQLLEN rows = 0;
    if (!ExecSQLRows(dbc, sql, &rows)) return false;
    return (rows > 0);
}

bool DbCars_Activate(void *dbcVoid, const char *mobilId)
{
    if (!dbcVoid || !mobilId) return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char idE[32];
    EscapeSql(mobilId, idE, sizeof(idE));

    char sql[256];
    snprintf(sql, sizeof(sql),
             "UPDATE dbo.Mobil SET IsActive = 1 "
             "WHERE MobilID='%s' AND IsActive = 0",
             idE);

    SQLLEN rows = 0;
    if (!ExecSQLRows(dbc, sql, &rows)) return false;
    return (rows > 0);
}

/* Delete = Deactivate (jangan duplicate di file lain!) */
bool DbCars_Delete(void *dbcVoid, const char *mobilId)
{
    return DbCars_Deactivate(dbcVoid, mobilId);
}

/* Update stok (optional) */
bool DbCarData_UpdateStock(void *dbcVoid, const char *mobilId, int newStock)
{
    if (!dbcVoid || !mobilId) return false;
    if (newStock < 0) newStock = 0;

    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char idE[32];
    EscapeSql(mobilId, idE, sizeof(idE));

    char sql[256];
    snprintf(sql, sizeof(sql),
             "UPDATE dbo.Mobil SET Stok=%d WHERE MobilID='%s'",
             newStock, idE);

    return ExecSQL(dbc, sql);
}
