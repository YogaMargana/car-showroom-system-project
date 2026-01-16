#include "db_accesoris.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <windows.h>
#include <sql.h>
#include <sqlext.h>

/* =========================
   HELPER FUNCTIONS
========================= */

static void EscapeSql(const char *src, char *dst, size_t dstSize)
{
    size_t j = 0;
    for (size_t i = 0; src && src[i] && j + 1 < dstSize; i++)
    {
        if (src[i] == '\'')
        {
            if (j + 2 < dstSize)
            {
                dst[j++] = '\'';
                dst[j++] = '\'';
            }
            else
                break;
        }
        else
        {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

static bool ExecSQL(SQLHDBC dbc, const char *sql)
{
    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt)))
        return false;

    SQLRETURN r = SQLExecDirect(stmt, (SQLCHAR *)sql, SQL_NTS);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return SQL_SUCCEEDED(r);
}

/* =========================
   LOAD ALL
========================= */

bool DbAccesoriss_LoadAll(void *dbcVoid,
                          Accessoris *out,
                          int outCap,
                          int *outCount)
{
    if (outCount)
        *outCount = 0;
    if (!dbcVoid || !out || outCap <= 0 || !outCount)
        return false;

    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    const char *sql =
        "SELECT AksesorisID, NamaAksesoris, MerkAksesoris, Stok, Harga "
        "FROM dbo.Aksesoris "
        "ORDER BY AksesorisID";

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt)))
        return false;

    if (!SQL_SUCCEEDED(SQLExecDirect(stmt, (SQLCHAR *)sql, SQL_NTS)))
    {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    int n = 0;
    SQLRETURN fr;
    while ((fr = SQLFetch(stmt)) != SQL_NO_DATA &&
           SQL_SUCCEEDED(fr) &&
           n < outCap)
    {
        Accessoris a = {0};

        SQLGetData(stmt, 1, SQL_C_CHAR, a.AksesorisID,   sizeof(a.AksesorisID),   NULL);
        SQLGetData(stmt, 2, SQL_C_CHAR, a.NamaAksesoris, sizeof(a.NamaAksesoris), NULL);
        SQLGetData(stmt, 3, SQL_C_CHAR, a.MerkAksesoris, sizeof(a.MerkAksesoris), NULL);
        SQLGetData(stmt, 4, SQL_C_SLONG, &a.Stok, 0, NULL);
        SQLGetData(stmt, 5, SQL_C_SLONG, &a.Harga, 0, NULL);

        out[n++] = a;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    *outCount = n;
    return true;
}

/* =========================
   INSERT
========================= */

bool DbAccesoriss_Insert(void *dbcVoid,
                         const char *NamaAksesoris,
                         const char *MerkAksesoris,
                         int Stok,
                         int Harga)
{
    if (!dbcVoid)
        return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char namaEsc[128], merkEsc[128];
    EscapeSql(NamaAksesoris, namaEsc, sizeof(namaEsc));
    EscapeSql(MerkAksesoris, merkEsc, sizeof(merkEsc));

    char sql[512];
    snprintf(sql, sizeof(sql),
             "INSERT INTO dbo.Aksesoris (NamaAksesoris, MerkAksesoris, Stok, Harga) "
             "VALUES ('%s', '%s', %d, %d)",
             namaEsc, merkEsc, Stok, Harga);

    return ExecSQL(dbc, sql);
}

/* =========================
   UPDATE
========================= */

bool DbAccesoriss_Update(void *dbcVoid,
                         const char *AksesorisID,
                         const char *NamaAksesoris,
                         const char *MerkAksesoris,
                         int Stok,
                         int Harga)
{
    if (!dbcVoid || !AksesorisID)
        return false;

    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char idEsc[32], namaEsc[128], merkEsc[128];
    EscapeSql(AksesorisID, idEsc, sizeof(idEsc));
    EscapeSql(NamaAksesoris, namaEsc, sizeof(namaEsc));
    EscapeSql(MerkAksesoris, merkEsc, sizeof(merkEsc));

    char sql[512];
    snprintf(sql, sizeof(sql),
             "UPDATE dbo.Aksesoris SET "
             "NamaAksesoris='%s', MerkAksesoris='%s', Stok=%d, Harga=%d "
             "WHERE AksesorisID='%s'",
             namaEsc, merkEsc, Stok, Harga, idEsc);

    return ExecSQL(dbc, sql);
}

/* =========================
   DELETE
========================= */

bool DbAccesoriss_Delete(void *dbcVoid, const char *AksesorisID)
{
    if (!dbcVoid || !AksesorisID)
        return false;

    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char idEsc[32];
    EscapeSql(AksesorisID, idEsc, sizeof(idEsc));

    char sql[256];
    snprintf(sql, sizeof(sql),
             "DELETE FROM dbo.Aksesoris WHERE AksesorisID='%s'",
             idEsc);

    return ExecSQL(dbc, sql);
}
