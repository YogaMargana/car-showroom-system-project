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
    if (!dst || dstSize == 0) return;
    if (!src) { dst[0] = '\0'; return; }

    for (size_t i = 0; src[i] && j + 1 < dstSize; i++)
    {
        if (src[i] == '\'')
        {
            if (j + 2 < dstSize)
            {
                dst[j++] = '\'';
                dst[j++] = '\'';
            }
            else break;
        }
        else
        {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

static void PrintOdbcError(SQLSMALLINT handleType, SQLHANDLE handle)
{
    SQLCHAR state[6] = {0}, msg[256] = {0};
    SQLINTEGER native = 0;
    SQLSMALLINT len = 0;

    if (SQLGetDiagRec(handleType, handle, 1, state, &native, msg, sizeof(msg), &len) == SQL_SUCCESS)
    {
        printf("ODBC Error [%s] %ld: %s\n", state, (long)native, msg);
    }
}

static bool ExecDirect(SQLHDBC dbc, const char *sql)
{
    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt)))
        return false;

    SQLRETURN r = SQLExecDirect(stmt, (SQLCHAR *)sql, SQL_NTS);
    if (!SQL_SUCCEEDED(r))
        PrintOdbcError(SQL_HANDLE_STMT, stmt);

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return SQL_SUCCEEDED(r);
}

bool DbCarData_LoadAll(void *dbcVoid, Car *out, int outCap, int *outCount)
{
    if (outCount) *outCount = 0;
    if (!dbcVoid || !out || outCap <= 0 || !outCount) return false;

    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    const char *sql =
        "SELECT MobilID, NamaMobil, "
        "CAST(TahunProduksi AS VARCHAR(8)) AS TahunProduksi, "
        "CAST(Harga AS VARCHAR(32)) AS Harga, "
        "CAST(Stok  AS VARCHAR(10)) AS Stok "
        "FROM dbo.Mobil ORDER BY ID";

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt)))
        return false;

    SQLRETURN r = SQLExecDirect(stmt, (SQLCHAR *)sql, SQL_NTS);
    if (!SQL_SUCCEEDED(r))
    {
        PrintOdbcError(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    int n = 0;
    SQLRETURN fr;
    while ((fr = SQLFetch(stmt)) != SQL_NO_DATA && SQL_SUCCEEDED(fr) && n < outCap)
    {
        SQLGetData(stmt, 1, SQL_C_CHAR, out[n].MobilID,       sizeof(out[n].MobilID),       NULL);
        SQLGetData(stmt, 2, SQL_C_CHAR, out[n].NamaMobil,     sizeof(out[n].NamaMobil),     NULL);
        SQLGetData(stmt, 3, SQL_C_CHAR, out[n].TahunProduksi, sizeof(out[n].TahunProduksi), NULL);
        SQLGetData(stmt, 4, SQL_C_CHAR, out[n].Harga,         sizeof(out[n].Harga),         NULL);
        SQLGetData(stmt, 5, SQL_C_CHAR, out[n].Stok,          sizeof(out[n].Stok),          NULL);
        n++;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    *outCount = n;
    return true;
}

bool DbCarData_Insert(void *dbcVoid,
                      const char *NamaMobil,
                      const char *TahunProduksi,
                      const char *Harga,
                      const char *Stok)
{
    SQLHDBC dbc = (SQLHDBC)dbcVoid;
    if (!dbc || !NamaMobil || NamaMobil[0] == '\0') return false;

    /* NOTE:
       Kolom TipeMobil pada dbo.Mobil adalah NOT NULL (sesuai revisi DB kamu).
       Karena fungsi ini tidak menerima tipe, kita isi default 'N/A'.
       Untuk CRUD Mobil yang lengkap, sebaiknya pakai db_cars.c (DbCars_Insert) yang punya parameter tipe.
    */
    char nmE[128], thE[32], hgE[64], skE[32];
    EscapeSql(NamaMobil, nmE, sizeof(nmE));
    EscapeSql(TahunProduksi ? TahunProduksi : "0", thE, sizeof(thE));
    EscapeSql(Harga ? Harga : "0", hgE, sizeof(hgE));
    EscapeSql(Stok ? Stok : "0", skE, sizeof(skE));

    char sql[512];
    snprintf(sql, sizeof(sql),
             "INSERT INTO dbo.Mobil (NamaMobil, TipeMobil, Stok, TahunProduksi, Harga) "
             "VALUES ('%s','N/A', %s, %s, %s)",
             nmE, skE, thE, hgE);

    return ExecDirect(dbc, sql);
}

bool DbCarData_Update(void *dbcVoid,
                      const char *MobilID,
                      const char *NamaMobil,
                      const char *TahunProduksi,
                      const char *Harga,
                      const char *Stok)
{
    SQLHDBC dbc = (SQLHDBC)dbcVoid;
    if (!dbc || !MobilID || MobilID[0] == '\0') return false;

    char idE[32], nmE[128], thE[32], hgE[64], skE[32];
    EscapeSql(MobilID, idE, sizeof(idE));
    EscapeSql(NamaMobil ? NamaMobil : "", nmE, sizeof(nmE));
    EscapeSql(TahunProduksi ? TahunProduksi : "0", thE, sizeof(thE));
    EscapeSql(Harga ? Harga : "0", hgE, sizeof(hgE));
    EscapeSql(Stok ? Stok : "0", skE, sizeof(skE));

    char sql[600];
    /* TipeMobil tidak diubah di sini (fungsi ini legacy). */
    snprintf(sql, sizeof(sql),
             "UPDATE dbo.Mobil SET NamaMobil='%s', Stok=%s, TahunProduksi=%s, Harga=%s "
             "WHERE MobilID='%s'",
             nmE, skE, thE, hgE, idE);

    return ExecDirect(dbc, sql);
}

bool DbCarData_Delete(void *dbcVoid, const char *MobilID)
{
    SQLHDBC dbc = (SQLHDBC)dbcVoid;
    if (!dbc || !MobilID || MobilID[0] == '\0') return false;

    char idE[32];
    EscapeSql(MobilID, idE, sizeof(idE));

    char sql[256];
    snprintf(sql, sizeof(sql),
             "DELETE FROM dbo.Mobil WHERE MobilID='%s'", idE);

    return ExecDirect(dbc, sql);
}
