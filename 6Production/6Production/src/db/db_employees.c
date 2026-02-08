#include "db_employees.h"

#include <stdio.h>
#include <string.h>

#include <windows.h>
#include <sql.h>
#include <sqlext.h>

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
    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;

    SQLRETURN r = SQLExecDirect(stmt, (SQLCHAR*)sql, SQL_NTS);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return SQL_SUCCEEDED(r);
}

bool DbEmployees_LoadAll(void *dbcVoid, Employee *out, int outCap, int *outCount)
{
    if (outCount) *outCount = 0;
    if (!dbcVoid || !out || outCap <= 0 || !outCount) return false;

    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    const char *sql =
        "SELECT KaryawanID, Nama, Posisi, NoHP, Email, Username, [Password] "
        "FROM dbo.Karyawan WHERE IsActive = 1 ORDER BY ID";

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;

    if (!SQL_SUCCEEDED(SQLExecDirect(stmt, (SQLCHAR*)sql, SQL_NTS))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    int n = 0;
    SQLRETURN fr;
    while ((fr = SQLFetch(stmt)) != SQL_NO_DATA && SQL_SUCCEEDED(fr) && n < outCap) {
        Employee e = {0};

        SQLGetData(stmt, 1, SQL_C_CHAR, e.KaryawanID, sizeof(e.KaryawanID), NULL);
        SQLGetData(stmt, 2, SQL_C_CHAR, e.Nama,       sizeof(e.Nama),       NULL);
        SQLGetData(stmt, 3, SQL_C_CHAR, e.Posisi,     sizeof(e.Posisi),     NULL);
        SQLGetData(stmt, 4, SQL_C_CHAR, e.NoHP,       sizeof(e.NoHP),       NULL);
        SQLGetData(stmt, 5, SQL_C_CHAR, e.Email,      sizeof(e.Email),      NULL);
        SQLGetData(stmt, 6, SQL_C_CHAR, e.Username,   sizeof(e.Username),   NULL);
        SQLGetData(stmt, 7, SQL_C_CHAR, e.Password,   sizeof(e.Password),   NULL);

        out[n++] = e;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    *outCount = n;
    return true;
}

bool DbEmployees_Insert(void *dbcVoid,
                        const char *nama,
                        const char *posisi,
                        const char *nohp,
                        const char *email,
                        const char *username,
                        const char *password)
{
    if (!dbcVoid) return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char n[128], p[64], h[32], em[128], u[128], pw[300];
    EscapeSql(nama, n, sizeof(n));
    EscapeSql(posisi, p, sizeof(p));
    EscapeSql(nohp, h, sizeof(h));
    EscapeSql(email, em, sizeof(em));
    EscapeSql(username, u, sizeof(u));
    EscapeSql(password, pw, sizeof(pw));

    char sql[1024];
    snprintf(sql, sizeof(sql),
             "INSERT INTO dbo.Karyawan (Nama, Posisi, NoHP, Email, Username, [Password]) "
             "VALUES ('%s','%s','%s','%s','%s','%s')",
             n, p, h, em, u, pw);

    return ExecSQL(dbc, sql);
}

bool DbEmployees_Update(void *dbcVoid,
                        const char *karyawanId,
                        const char *nama,
                        const char *posisi,
                        const char *nohp,
                        const char *email,
                        const char *username,
                        const char *password)
{
    if (!dbcVoid || !karyawanId) return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char id[32], n[128], p[64], h[32], em[128], u[128], pw[300];
    EscapeSql(karyawanId, id, sizeof(id));
    EscapeSql(nama, n, sizeof(n));
    EscapeSql(posisi, p, sizeof(p));
    EscapeSql(nohp, h, sizeof(h));
    EscapeSql(email, em, sizeof(em));
    EscapeSql(username, u, sizeof(u));
    EscapeSql(password, pw, sizeof(pw));

    char sql[1200];
    snprintf(sql, sizeof(sql),
             "UPDATE dbo.Karyawan SET "
             "Nama='%s', Posisi='%s', NoHP='%s', Email='%s', Username='%s', [Password]='%s' "
             "WHERE KaryawanID='%s'",
             n, p, h, em, u, pw, id);

    return ExecSQL(dbc, sql);
}

bool DbEmployees_Delete(void *dbcVoid, const char *karyawanId)
{
    if (!dbcVoid || !karyawanId) return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char id[32];
    EscapeSql(karyawanId, id, sizeof(id));

    char sql[256];
    snprintf(sql, sizeof(sql),
             "UPDATE dbo.Karyawan SET IsActive = 0 WHERE KaryawanID='%s' AND IsActive = 1",
             id);

    return ExecSQL(dbc, sql);
}
