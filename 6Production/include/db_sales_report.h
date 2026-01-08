#ifndef DB_SALES_REPORT_H
#define DB_SALES_REPORT_H

#include <stdbool.h>

typedef struct {
    char Tanggal[16];      // YYYY-MM-DD
    char TransaksiID[32];  // PM00001 / PA00021
    char Type[16];         // Car / Accessory
    char Item[96];         // NamaMobil / NamaAksesoris
    char Customer[64];
    char Employee[64];
    char Status[24];       // Success / Failed
    int  Qty;              // 1 untuk mobil, qty untuk aksesoris
    double Total;
    double TotalVal;
} SalesReportRow;

bool DbSalesReport_LoadAll(void *dbcVoid, SalesReportRow *out, int outCap, int *outCount);
int  DbSalesReport_Count(void *dbcPtr);
int  DbSalesReport_LoadPage(void *dbcPtr, int page, int pageSize, SalesReportRow *outRows, int maxRows);

#endif
