#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "db_lookup.h"

#include <windows.h>
#include <sql.h>
#include <sqlext.h>

#include <stdio.h>
#include <string.h>

static bool QueryTwoCols(SQLHDBC dbc,
                         const char *sql,
                         LookupItem *out,
                         int outCap,
                         int *outCount,
                         const char *prefix)
{
    if (outCount) *outCount = 0;
    if (!dbc || !sql || !out || outCap <= 0 || !outCount) return false;

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;
    if (!SQL_SUCCEEDED(SQLExecDirect(stmt, (SQLCHAR*)sql, SQL_NTS))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    int n = 0;
    SQLRETURN fr;
    while ((fr = SQLFetch(stmt)) != SQL_NO_DATA && SQL_SUCCEEDED(fr) && n < outCap) {
        char id[32] = {0};
        char name[80] = {0};

        SQLGetData(stmt, 1, SQL_C_CHAR, id, sizeof(id), NULL);
        SQLGetData(stmt, 2, SQL_C_CHAR, name, sizeof(name), NULL);

        LookupItem it = {0};
        strncpy(it.id, id, sizeof(it.id) - 1);

        if (prefix && prefix[0]) {
            snprintf(it.label, sizeof(it.label), "%s%s - %s", prefix, id, name);
        } else {
            snprintf(it.label, sizeof(it.label), "%s - %s", id, name);
        }

        out[n++] = it;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    *outCount = n;
    return true;
}

bool DbLookup_LoadMobilList(void *dbcVoid, LookupItem *out, int outCap, int *outCount)
{
    if (!dbcVoid) return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;
    const char *sql =
        "SELECT MobilID, NamaMobil "
        "FROM dbo.Mobil ORDER BY ID";
    return QueryTwoCols(dbc, sql, out, outCap, outCount, "");
}

bool DbLookup_LoadCustomerList(void *dbcVoid, LookupItem *out, int outCap, int *outCount)
{
    if (!dbcVoid) return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;
    const char *sql =
        "SELECT PelangganID, Nama "
        "FROM dbo.Pelanggan ORDER BY ID";
    return QueryTwoCols(dbc, sql, out, outCap, outCount, "");
}

bool DbLookup_LoadSalesEmployeeList(void *dbcVoid, LookupItem *out, int outCap, int *outCount)
{
    if (!dbcVoid) return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;
    const char *sql =
        "SELECT KaryawanID, Nama "
        "FROM dbo.Karyawan "
        "WHERE Posisi='Sales' "
        "ORDER BY ID";
    return QueryTwoCols(dbc, sql, out, outCap, outCount, "");
}
