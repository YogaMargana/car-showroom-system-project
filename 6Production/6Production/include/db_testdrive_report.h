#ifndef DB_TESTDRIVE_REPORT_H
#define DB_TESTDRIVE_REPORT_H

#include <stdbool.h>

typedef struct {
    char Tanggal[16];       // YYYY-MM-DD
    char TestDriveID[16];   // TD00001
    char Mobil[96];         // NamaMobil (atau Nama + Tipe)
    char Customer[64];
    char Sales[64];
    char Status[16];        // Scheduled/Ongoing/Canceled/Finished
} TestDriveRow;

// ===== Insights structs =====
typedef struct {
    int totalScheduled;
    int totalOngoing;
    int totalCanceled;
    int totalFinished;

    int totalConverted;         // finished -> bought within window
    double conversionRatePct;   // converted / finished * 100
} TestDriveInsightSummary;

typedef struct {
    char MobilID[16];
    char Mobil[96];
    int totalTestDrive;
    int finishedCount;
} TestDriveMostCarRow;

typedef struct {
    char SalesID[8];
    char SalesName[64];

    int totalTestDrive;
    int finishedTestDrive;
    int canceledTestDrive;

    int convertedCount;
    double conversionRatePct;
    double revenueFromConverted;
} TestDriveSalesPerfRow;

// ===== Page 1: history list =====
bool DbTestDriveReport_LoadAll(void *dbcVoid, TestDriveRow *out, int outCap, int *outCount);

// ===== Page 2: insights =====
bool DbTestDriveInsight_LoadSummary(void *dbcVoid, TestDriveInsightSummary *out, const char *fromYmd, const char *toYmd);
int  DbTestDriveInsight_LoadMostTestDrivenCars(void *dbcVoid, TestDriveMostCarRow *out, int cap, const char *fromYmd, const char *toYmd, const char *search, int topN);
int  DbTestDriveInsight_LoadSalesPerformance(void *dbcVoid, TestDriveSalesPerfRow *out, int cap, const char *fromYmd, const char *toYmd, int topN);

#endif
