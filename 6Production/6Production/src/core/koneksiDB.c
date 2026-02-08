#include "koneksiDB.h"
#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>
#include <stdio.h>

void connectKeDB(SQLHDBC *koneksiDb)
{
    printf("Mulai connectKeDB...\n");
    SQLHENV env;
    SQLHSTMT stmt;
    SQLRETURN ret;
    SQLSMALLINT columns;
    SQLCHAR sqlState[6], message[256];
    SQLINTEGER nativeError;
    SQLSMALLINT textLength;

    // Allocate environment handle
    ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    if (!SQL_SUCCEEDED(ret)) {
        printf("Gagal allocate environment handle: %d\n", ret);
        return;
    }

    // Set ODBC version
    ret = SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void *)SQL_OV_ODBC3, 0);
    if (!SQL_SUCCEEDED(ret)) {
        printf("Gagal set ODBC version: %d\n", ret);
        SQLFreeHandle(SQL_HANDLE_ENV, env);
        return;
    }

    // Allocate connection handle
    ret = SQLAllocHandle(SQL_HANDLE_DBC, env, koneksiDb);
    if (!SQL_SUCCEEDED(ret)) {
        printf("Gagal allocate connection handle: %d\n", ret);
        SQLFreeHandle(SQL_HANDLE_ENV, env);
        return;
    }

    // Connect to database
    // Database project ini sekarang menggunakan dbGWD_NEW.
    // Pastikan DSN ODBC kamu mengarah ke server yang sama.
    ret = SQLDriverConnect(*koneksiDb, NULL,
        "DSN=dbGWD_NEW;DATABASE=dbGWD_NEW;Uid=Admin;Pwd=6Production",
        SQL_NTS, NULL, 0, NULL, SQL_DRIVER_COMPLETE);

    if (SQL_SUCCEEDED(ret))
    {
        printf("Koneksi berhasil\n");
        // --- debug: pastikan DB yang kepake sama ---
        SQLHSTMT st = SQL_NULL_HSTMT;
        if (SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, *koneksiDb, &st)))
        {
            SQLCHAR dbname[128] = {0};
            if (SQL_SUCCEEDED(SQLExecDirect(st, (SQLCHAR*)"SELECT DB_NAME()", SQL_NTS)) &&
                SQL_SUCCEEDED(SQLFetch(st)))
            {
                SQLGetData(st, 1, SQL_C_CHAR, dbname, sizeof(dbname), NULL);
                printf("DB aktif (ODBC): %s\n", dbname);
            }
            SQLFreeHandle(SQL_HANDLE_STMT, st);
        }
    }
    else
    {
        printf("Koneksi gagal! Kode error: %d\n", ret);

        // Get detailed error information
        if (SQLGetDiagRec(SQL_HANDLE_DBC, *koneksiDb, 1, sqlState, &nativeError, message, sizeof(message), &textLength) == SQL_SUCCESS) {
            printf("ODBC Error: [%s] %ld: %s\n", sqlState, (long)nativeError, message);
        } else {
            printf("Tidak dapat mendapatkan detail error ODBC\n");
        }

        // Free handles on failure
        SQLFreeHandle(SQL_HANDLE_DBC, *koneksiDb);
        *koneksiDb = NULL;
        SQLFreeHandle(SQL_HANDLE_ENV, env);
    }
}

void disconnectDB(SQLHDBC *koneksiDb) {
    if (koneksiDb && *koneksiDb) {
        SQLDisconnect(*koneksiDb);
        SQLFreeHandle(SQL_HANDLE_DBC, *koneksiDb);
        *koneksiDb = NULL;
    }
}
