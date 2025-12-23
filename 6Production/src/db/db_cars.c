#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "db_cars.h"

#include <windows.h>
#include <sql.h>
#include <sqlext.h>

#include <stdio.h>
#include <string.h>

static void EscapeSql(const char *src, char *dst, size_t dstSize)
{
    size_t j = 0;
    for (size_t i = 0; src && src[i] && j + 1 < dstSize; i++) {
        if (src[i] == '\'') {
            if (j + 2 < dstSize) {
                dst[j++] = '\'';
                dst[j++] = '\'';
            } else {
                break;
            }
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

    SQLRETURN r = SQLExecDirect(stmt, (SQLCHAR*)sql, SQL_NTS);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return SQL_SUCCEEDED(r);
}

bool DbCars_LoadAll(void *dbcVoid, CarData *out, int outCap, int *outCount)
{
    if (outCount) *outCount = 0;
    if (!dbcVoid || !out || outCap <= 0 || !outCount) return false;

    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    const char *sql =
        "SELECT MobilID, NamaMobil, "
        "CAST(Stok AS VARCHAR(8)) AS Stok, "
        "CAST(TahunProduksi AS VARCHAR(8)) AS TahunProduksi, "
        "CAST(Harga AS VARCHAR(32)) AS Harga "
        "FROM dbo.Mobil ORDER BY ID";

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;

    if (!SQL_SUCCEEDED(SQLExecDirect(stmt, (SQLCHAR*)sql, SQL_NTS))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    int n = 0;
    SQLRETURN fr;
    while ((fr = SQLFetch(stmt)) != SQL_NO_DATA && SQL_SUCCEEDED(fr) && n < outCap) {
        CarData c = {0};

        SQLGetData(stmt, 1, SQL_C_CHAR, c.MobilID,       sizeof(c.MobilID),       NULL);
        SQLGetData(stmt, 2, SQL_C_CHAR, c.NamaMobil,     sizeof(c.NamaMobil),     NULL);
        SQLGetData(stmt, 3, SQL_C_CHAR, c.Stok,          sizeof(c.Stok),          NULL);
        SQLGetData(stmt, 4, SQL_C_CHAR, c.TahunProduksi, sizeof(c.TahunProduksi), NULL);
        SQLGetData(stmt, 5, SQL_C_CHAR, c.Harga,         sizeof(c.Harga),         NULL);

        out[n++] = c;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    *outCount = n;
    return true;
}

bool DbCars_Insert(void *dbcVoid,
                   const char *nama,
                   const char *stok,
                   const char *tahunProduksi,
                   const char *harga)
{
    if (!dbcVoid) return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char namaE[64], stokE[16], tahunE[16], hargaE[32];
    EscapeSql(nama ? nama : "", namaE, sizeof(namaE));
    EscapeSql(stok ? stok : "", stokE, sizeof(stokE));
    EscapeSql(tahunProduksi ? tahunProduksi : "", tahunE, sizeof(tahunE));
    EscapeSql(harga ? harga : "", hargaE, sizeof(hargaE));

    char sql[1024];
    snprintf(sql, sizeof(sql),
             "INSERT INTO dbo.Mobil (NamaMobil, Stok, TahunProduksi, Harga) "
             "VALUES ('%s','%s','%s','%s')",
             namaE, stokE, tahunE, hargaE);

    return ExecSQL(dbc, sql);
}

bool DbCars_Update(void *dbcVoid,
                   const char *mobilId,
                   const char *nama,
                   const char *stok,
                   const char *tahunProduksi,
                   const char *harga)
{
    if (!dbcVoid || !mobilId) return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char idE[32], namaE[64], stokE[16], tahunE[16], hargaE[32];
    EscapeSql(mobilId, idE, sizeof(idE));
    EscapeSql(nama ? nama : "", namaE, sizeof(namaE));
    EscapeSql(stok ? stok : "", stokE, sizeof(stokE));
    EscapeSql(tahunProduksi ? tahunProduksi : "", tahunE, sizeof(tahunE));
    EscapeSql(harga ? harga : "", hargaE, sizeof(hargaE));

    char sql[1200];
    snprintf(sql, sizeof(sql),
             "UPDATE dbo.Mobil SET "
             "NamaMobil='%s', Stok='%s', TahunProduksi='%s', Harga='%s' "
             "WHERE MobilID='%s'",
             namaE, stokE, tahunE, hargaE, idE);

    return ExecSQL(dbc, sql);
}

bool DbCars_Delete(void *dbcVoid, const char *mobilId)
{
    if (!dbcVoid || !mobilId) return false;
    SQLHDBC dbc = (SQLHDBC)dbcVoid;

    char idE[32];
    EscapeSql(mobilId, idE, sizeof(idE));

    char sql[256];
    snprintf(sql, sizeof(sql),
             "DELETE FROM dbo.Mobil WHERE MobilID='%s'",
             idE);

    return ExecSQL(dbc, sql);
}
