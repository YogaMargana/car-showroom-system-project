#include "admin_sales_report.h"

#include "ui.h"
#include "textbox.h"
#include "db_sales_report.h"
#include "db_sales_insight.h"


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#define MAX_ROWS 2000
#define MAX_VIEW 2000

typedef enum {
    SORT_TANGGAL = 0,
    SORT_ID,
    SORT_TYPE,
    SORT_ITEM,
    SORT_CUSTOMER,
    SORT_EMPLOYEE,
    SORT_STATUS,
    SORT_QTY,
    SORT_TOTAL
} SortKey;

static SalesReportRow gRows[MAX_ROWS];
static int gRowCount = 0;

static int gViewIdx[MAX_VIEW];
static int gViewCount = 0;

static int gNeedReload = 1;
static int gScroll = 0;

// Report pager (bawah) -> 1 = transaksi, 2 = insight
static int gReportPage = 1;
static const int gReportPages = 2;

// input
static char gSearch[96] = "";
static char gFrom[16] = ""; // YYYY-MM-DD
static char gTo[16]   = "";

static UITextBox tbSearch;
static UITextBox tbFrom;
static UITextBox tbTo;
static int gUiInited = 0;

// sort
static SortKey gSortKey = SORT_TANGGAL;
static bool gSortAsc = false; // default terbaru dulu

// toast
static char gToast[160] = "";
static float gToastTimer = 0.0f;

static SalesTopSalesRow gTopSales[8];
static int gTopSalesCount = 0;

// ---------- helpers ----------
static int ClampInt(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void SetToast(const char *msg)
{
    strncpy(gToast, msg ? msg : "", sizeof(gToast) - 1);
    gToast[sizeof(gToast) - 1] = '\0';
    gToastTimer = 2.5f;
}

static void ToLowerCopy(const char *src, char *dst, int cap)
{
    if (!dst || cap <= 0) return;
    int j = 0;
    for (int i = 0; src && src[i] && j + 1 < cap; i++) {
        dst[j++] = (char)tolower((unsigned char)src[i]);
    }
    dst[j] = '\0';
}

static bool ContainsInsensitive(const char *hay, const char *needle)
{
    if (!needle || needle[0] == '\0') return true;
    if (!hay) return false;

    char h[256], n[128];
    ToLowerCopy(hay, h, (int)sizeof(h));
    ToLowerCopy(needle, n, (int)sizeof(n));
    return strstr(h, n) != NULL;
}

static bool StrEqI(const char *a, const char *b)
{
    if (!a) a = "";
    if (!b) b = "";
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

static bool IsSuccessStatus(const char *s)
{
    return StrEqI(s, "Success") || StrEqI(s, "Succeed");
}

static int ParseDateKey(const char *ymd)
{
    // return YYYYMMDD, atau -1 kalau kosong/invalid
    if (!ymd || ymd[0] == '\0') return -1;
    int y = 0, m = 0, d = 0;
    if (sscanf(ymd, "%d-%d-%d", &y, &m, &d) != 3) return -1;
    if (y < 1900 || y > 2100) return -1;
    if (m < 1 || m > 12) return -1;
    if (d < 1 || d > 31) return -1;
    return y * 10000 + m * 100 + d;
}

static int ParseYearMonthKey(const char *ymd)
{
    // return YYYYMM (contoh 202512), -1 invalid
    if (!ymd || ymd[0] == '\0') return -1;
    int y = 0, m = 0, d = 0;
    if (sscanf(ymd, "%d-%d-%d", &y, &m, &d) != 3) return -1;
    if (y < 1900 || y > 2100) return -1;
    if (m < 1 || m > 12) return -1;
    return y * 100 + m;
}

static int CmpStr(const char *a, const char *b)
{
    if (!a) a = "";
    if (!b) b = "";
    return strcmp(a, b);
}

// ---------- data load / build view ----------
static void ReloadRows(void *dbcPtr)
{
    void *dbc = dbcPtr ? *(void**)dbcPtr : NULL;
    int count = 0;

    if (DbSalesReport_LoadAll(dbc, gRows, MAX_ROWS, &count)) gRowCount = count;
    else gRowCount = 0;

    gScroll = 0;
}

static int CompareRow(const SalesReportRow *a, const SalesReportRow *b)
{
    int r = 0;
    switch (gSortKey) {
        case SORT_ID:        r = CmpStr(a->TransaksiID, b->TransaksiID); break;
        case SORT_TYPE:      r = CmpStr(a->Type, b->Type); break;
        case SORT_ITEM:      r = CmpStr(a->Item, b->Item); break;
        case SORT_CUSTOMER:  r = CmpStr(a->Customer, b->Customer); break;
        case SORT_EMPLOYEE:  r = CmpStr(a->Employee, b->Employee); break;
        case SORT_STATUS:    r = CmpStr(a->Status, b->Status); break;
        case SORT_QTY:       r = (a->Qty - b->Qty); break;
        case SORT_TOTAL:
            if (a->TotalVal < b->TotalVal) r = -1;
            else if (a->TotalVal > b->TotalVal) r = 1;
            else r = 0;
            break;
        case SORT_TANGGAL:
        default:
            // YYYY-MM-DD aman buat strcmp (lexicographic)
            r = CmpStr(a->Tanggal, b->Tanggal);
            break;
    }

    if (!gSortAsc) r = -r;

    // tie breaker biar stabil
    if (r == 0) {
        r = CmpStr(a->TransaksiID, b->TransaksiID);
        if (!gSortAsc) r = -r;
    }
    return r;
}

static void SortView(void)
{
    // insertion sort (<=2000 ok)
    for (int i = 1; i < gViewCount; i++) {
        int key = gViewIdx[i];
        int j = i - 1;
        while (j >= 0) {
            const SalesReportRow *A = &gRows[gViewIdx[j]];
            const SalesReportRow *B = &gRows[key];
            if (CompareRow(A, B) <= 0) break;
            gViewIdx[j + 1] = gViewIdx[j];
            j--;
        }
        gViewIdx[j + 1] = key;
    }
}

static void BuildView(void)
{
    int fromK = ParseDateKey(gFrom);
    int toK   = ParseDateKey(gTo);
    if (fromK != -1 && toK != -1 && fromK > toK) { int t = fromK; fromK = toK; toK = t; }

    gViewCount = 0;
    for (int i = 0; i < gRowCount && gViewCount < MAX_VIEW; i++) {
        const SalesReportRow *r = &gRows[i];

        int dk = ParseDateKey(r->Tanggal);
        if (fromK != -1 && dk != -1 && dk < fromK) continue;
        if (toK   != -1 && dk != -1 && dk > toK) continue;

        bool ok = true;
        if (gSearch[0] != '\0') {
            ok =
                ContainsInsensitive(r->Tanggal, gSearch) ||
                ContainsInsensitive(r->TransaksiID, gSearch) ||
                ContainsInsensitive(r->Type, gSearch) ||
                ContainsInsensitive(r->Item, gSearch) ||
                ContainsInsensitive(r->Customer, gSearch) ||
                ContainsInsensitive(r->Employee, gSearch) ||
                ContainsInsensitive(r->Status, gSearch);
        }
        if (!ok) continue;

        gViewIdx[gViewCount++] = i;
    }

    SortView();

    // clamp scroll biar ga “hilang”
    if (gScroll > gViewCount) gScroll = 0;
}

// ---------- UI pieces ----------
static bool HeaderSortCell(Rectangle r, const char *txt, SortKey key)
{
    bool hot = CheckCollisionPointRec(GetMousePosition(), r);

    Color bg = hot ? ((Color){220,220,220,255}) : ((Color){235,235,235,255});
    DrawRectangleRec(r, bg);
    DrawRectangleLines((int)r.x, (int)r.y, (int)r.width, (int)r.height, BLACK);

    char label[64];
    if (gSortKey == key) snprintf(label, sizeof(label), "%s %s", txt, gSortAsc ? "^" : "v");
    else snprintf(label, sizeof(label), "%s", txt);

    DrawText(label, (int)r.x + 6, (int)r.y + 6, 18, BLACK);

    if (hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (gSortKey == key) gSortAsc = !gSortAsc;
        else { gSortKey = key; gSortAsc = true; }
        return true;
    }
    return false;
}

static void DrawToast(void)
{
    if (gToastTimer <= 0.0f) return;

    float w = 520.0f;
    float h = 46.0f;
    float x = (GetScreenWidth() - w) / 2.0f;
    float y = 20.0f;

    DrawRectangleRounded((Rectangle){x, y, w, h}, 0.2f, 8, ((Color){20,20,20,220}));
    DrawText(gToast, (int)x + 14, (int)y + 14, 18, RAYWHITE);

    gToastTimer -= GetFrameTime();
    if (gToastTimer < 0.0f) gToastTimer = 0.0f;
}

static void DrawSummary(Rectangle area)
{
    int totalCount = gViewCount;
    int successCount = 0;
    double totalSuccess = 0.0;
    double totalAll = 0.0;

    for (int i = 0; i < gViewCount; i++) {
        const SalesReportRow *r = &gRows[gViewIdx[i]];
        totalAll += r->TotalVal;
        if (IsSuccessStatus(r->Status)) {
            successCount++;
            totalSuccess += r->TotalVal;
        }
    }

    float gap = 14.0f;
    float cardW = (area.width - 2 * gap) / 3.0f;
    float cardH = area.height;

    Rectangle c1 = { area.x, area.y, cardW, cardH };
    Rectangle c2 = { area.x + cardW + gap, area.y, cardW, cardH };
    Rectangle c3 = { area.x + (cardW + gap) * 2, area.y, cardW, cardH };

    DrawRectangleRounded(c1, 0.18f, 10, ((Color){245,245,245,255}));
    DrawRectangleRounded(c2, 0.18f, 10, ((Color){245,245,245,255}));
    DrawRectangleRounded(c3, 0.18f, 10, ((Color){245,245,245,255}));
    DrawRectangleRoundedLines(c1, 0.18f, 10, ((Color){40,40,40,255}));
    DrawRectangleRoundedLines(c2, 0.18f, 10, ((Color){40,40,40,255}));
    DrawRectangleRoundedLines(c3, 0.18f, 10, ((Color){40,40,40,255}));

    char buf[128];

    DrawText("Transaction (Filtered)", (int)c1.x + 14, (int)c1.y + 12, 18, BLACK);
    snprintf(buf, sizeof(buf), "%d", totalCount);
    DrawText(buf, (int)c1.x + 14, (int)c1.y + 42, 34, BLACK);

    DrawText("Succeed", (int)c2.x + 14, (int)c2.y + 12, 18, BLACK);
    snprintf(buf, sizeof(buf), "%d", successCount);
    DrawText(buf, (int)c2.x + 14, (int)c2.y + 42, 34, BLACK);

    DrawText("Total (succeed)", (int)c3.x + 14, (int)c3.y + 12, 18, BLACK);
    snprintf(buf, sizeof(buf), "%.0f", totalSuccess);
    DrawText(buf, (int)c3.x + 14, (int)c3.y + 42, 34, BLACK);

    snprintf(buf, sizeof(buf), "Total of All: %.0f", totalAll);
    DrawText(buf, (int)c3.x + 14, (int)c3.y + 82, 16, ((Color){60,60,60,255}));
}

// klik tombol custom (biar ga transparan & ga bergantung UIButton nge-draw)
static bool SolidButton(Rectangle r, const char *label, bool enabled)
{
    bool hot = CheckCollisionPointRec(GetMousePosition(), r);

    Color bg = enabled
        ? (hot ? ((Color){225,225,225,255}) : ((Color){235,235,235,255}))
        : ((Color){215,215,215,255});

    DrawRectangleRounded(r, 0.2f, 8, bg);
    DrawRectangleRoundedLines(r, 0.2f, 8, ((Color){40,40,40,255}));

    Color tc = enabled ? BLACK : ((Color){120,120,120,255});
    int fontSize = 18;
    int tw = MeasureText(label, fontSize);
    int tx = (int)(r.x + (r.width - tw) / 2);
    int ty = (int)(r.y + (r.height - fontSize) / 2);
    DrawText(label, tx, ty, fontSize, tc);

    if (!enabled) return false;
    return hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

static void DrawTable(Rectangle area)
{
    DrawRectangleRec(area, RAYWHITE);
    DrawRectangleLines((int)area.x, (int)area.y, (int)area.width, (int)area.height, BLACK);

    float pad = 8.0f;
    float x = area.x + pad;
    float y = area.y + pad;

    float wTgl   = 120;
    float wID    = 120;
    float wType  = 90;
    float wItem  = 180;
    float wCust  = 160;
    float wEmp   = 160;
    float wStat  = 110;
    float wQty   = 60;
    float wTotal = 120;

    float hHead = 32;

    Rectangle cT   = { x, y, wTgl, hHead }; x += wTgl;
    Rectangle cI   = { x, y, wID,  hHead }; x += wID;
    Rectangle cTy  = { x, y, wType, hHead }; x += wType;
    Rectangle cIt  = { x, y, wItem, hHead }; x += wItem;
    Rectangle cCu  = { x, y, wCust, hHead }; x += wCust;
    Rectangle cEm  = { x, y, wEmp,  hHead }; x += wEmp;
    Rectangle cSt  = { x, y, wStat, hHead }; x += wStat;
    Rectangle cQ   = { x, y, wQty,  hHead }; x += wQty;
    Rectangle cTo  = { x, y, wTotal, hHead };

    bool changed = false;
    changed |= HeaderSortCell(cT,  "Date",   SORT_TANGGAL);
    changed |= HeaderSortCell(cI,  "ID",     SORT_ID);
    changed |= HeaderSortCell(cTy, "Type",   SORT_TYPE);
    changed |= HeaderSortCell(cIt, "Item",   SORT_ITEM);
    changed |= HeaderSortCell(cCu, "Customer", SORT_CUSTOMER);
    changed |= HeaderSortCell(cEm, "Employee", SORT_EMPLOYEE);
    changed |= HeaderSortCell(cSt, "Status", SORT_STATUS);
    changed |= HeaderSortCell(cQ,  "Qty",    SORT_QTY);
    changed |= HeaderSortCell(cTo, "Total",  SORT_TOTAL);
    if (changed) { BuildView(); gScroll = 0; }

    int rowH = 26;
    float bodyY = y + hHead + 8;

    int visibleRows = (int)((area.y + area.height - bodyY - 10) / rowH);
    if (visibleRows < 1) visibleRows = 1;

    int maxScroll = gViewCount - visibleRows;
    if (maxScroll < 0) maxScroll = 0;
    gScroll = ClampInt(gScroll, 0, maxScroll);

    // scroll wheel
    if (CheckCollisionPointRec(GetMousePosition(), area)) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            gScroll -= (int)wheel;
            gScroll = ClampInt(gScroll, 0, maxScroll);
        }
    }

    for (int i = 0; i < visibleRows; i++) {
        int idx = gScroll + i;
        if (idx >= gViewCount) break;

        const SalesReportRow *r = &gRows[gViewIdx[idx]];
        float rowY = bodyY + i * rowH;

        if (i % 2 == 0) {
            DrawRectangle((int)area.x + 1, (int)rowY, (int)area.width - 2, rowH, ((Color){245,245,245,255}));
        }

        float cx = area.x + pad;

        DrawText(r->Tanggal,     (int)cx + 4, (int)rowY + 4, 18, BLACK); cx += wTgl;
        DrawText(r->TransaksiID, (int)cx + 4, (int)rowY + 4, 18, BLACK); cx += wID;
        DrawText(r->Type,        (int)cx + 4, (int)rowY + 4, 18, BLACK); cx += wType;
        DrawText(r->Item,        (int)cx + 4, (int)rowY + 4, 18, BLACK); cx += wItem;
        DrawText(r->Customer,    (int)cx + 4, (int)rowY + 4, 18, BLACK); cx += wCust;
        DrawText(r->Employee,    (int)cx + 4, (int)rowY + 4, 18, BLACK); cx += wEmp;
        DrawText(r->Status,      (int)cx + 4, (int)rowY + 4, 18, BLACK); cx += wStat;

        char buf[32];
        snprintf(buf, sizeof(buf), "%d", r->Qty);
        DrawText(buf, (int)cx + 4, (int)rowY + 4, 18, BLACK); cx += wQty;

        snprintf(buf, sizeof(buf), "%.0f", r->TotalVal);
        DrawText(buf, (int)cx + 4, (int)rowY + 4, 18, BLACK);
    }
}

static void ExportCSV(const char *prefix)
{
    time_t t = time(NULL);
    struct tm *ti = localtime(&t);

    char fname[128];
    snprintf(fname, sizeof(fname), "%s_%04d%02d%02d_%02d%02d%02d.csv",
             prefix,
             ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
             ti->tm_hour, ti->tm_min, ti->tm_sec);

    FILE *f = fopen(fname, "w");
    if (!f) { SetToast("Failed to create CSV file (check permission folder)."); return; }

    fprintf(f, "Date,ID,Type,Item,Customer,Employee,Status,Qty,Total\n");
    for (int i = 0; i < gViewCount; i++) {
        const SalesReportRow *r = &gRows[gViewIdx[i]];

        char item[256], cust[128], emp[128];
        strncpy(item, r->Item, sizeof(item) - 1); item[sizeof(item)-1] = '\0';
        strncpy(cust, r->Customer, sizeof(cust) - 1); cust[sizeof(cust)-1] = '\0';
        strncpy(emp,  r->Employee, sizeof(emp) - 1); emp[sizeof(emp)-1] = '\0';

        for (int j = 0; item[j]; j++) if (item[j] == ',') item[j] = ' ';
        for (int j = 0; cust[j]; j++) if (cust[j] == ',') cust[j] = ' ';
        for (int j = 0; emp[j]; j++)  if (emp[j] == ',')  emp[j] = ' ';

        fprintf(f, "%s,%s,%s,%s,%s,%s,%s,%d,%.0f\n",
                r->Tanggal, r->TransaksiID, r->Type, item,
                cust, emp, r->Status, r->Qty, r->TotalVal);
    }

    fclose(f);

    char msg[200];
    snprintf(msg, sizeof(msg), "CSV export successful: %s", fname);
    SetToast(msg);
}

static void DrawSalesReport_Page1(Rectangle contentArea)
{
    float yOff = -70.0f;

    Rectangle sum = { contentArea.x + 40, 225 + yOff, contentArea.width - 80, 110 };
    DrawSummary(sum);

    Rectangle table = { contentArea.x + 40, 350 + yOff, contentArea.width - 80, contentArea.height - 500 };
    DrawTable(table);
}

/* =========================
   PAGE 2: INSIGHTS
   ========================= */

#define MAX_MONTHS  64
#define MAX_EMP     64
#define MAX_PROD    128

typedef struct {
    int ym;              // YYYYMM
    int tx;              // jumlah transaksi (success)
    double omzet;        // total (success)
} MonthAgg;

typedef struct {
    char name[64];
    int tx;              // jumlah transaksi (success)
    double total;        // total penjualan (success)
} EmpAgg;

typedef struct {
    char item[96];
    int qty;             // total qty (success)
    double total;        // total penjualan (success)
} ProdAgg;

static void FormatYM(int ym, char *out, int cap)
{
    int y = ym / 100;
    int m = ym % 100;
    snprintf(out, cap, "%04d-%02d", y, m);
}

static void SortMonthAsc(MonthAgg *a, int n)
{
    for (int i = 1; i < n; i++) {
        MonthAgg key = a[i];
        int j = i - 1;
        while (j >= 0 && a[j].ym > key.ym) {
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = key;
    }
}

static void SortEmpByTxDesc(EmpAgg *a, int n)
{
    for (int i = 1; i < n; i++) {
        EmpAgg key = a[i];
        int j = i - 1;
        while (j >= 0) {
            if (a[j].tx > key.tx) break;
            if (a[j].tx == key.tx && a[j].total >= key.total) break;
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = key;
    }
}

static void SortProdByQtyDesc(ProdAgg *a, int n)
{
    for (int i = 1; i < n; i++) {
        ProdAgg key = a[i];
        int j = i - 1;
        while (j >= 0) {
            if (a[j].qty > key.qty) break;
            if (a[j].qty == key.qty && a[j].total >= key.total) break;
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = key;
    }
}

static void DrawCard(Rectangle r, const char *title, const char *value, const char *sub)
{
    DrawRectangleRounded(r, 0.18f, 10, ((Color){245,245,245,255}));
    DrawRectangleRoundedLines(r, 0.18f, 10, ((Color){40,40,40,255}));
    DrawText(title, (int)r.x + 14, (int)r.y + 12, 18, BLACK);
    DrawText(value, (int)r.x + 14, (int)r.y + 42, 34, BLACK);
    if (sub && sub[0]) DrawText(sub, (int)r.x + 14, (int)r.y + 82, 16, ((Color){60,60,60,255}));
}

static void DrawMiniHeader(Rectangle r, const char *title)
{
    DrawRectangleRounded(r, 0.15f, 8, ((Color){245,245,245,255}));
    DrawRectangleRoundedLines(r, 0.15f, 8, ((Color){40,40,40,255}));
    DrawText(title, (int)r.x + 12, (int)r.y + 10, 20, BLACK);
}

static void DrawSalesReport_Page2(Rectangle contentArea, void *dbcPtr)
{
    // Naikin/turunin konten insight DI SINI (pagination tetap)
    // makin negatif -> makin ke atas
    float yOff = -130.0f;

    void *dbc = dbcPtr ? *(void**)dbcPtr : NULL;

    // ====== pager position (HARUS sama dengan DrawReportPager) ======
    float btnW = 90, btnH = 34;
    float pagerY  = contentArea.y + contentArea.height - btnH - 160; // sama
    float pagerTop = pagerY - 8; // sama dengan bar: py - 8

    // ====== hitung area insight supaya berhenti sebelum pager ======
    float topY = (225 + yOff);        // start Y insight
    float marginBottom = 14.0f;       // jarak insight -> pager bar
    float areaH = (pagerTop - marginBottom) - topY;

    if (areaH < 240) areaH = 240;     // safety clamp

    Rectangle area = {
        contentArea.x + 40,
        topY,
        contentArea.width - 80,
        areaH
    };

    DrawRectangleRec(area, RAYWHITE);
    DrawRectangleLines((int)area.x, (int)area.y, (int)area.width, (int)area.height, BLACK);

    // ====== Load dari DB ======
    SalesInsightSummary sum;
    SalesInsightMonth  months[64];    int mCount = 0;
    SalesTopSalesRow   topSales[8];   int nTop = 0;
    SalesInsightProd   bestCar[8];    int nCar = 0;
    SalesInsightProd   bestAcc[8];    int nAcc = 0;

    bool okSum = DbSalesInsight_LoadSummary(dbc, &sum);
    mCount = DbSalesInsight_LoadMonthly(dbc, months, 64);

    // Top Sales = berdasarkan revenue, tapi ada TxCount juga
    // Filter ikut From/To (kalau kosong -> all time)
    nTop  = DbSalesInsight_LoadTopSalesByRevenue(dbc, gFrom, gTo, topSales, 8);

    nCar = DbSalesInsight_LoadBestCars(dbc, bestCar, 8);
    nAcc = DbSalesInsight_LoadBestAccessories(dbc, bestAcc, 8);

    if (!okSum) memset(&sum, 0, sizeof(sum));
    if (mCount < 0) mCount = 0;
    if (nTop < 0) nTop = 0;
    if (nCar < 0) nCar = 0;
    if (nAcc < 0) nAcc = 0;

    double omzetTotal = sum.omzetCar + sum.omzetAcc;

    // ====== Layout ======
    float pad = 16.0f;
    float x0 = area.x + pad;
    float y0 = area.y + pad;

    DrawText("Insights", (int)x0, (int)y0, 24, BLACK);
    y0 += 36;

    // ----- Cards: total car, total acc, total omzet -----
    float gap = 14.0f;
    float cardW = (area.width - pad*2 - gap*2) / 3.0f;
    float cardH = 110.0f;

    Rectangle c1 = { x0, y0, cardW, cardH };
    Rectangle c2 = { x0 + cardW + gap, y0, cardW, cardH };
    Rectangle c3 = { x0 + (cardW + gap)*2, y0, cardW, cardH };

    char v1[64], v2[64], v3[64], sub3[64];
    snprintf(v1, sizeof(v1), "%d", sum.totalCarSold);
    snprintf(v2, sizeof(v2), "%d", sum.totalAccSold);
    snprintf(v3, sizeof(v3), "%.0f", omzetTotal);
    snprintf(sub3, sizeof(sub3), "Success Tx: %d", sum.successTx);

    DrawCard(c1, "Total Cars Sold", v1, "");
    DrawCard(c2, "Total Accessories Sold", v2, "");
    DrawCard(c3, "Revenue (Success)", v3, sub3);

    y0 += cardH + 18;

    // ----- Row 2: Monthly (left) + Top Sales (right) -----
    float boxH = 210.0f;
    float boxW = (area.width - pad*2 - gap) / 2.0f;

    Rectangle bMonth = { x0, y0, boxW, boxH };
    Rectangle bTop   = { x0 + boxW + gap, y0, boxW, boxH };

    DrawMiniHeader(bMonth, "Monthly Sales (Success)");
    DrawMiniHeader(bTop,   "Top Sales (By Revenue)");

    // Monthly content
    {
        float tx = bMonth.x + 12;
        float ty = bMonth.y + 50;

        DrawText("Month",   (int)tx,         (int)ty, 18, BLACK);
        DrawText("Tx",      (int)(tx + 150), (int)ty, 18, BLACK);
        DrawText("Revenue", (int)(tx + 230), (int)ty, 18, BLACK);
        ty += 26;

        int show = (mCount > 6) ? 6 : mCount;
        for (int i = 0; i < show; i++) {
            char txc[16]; snprintf(txc, sizeof(txc), "%d", months[i].tx);
            char rev[32]; snprintf(rev, sizeof(rev), "%.0f", months[i].omzet);

            DrawText(months[i].month, (int)tx, (int)ty, 18, ((Color){40,40,40,255}));
            DrawText(txc, (int)(tx + 150), (int)ty, 18, ((Color){40,40,40,255}));
            DrawText(rev, (int)(tx + 230), (int)ty, 18, ((Color){40,40,40,255}));
            ty += 24;
        }
        if (mCount == 0) DrawText("No data.", (int)tx, (int)ty, 18, ((Color){80,80,80,255}));
    }

    // Top Sales content (Revenue desc) + tampilkan TxCount
    {
        float tx = bTop.x + 12;
        float ty = bTop.y + 50;

        DrawText("Sales",   (int)tx,         (int)ty, 18, BLACK);
        DrawText("Tx",      (int)(tx + 220), (int)ty, 18, BLACK);
        DrawText("Revenue", (int)(tx + 280), (int)ty, 18, BLACK);
        ty += 26;

        int show = (nTop > 6) ? 6 : nTop;
        for (int i = 0; i < show; i++) {
            char txc[16]; snprintf(txc, sizeof(txc), "%d", topSales[i].TxCount);
            char rev[32]; snprintf(rev, sizeof(rev), "%.0f", topSales[i].Revenue);

            const char *nm = topSales[i].SalesName[0] ? topSales[i].SalesName : topSales[i].SalesID;

            DrawText(nm,  (int)tx, (int)ty, 18, ((Color){40,40,40,255}));
            DrawText(txc, (int)(tx + 220), (int)ty, 18, ((Color){40,40,40,255}));
            DrawText(rev, (int)(tx + 280), (int)ty, 18, ((Color){40,40,40,255}));
            ty += 24;
        }
        if (nTop == 0) DrawText("No data.", (int)tx, (int)ty, 18, ((Color){80,80,80,255}));
    }

    y0 += boxH + 18;

    // ----- Row 3: Best Cars (left) + Best Accessories (right) -----
    Rectangle bCar = { x0, y0, boxW, 220.0f };
    Rectangle bAcc = { x0 + boxW + gap, y0, boxW, 220.0f };

    DrawMiniHeader(bCar, "Best-selling Cars (By Qty)");
    DrawMiniHeader(bAcc, "Best-selling Accessories (By Qty)");

    // Best-selling Cars
    {
        float tx = bCar.x + 12;
        float ty = bCar.y + 50;

        DrawText("Car",   (int)tx,         (int)ty, 18, BLACK);
        DrawText("Qty",   (int)(tx + 240), (int)ty, 18, BLACK);
        DrawText("Total", (int)(tx + 300), (int)ty, 18, BLACK);
        ty += 26;

        int show = (nCar > 6) ? 6 : nCar;
        for (int i = 0; i < show; i++) {
            char q[16]; snprintf(q, sizeof(q), "%d", bestCar[i].qty);
            char t[32]; snprintf(t, sizeof(t), "%.0f", bestCar[i].total);

            DrawText(bestCar[i].item, (int)tx, (int)ty, 18, ((Color){40,40,40,255}));
            DrawText(q,               (int)(tx + 240), (int)ty, 18, ((Color){40,40,40,255}));
            DrawText(t,               (int)(tx + 300), (int)ty, 18, ((Color){40,40,40,255}));
            ty += 24;
        }
        if (nCar == 0) DrawText("No data.", (int)tx, (int)ty, 18, ((Color){80,80,80,255}));
    }

    // Best-selling Accessories
    {
        float tx = bAcc.x + 12;
        float ty = bAcc.y + 50;

        DrawText("Accessory", (int)tx,         (int)ty, 18, BLACK);
        DrawText("Qty",       (int)(tx + 240), (int)ty, 18, BLACK);
        DrawText("Total",     (int)(tx + 300), (int)ty, 18, BLACK);
        ty += 26;

        int show = (nAcc > 6) ? 6 : nAcc;
        for (int i = 0; i < show; i++) {
            char q[16]; snprintf(q, sizeof(q), "%d", bestAcc[i].qty);
            char t[32]; snprintf(t, sizeof(t), "%.0f", bestAcc[i].total);

            DrawText(bestAcc[i].item, (int)tx, (int)ty, 18, ((Color){40,40,40,255}));
            DrawText(q,               (int)(tx + 240), (int)ty, 18, ((Color){40,40,40,255}));
            DrawText(t,               (int)(tx + 300), (int)ty, 18, ((Color){40,40,40,255}));
            ty += 24;
        }
        if (nAcc == 0) DrawText("No data.", (int)tx, (int)ty, 18, ((Color){80,80,80,255}));
    }
}

// pager bawah (pindah report page 1/2) -> SELALU kelihatan, ga ngilang
static void DrawReportPager(Rectangle contentArea)
{
    float btnW = 90, btnH = 34;

    // taruh di kanan bawah area konten
    float py = contentArea.y + contentArea.height - btnH - 160;
    float px = contentArea.x + contentArea.width - (btnW * 2 + 10 + 140) - 50;

    // background bar biar jelas
    Rectangle bar = { px - 10, py - 8, (btnW * 2 + 10 + 140) + 20, btnH + 16 };
    DrawRectangleRounded(bar, 0.15f, 8, ((Color){245,245,245,230}));
    DrawRectangleRoundedLines(bar, 0.15f, 8, ((Color){40,40,40,255}));

    bool canPrev = (gReportPage > 1);
    bool canNext = (gReportPage < gReportPages);

    Rectangle rPrev = { px, py, btnW, btnH };
    if (SolidButton(rPrev, "Prev", canPrev)) {
        gReportPage--;
        gScroll = 0;
    }

    px += btnW + 10;

    char pageText[64];
    snprintf(pageText, sizeof(pageText), "Page %d / %d", gReportPage, gReportPages);
    DrawText(pageText, (int)px, (int)py + 8, 18, BLACK);

    px += 140;

    Rectangle rNext = { px, py, btnW, btnH };
    if (SolidButton(rNext, "Next", canNext)) {
        gReportPage++;
        gScroll = 0;
    }
}

void AdminSalesReportPage(Rectangle contentArea, void *dbcPtr)
{
    float yOff = -70.0f;

    DrawText("Sales Reports", (int)contentArea.x + 40, (int)(100 + yOff), 28, WHITE);

    // init textbox (boleh tetap di-init sekali aja, aman)
    if (!gUiInited) {
        UITextBoxInit(&tbSearch, (Rectangle){ contentArea.x + 40,  (int)(170 + yOff), 380, 40 }, gSearch, (int)sizeof(gSearch), false);
        UITextBoxInit(&tbFrom,   (Rectangle){ contentArea.x + 440, (int)(170 + yOff), 140, 40 }, gFrom,   (int)sizeof(gFrom), false);
        UITextBoxInit(&tbTo,     (Rectangle){ contentArea.x + 600, (int)(170 + yOff), 140, 40 }, gTo,     (int)sizeof(gTo), false);
        gUiInited = 1;
    }

    static bool firstLoad = true;
    if (firstLoad) {
        ReloadRows(dbcPtr);
        BuildView();
        firstLoad = false;
    }

    if (gNeedReload) {
        ReloadRows(dbcPtr);
        gNeedReload = 0;
        BuildView();
    }

    if (gReportPage == 1) {
        DrawText("Search", (int)contentArea.x + 40, (int)(145 + yOff), 18, WHITE);
        UITextBoxUpdate(&tbSearch);
        UITextBoxDraw(&tbSearch, 18);

        DrawText("From", (int)contentArea.x + 440, (int)(145 + yOff), 18, WHITE);
        UITextBoxUpdate(&tbFrom);
        UITextBoxDraw(&tbFrom, 18);

        DrawText("To", (int)contentArea.x + 600, (int)(145 + yOff), 18, WHITE);
        UITextBoxUpdate(&tbTo);
        UITextBoxDraw(&tbTo, 18);

        if (IsKeyPressed(KEY_ENTER)) { BuildView(); gScroll = 0; }

        if (UIButton((Rectangle){ contentArea.x + 760, (int)(170 + yOff), 110, 40 }, "Apply", 18)) {
            if (gFrom[0] && ParseDateKey(gFrom) == -1) SetToast("Wrong from format. Use YYYY-MM-DD.");
            else if (gTo[0] && ParseDateKey(gTo) == -1) SetToast("Wrong to format. Use YYYY-MM-DD.");
            else { BuildView(); gScroll = 0; }
        }

        if (UIButton((Rectangle){ contentArea.x + 880, (int)(170 + yOff), 110, 40 }, "Refresh", 18)) {
            gNeedReload = 1;
        }

        if (UIButton((Rectangle){ contentArea.x + 1000, (int)(170 + yOff), 140, 40 }, "Export CSV", 18)) {
            ExportCSV("sales_report");
        }
    }

    // --- page content ---
    if (gReportPage == 1) DrawSalesReport_Page1(contentArea);
    else DrawSalesReport_Page2(contentArea, dbcPtr); // kalau page2 butuh dbcPtr

    // --- report pager bottom (HANYA INI pagination) ---
    DrawReportPager(contentArea);

    DrawToast();
}
