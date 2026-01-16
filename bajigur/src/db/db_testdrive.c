#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "db_testdrive.h"

#include <windows.h>
#include <sql.h>
#include <sqlext.h>

#include <stdio.h>
#include <string.h>
#include <stdint.h>

static void PrintOdbcError(SQLSMALLINT handleType, SQLHANDLE handle)
{
    SQLCHAR state[6], msg[256];
    SQLINTEGER native;
    SQLSMALLINT len;

    if (SQLGetDiagRec(handleType, handle, 1, state, &native, msg, sizeof(msg), &len) == SQL_SUCCESS) {
        printf("ODBC Error [%s] %ld: %s\n", state, (long)native, msg);
    }
}

static void EscapeSql(const char *src, char *dst, size_t dstSize)
{
    size_t j = 0;
    for (size_t i = 0; src && src[i] && j + 1 < dstSize; i++) {
        if (src[i] == '\'') {
            if (j + 2 < dstSize) { dst[j++] = '\''; dst[j++] = '\''; }
            else break;
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

static bool ExecSQL(SQLHDBC dbc, const char *sql)
{
    if (!dbc || !sql) return false;

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;

    /* Prevent UI freeze if server waits on a lock/network: limit query execution time. */
    {
        SQLULEN timeoutSec = 5;
        (void)SQLSetStmtAttr(stmt, SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER)(uintptr_t)timeoutSec, 0);
    }

    SQLRETURN r = SQLExecDirect(stmt, (SQLCHAR*)sql, SQL_NTS);

    if (!SQL_SUCCEEDED(r)) {
        PrintOdbcError(SQL_HANDLE_STMT, stmt);
        /* Optional: uncomment if you want to see the SQL that failed */
        /* printf("SQL Failed: %s\n", sql); */
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return SQL_SUCCEEDED(r);
}

bool DbTestDrive_LoadAll(void *dbcVoid, TestDrive *out, int outCap, int *outCount)
{
    if (outCount) *outCount = 0;
    if (!dbcVoid || !out || outCap <= 0 || !outCount) return false;

    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    /* Limit rows at SQL level so reload doesn't stall if table grows large. */
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT TOP (%d) TestDriveID, MobilID, KaryawanID, PelangganID, "
             "CONVERT(VARCHAR(16), Tanggal_TestDrive, 23) AS Tanggal, "
             "Status "
             "FROM dbo.TestDrive "
             "ORDER BY ID DESC",
             outCap);

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;

    {
        SQLULEN timeoutSec = 5;
        (void)SQLSetStmtAttr(stmt, SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER)(uintptr_t)timeoutSec, 0);
    }

    if (!SQL_SUCCEEDED(SQLExecDirect(stmt, (SQLCHAR*)sql, SQL_NTS))) {
        PrintOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    int n = 0;
    SQLRETURN fr;
    while ((fr = SQLFetch(stmt)) != SQL_NO_DATA && SQL_SUCCEEDED(fr) && n < outCap) {
        TestDrive td = {0};

        SQLGetData(stmt, 1, SQL_C_CHAR, td.TestDriveID, sizeof(td.TestDriveID), NULL);
        SQLGetData(stmt, 2, SQL_C_CHAR, td.MobilID,     sizeof(td.MobilID),     NULL);
        SQLGetData(stmt, 3, SQL_C_CHAR, td.KaryawanID,  sizeof(td.KaryawanID),  NULL);
        SQLGetData(stmt, 4, SQL_C_CHAR, td.PelangganID, sizeof(td.PelangganID), NULL);
        SQLGetData(stmt, 5, SQL_C_CHAR, td.Tanggal,     sizeof(td.Tanggal),     NULL);
        SQLGetData(stmt, 6, SQL_C_CHAR, td.Status,      sizeof(td.Status),      NULL);

        out[n++] = td;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    *outCount = n;
    return true;
}

bool DbTestDrive_Insert(void *dbcVoid,
                        const char *mobilId,
                        const char *karyawanId,
                        const char *pelangganId,
                        const char *tanggal,
                        const char *status)
{
    if (!dbcVoid) return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char m[32], k[32], p[32], t[32], s[32];
    EscapeSql(mobilId ? mobilId : "",     m, sizeof(m));
    EscapeSql(karyawanId ? karyawanId : "", k, sizeof(k));
    EscapeSql(pelangganId ? pelangganId : "", p, sizeof(p));
    EscapeSql(tanggal ? tanggal : "",     t, sizeof(t));
    EscapeSql(status ? status : "",       s, sizeof(s));

    char sql[600];
    snprintf(sql, sizeof(sql),
             "INSERT INTO dbo.TestDrive (MobilID, KaryawanID, PelangganID, Tanggal_TestDrive, Status) "
             "VALUES ('%s','%s','%s','%s','%s')",
             m, k, p, t, s);

    return ExecSQL(dbc, sql);
}

bool DbTestDrive_Update(void *dbcVoid,
                        const char *testDriveId,
                        const char *mobilId,
                        const char *karyawanId,
                        const char *pelangganId,
                        const char *tanggal,
                        const char *status)
{
    if (!dbcVoid || !testDriveId) return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char id[32], m[32], k[32], p[32], t[32], s[32];
    EscapeSql(testDriveId, id, sizeof(id));
    EscapeSql(mobilId ? mobilId : "",     m, sizeof(m));
    EscapeSql(karyawanId ? karyawanId : "", k, sizeof(k));
    EscapeSql(pelangganId ? pelangganId : "", p, sizeof(p));
    EscapeSql(tanggal ? tanggal : "",     t, sizeof(t));
    EscapeSql(status ? status : "",       s, sizeof(s));

    char sql[700];
    snprintf(sql, sizeof(sql),
             "UPDATE dbo.TestDrive SET "
             "MobilID='%s', KaryawanID='%s', PelangganID='%s', "
             "Tanggal_TestDrive='%s', Status='%s' "
             "WHERE TestDriveID='%s'",
             m, k, p, t, s, id);

    return ExecSQL(dbc, sql);
}

bool DbTestDrive_Delete(void *dbcVoid, const char *testDriveId)
{
    if (!dbcVoid || !testDriveId) return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char id[32];
    EscapeSql(testDriveId, id, sizeof(id));

    char sql[256];
    snprintf(sql, sizeof(sql),
             "DELETE FROM dbo.TestDrive WHERE TestDriveID='%s'",
             id);

    return ExecSQL(dbc, sql);
}
