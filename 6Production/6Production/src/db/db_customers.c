#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "db_customers.h"

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


bool DbCustomers_LoadAll(void *dbcVoid, Customer *out, int outCap, int *outCount)
{
    SQLHDBC dbc = (SQLHDBC)dbcVoid;
    if (!dbc || !out || outCap <= 0 || !outCount) return false;

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;

    const char *sql =
    "SELECT PelangganID, NoKTP, Nama, Email, NoHP, CAST(Alamat AS VARCHAR(128)) AS Alamat "
    "FROM dbo.Pelanggan WHERE IsActive = 1 ORDER BY ID";

    if (!SQL_SUCCEEDED(SQLExecDirect(stmt, (SQLCHAR*)sql, SQL_NTS))) {
    PrintOdbcError(SQL_HANDLE_STMT, stmt);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return false;
}

    int n = 0;
  SQLRETURN fr;
while ((fr = SQLFetch(stmt)) != SQL_NO_DATA && SQL_SUCCEEDED(fr) && n < outCap) {
        SQLGetData(stmt, 1, SQL_C_CHAR, out[n].PelangganID, sizeof(out[n].PelangganID), NULL);
        SQLGetData(stmt, 2, SQL_C_CHAR, out[n].NoKTP,       sizeof(out[n].NoKTP),       NULL);
        SQLGetData(stmt, 3, SQL_C_CHAR, out[n].Nama,        sizeof(out[n].Nama),        NULL);
        SQLGetData(stmt, 4, SQL_C_CHAR, out[n].Email,       sizeof(out[n].Email),       NULL);
        SQLGetData(stmt, 5, SQL_C_CHAR, out[n].NoHp,        sizeof(out[n].NoHp),        NULL);
        SQLGetData(stmt, 6, SQL_C_CHAR, out[n].Alamat,      sizeof(out[n].Alamat),      NULL);
        n++;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    *outCount = n;
    return true;
}

bool DbCustomers_Insert(void *dbcVoid, const char *noKtp, const char *nama, const char *email, const char *noHp, const char *alamat)
{
    SQLHDBC dbc = (SQLHDBC)dbcVoid;
    if (!dbc || !nama || nama[0] == '\0') return false;

    char ktpE[64], nmE[128], emE[128], hpE[64], alE[256];
    EscapeSql(noKtp ? noKtp : "", ktpE, sizeof(ktpE));
    EscapeSql(nama,   nmE, sizeof(nmE));
    EscapeSql(email ? email : "", emE, sizeof(emE));
    EscapeSql(noHp  ? noHp  : "", hpE, sizeof(hpE));
    EscapeSql(alamat? alamat: "", alE, sizeof(alE));

    char query[700];
    snprintf(query, sizeof(query),
        "INSERT INTO dbo.Pelanggan (NoKTP, Nama, Email, NoHP, Alamat)"
        "VALUES ('%s','%s','%s','%s','%s')", ktpE, nmE, emE, hpE, alE);

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;

    SQLRETURN r = SQLExecDirect(stmt, (SQLCHAR*)query, SQL_NTS);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    return SQL_SUCCEEDED(r);
}

bool DbCustomers_Update(void *dbcVoid, const char *pelangganId, const char *noKtp, const char *nama, const char *email, const char *noHp, const char *alamat)
{
    SQLHDBC dbc = (SQLHDBC)dbcVoid;
    if (!dbc || !pelangganId || pelangganId[0] == '\0') return false;

    char idE[32], ktpE[64], nmE[128], emE[128], hpE[64], alE[256];
    EscapeSql(pelangganId, idE, sizeof(idE));
    EscapeSql(noKtp ? noKtp : "", ktpE, sizeof(ktpE));
    EscapeSql(nama ? nama : "", nmE, sizeof(nmE));
    EscapeSql(email? email: "", emE, sizeof(emE));
    EscapeSql(noHp ? noHp : "", hpE, sizeof(hpE));
    EscapeSql(alamat?alamat:"", alE, sizeof(alE));

    char query[800];
    snprintf(query, sizeof(query),
       "UPDATE dbo.Pelanggan SET NoKTP='%s', Nama='%s', Email='%s', NoHP='%s', Alamat='%s' WHERE PelangganID='%s'",
        ktpE, nmE, emE, hpE, alE, idE);

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;

    SQLRETURN r = SQLExecDirect(stmt, (SQLCHAR*)query, SQL_NTS);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    return SQL_SUCCEEDED(r);
}

bool DbCustomers_Delete(void *dbcVoid, const char *pelangganId)
{
    SQLHDBC dbc = (SQLHDBC)dbcVoid;
    if (!dbc || !pelangganId || pelangganId[0] == '\0') return false;

    char idE[32];
    EscapeSql(pelangganId, idE, sizeof(idE));

    char query[256];
    snprintf(query, sizeof(query),
        "UPDATE dbo.Pelanggan SET IsActive = 0 WHERE PelangganID='%s' AND IsActive = 1", idE);

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;

    SQLRETURN r = SQLExecDirect(stmt, (SQLCHAR*)query, SQL_NTS);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    return SQL_SUCCEEDED(r);
}