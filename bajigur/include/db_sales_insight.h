#ifndef DB_SALES_INSIGHT_H
#define DB_SALES_INSIGHT_H

#include <stdbool.h>

typedef struct {
    int totalCarSold;    // total mobil terjual (success)
    int totalAccSold;    // total qty aksesoris terjual (success)
    double omzetCar;     // omzet mobil (success)
    double omzetAcc;     // omzet aksesoris (success)
    int successTx;       // total transaksi sukses (car tx + acc tx distinct)
} SalesInsightSummary;

typedef struct {
    char month[8];       // "YYYY-MM"
    int  tx;             // jumlah transaksi sukses (car + acc distinct)
    double omzet;        // omzet sukses (car + acc)
} SalesInsightMonth;

typedef struct {
    char name[64];
    int  tx;             // jumlah transaksi sukses
    double total;        // total penjualan sukses
} SalesInsightEmp;

typedef struct {
    char item[96];
    int  qty;            // qty terjual (mobil=unit, aksesoris=sum qty)
    double total;        // total penjualan sukses
} SalesInsightProd;

typedef struct {
    char SalesID[8];
    char SalesName[64];
    int  TxCount;
    double Revenue;
} SalesTopSalesRow;

// ===== API =====
bool DbSalesInsight_LoadSummary(void *dbcVoid, SalesInsightSummary *outSum);

int DbSalesInsight_LoadMonthly(void *dbcVoid, SalesInsightMonth *outArr, int cap);

// produk terlaris DIPISAH:
int DbSalesInsight_LoadBestCars(void *dbcVoid, SalesInsightProd *outArr, int cap);
int DbSalesInsight_LoadBestAccessories(void *dbcVoid, SalesInsightProd *outArr, int cap);

// top sales by transactions / revenue (tanpa filter tanggal)
int DbSalesInsight_LoadTopSalesByTx(void *dbcVoid, SalesInsightEmp *outArr, int cap);
int DbSalesInsight_LoadTopSalesByRevenueEmp(void *dbcVoid, SalesInsightEmp *outArr, int cap);

// return jumlah row (0..cap), -1 kalau error
int DbSalesInsight_LoadTopSalesByRevenue(void *dbc, const char *fromYMD, const char *toYMD,
                                        SalesTopSalesRow *out, int cap);

#endif
