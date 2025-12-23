#include "auth.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

// pakai koneksi yang sudah dibuat di main.c (dikirim lewat parameter).
bool DBCheckLogin(SQLHDBC *conn,
                  const char *username,
                  const char *password,
                  Role *outRole)
{
    *outRole = ROLE_NONE;

    if (conn == NULL || *conn == SQL_NULL_HDBC) {
        printf("DBCheckLogin: koneksi DB belum siap.\n");
        return false;
    }

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    SQLRETURN ret;

    // 1) Allocate statement handle
    ret = SQLAllocHandle(SQL_HANDLE_STMT, *conn, &stmt);
    if (!SQL_SUCCEEDED(ret)) {
        printf("DBCheckLogin: gagal SQLAllocHandle STMT\n");
        return false;
    }

    // 2) Query ke tabel Karyawan
    const char *sql =
        "SELECT Posisi "
        "FROM Karyawan "
        "WHERE Username = ? AND [Password] = ?";

    ret = SQLPrepare(stmt, (SQLCHAR*)sql, SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) {
        printf("DBCheckLogin: gagal SQLPrepare\n");
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    // 3) Bind parameter Username
    SQLBindParameter(
        stmt,
        1,                  // param ke-1
        SQL_PARAM_INPUT,
        SQL_C_CHAR,         // tipe di C
        SQL_VARCHAR,        // tipe di SQL
        (SQLULEN)strlen(username),
        0,
        (SQLPOINTER)username,
        0,
        NULL
    );

    // 4) Bind parameter Password
    SQLBindParameter(
        stmt,
        2,                  // param ke-2
        SQL_PARAM_INPUT,
        SQL_C_CHAR,
        SQL_VARCHAR,
        (SQLULEN)strlen(password),
        0,
        (SQLPOINTER)password,
        0,
        NULL
    );

    // 5) Eksekusi
    ret = SQLExecute(stmt);
    if (!SQL_SUCCEEDED(ret)) {
        printf("DBCheckLogin: gagal SQLExecute\n");
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    // 6) Ambil satu baris (kalau ada)
    ret = SQLFetch(stmt);
    if (ret == SQL_NO_DATA) {
        // username/password tidak cocok
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
    if (!SQL_SUCCEEDED(ret)) {
        printf("DBCheckLogin: gagal SQLFetch\n");
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    char posisi[32] = {0};
    SQLLEN outLen = 0;

    ret = SQLGetData(stmt, 1, SQL_C_CHAR, posisi, sizeof(posisi), &outLen);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    if (!SQL_SUCCEEDED(ret)) {
        printf("DBCheckLogin: gagal SQLGetData\n");
        return false;
    }

    // 7) Mapping string Posisi -> enum Role
    if (strcmp(posisi, "Admin") == 0) {
        *outRole = ROLE_ADMIN;
    } else if (strcmp(posisi, "Kasir") == 0) {
        *outRole = ROLE_CASHIER;
    } else if (strcmp(posisi, "Sales") == 0) {
        *outRole = ROLE_SALES;
    } else {
        *outRole = ROLE_NONE;
    }

    return (*outRole != ROLE_NONE);
}
