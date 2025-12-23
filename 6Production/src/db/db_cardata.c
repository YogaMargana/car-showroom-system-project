#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "db_cardata.h"

#include <windows.h>
#include <sql.h>
#include <sqlext.h>

#include <stdio.h>
#include <string.h>

static void EscapeSql(const char *src, char *dst, size_t dstSize)
{
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 1 < dstSize; i++) {
        if (src[i] == '\'') {
            if (j + 2 < dstSize) { dst[j++] = '\''; dst[j++] = '\''; }
            else break;
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

static void PrintOdbcError(SQLSMALLINT handleType, SQLHANDLE handle)
{
    SQLCHAR state[6], msg[256];
    SQLINTEGER native;
    SQLSMALLINT len;

    if (SQLGetDiagRec(handleType, handle, 1, state, &native, msg, sizeof(msg), &len) == SQL_SUCCESS) {
        printf("ODBC Error [%s] %ld: %s\n", state, (long)native, msg);
    }
}


bool DbCarData_LoadAll(void *dbcVoid, Car *out, int outCap, int *outCount)
{
    SQLHDBC dbc = (SQLHDBC)dbcVoid;
    if (!dbc || !out || outCap <= 0 || !outCount) return false;

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;

    const char *sql =
    "SELECT MobilID, NamaMobil, TahunProduksi, Harga, Stok "
    "FROM dbo.Cars ORDER BY MobilID";

    if (!SQL_SUCCEEDED(SQLExecDirect(stmt, (SQLCHAR*)sql, SQL_NTS))) {
        PrintOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    int n = 0;
    SQLRETURN fr;
    while ((fr = SQLFetch(stmt)) != SQL_NO_DATA && SQL_SUCCEEDED(fr) && n < outCap) {
        SQLGetData(stmt, 1, SQL_C_CHAR, out[n].MobilID,      sizeof(out[n].MobilID),      NULL);
        SQLGetData(stmt, 2, SQL_C_CHAR, out[n].NamaMobil,    sizeof(out[n].NamaMobil),    NULL);
        SQLGetData(stmt, 3, SQL_C_CHAR, out[n].TahunProduksi,sizeof(out[n].TahunProduksi),NULL);
        SQLGetData(stmt, 4, SQL_C_CHAR, out[n].Harga,        sizeof(out[n].Harga),        NULL);
        SQLGetData(stmt, 5, SQL_C_CHAR, out[n].Stok,         sizeof(out[n].Stok),         NULL);
        n++;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    *outCount = n;
    return true;
}

bool DbCarData_Insert(void *dbc, const char *namaMobil, const char *tahunProduksi, const char *harga, const char *stok)
{
    SQLHDBC dbcHandle = (SQLHDBC)dbc;
    if (!dbcHandle || !namaMobil || namaMobil[0] == '\0') return false;

    char nmE[128], brE[128], tpE[64], hgE[256], skE[64];
    EscapeSql(namaMobil,    nmE, sizeof(nmE));
    EscapeSql(tahunProduksi ? tahunProduksi : "", tpE, sizeof(tpE));
    EscapeSql(harga ? harga : "", hgE, sizeof(hgE));
    EscapeSql(stok ? stok : "", skE, sizeof(skE));

    char query[800];
    snprintf(query, sizeof(query),
        "INSERT INTO dbo.Cars (NamaMobil, TahunProduksi, Harga, Stok)"
        "VALUES ('%s','%s','%s','%s','%s')", nmE, brE, tpE, hgE, skE);

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbcHandle, &stmt))) return false;

    SQLRETURN r = SQLExecDirect(stmt, (SQLCHAR*)query, SQL_NTS);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    return SQL_SUCCEEDED(r);
}

bool DbCarData_Update(void *dbc, const char *mobilID, const char *namaMobil, const char *tahunProduksi, const char *harga, const char *stok)
{
    SQLHDBC dbcHandle = (SQLHDBC)dbc;
    if (!dbcHandle || !mobilID || mobilID[0] == '\0') return false;

    char idE[32], nmE[128], brE[128], tpE[64], hgE[256], skE[64];
    EscapeSql(mobilID, idE, sizeof(idE));
    EscapeSql(namaMobil ? namaMobil : "", nmE, sizeof(nmE));
    EscapeSql(tahunProduksi ? tahunProduksi : "", tpE, sizeof(tpE));
    EscapeSql(harga ? harga : "", hgE, sizeof(hgE));
    EscapeSql(stok ? stok : "", skE, sizeof(skE));

    char query[800];
    snprintf(query, sizeof(query),
       "UPDATE dbo.Cars SET NamaMobil='%s', TahunProduksi='%s', Harga='%s', Stok='%s' WHERE MobilID='%s'",
        nmE, brE, tpE, hgE, skE, idE);

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbcHandle, &stmt))) return false;

    SQLRETURN r = SQLExecDirect(stmt, (SQLCHAR*)query, SQL_NTS);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    return SQL_SUCCEEDED(r);
}

bool DbCarData_Delete(void *dbc, const char *mobilID)
{
    SQLHDBC dbcHandle = (SQLHDBC)dbc;
    if (!dbcHandle || !mobilID || mobilID[0] == '\0') return false;

    char idE[32];
    EscapeSql(mobilID, idE, sizeof(idE));

    char query[256];
    snprintf(query, sizeof(query),
        "DELETE FROM dbo.Cars WHERE MobilID='%s'", idE);

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbcHandle, &stmt))) return false;

    SQLRETURN r = SQLExecDirect(stmt, (SQLCHAR*)query, SQL_NTS);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    return SQL_SUCCEEDED(r);
}
