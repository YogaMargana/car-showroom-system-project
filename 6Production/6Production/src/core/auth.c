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
                  Role *outRole,
                  char *outKaryawanID,
                  int outKaryawanIDSize,
                  char *outNama,
                  int outNamaSize)
{
    if (outRole) *outRole = ROLE_NONE;
    if (outKaryawanID && outKaryawanIDSize > 0) outKaryawanID[0] = '\0';
    if (outNama && outNamaSize > 0) outNama[0] = '\0';

    if (!conn || !*conn || !username || !password || !outRole)
        return false;

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, *conn, &stmt);
    if (!SQL_SUCCEEDED(ret))
        return false;

    const char *sql =
        "SELECT KaryawanID, Nama, Posisi "
        "FROM dbo.Karyawan "
        "WHERE Username = ? AND [Password] = ?";

    ret = SQLPrepare(stmt, (SQLCHAR *)sql, SQL_NTS);
    if (!SQL_SUCCEEDED(ret))
    {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                     0, 0, (SQLPOINTER)username, 0, NULL);

    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                     0, 0, (SQLPOINTER)password, 0, NULL);

    ret = SQLExecute(stmt);
    if (!SQL_SUCCEEDED(ret))
    {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    ret = SQLFetch(stmt);
    if (ret == SQL_NO_DATA)
    {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
    if (!SQL_SUCCEEDED(ret))
    {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    char karyawanID[32] = {0};
    char nama[64] = {0};
    char posisi[32] = {0};

    SQLGetData(stmt, 1, SQL_C_CHAR, karyawanID, sizeof(karyawanID), NULL);
    SQLGetData(stmt, 2, SQL_C_CHAR, nama, sizeof(nama), NULL);
    SQLGetData(stmt, 3, SQL_C_CHAR, posisi, sizeof(posisi), NULL);

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    if (strcmp(posisi, "Admin") == 0) *outRole = ROLE_ADMIN;
    else if (strcmp(posisi, "Kasir") == 0) *outRole = ROLE_CASHIER;
    else if (strcmp(posisi, "Sales") == 0) *outRole = ROLE_SALES;
    else *outRole = ROLE_NONE;

    if (outKaryawanID && outKaryawanIDSize > 0)
        snprintf(outKaryawanID, outKaryawanIDSize, "%s", karyawanID);

    if (outNama && outNamaSize > 0)
        snprintf(outNama, outNamaSize, "%s", nama);

    return (*outRole != ROLE_NONE);
}
