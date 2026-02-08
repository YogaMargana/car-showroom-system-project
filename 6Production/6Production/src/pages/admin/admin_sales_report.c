#include "admin_sales_report.h"

#include "ui.h"
#include "textbox.h"
#include "db_sales_report.h"
#include "datepicker.h"
#include "xlsx_export.h"
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

// previous filter snapshot (for auto-filter on typing)
static char gPrevSearch[96] = "";
static char gPrevFrom[16] = "";
static char gPrevTo[16] = "";

static UITextBox tbSearch;
static UITextBox tbFrom;
static UITextBox tbTo;
static UIDatePicker dpFrom;
static UIDatePicker dpTo;
static int gUiInited = 0;

// sort
static SortKey gSortKey = SORT_TANGGAL;
static bool gSortAsc = false; // default terbaru dulu

// toast
static char gToast[160] = "";
static float gToastTimer = 0.0f;

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
    return StrEqI(s, "Success") || StrEqI(s, "Succeed") || StrEqI(s, "Berhasil");
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

static int CmpStr(const char *a, const char *b)
{
    if (!a) a = "";
    if (!b) b = "";
    return strcmp(a, b);
}

// format uang: double -> long long (pembulatan) -> "Rp x.xxx"
static void FormatRupiahFromDouble(double v, char *out, int outSize)
{
    long long val = (long long)((v >= 0.0) ? (v + 0.5) : (v - 0.5));
    UIFormatRupiahLL(val, out, outSize);
}

// ---------- data load / build view ----------
static void ReloadRows(void *dbcPtr)
{
    void *dbc = dbcPtr ? *(void**)dbcPtr : NULL;
    int count = 0;

    if (DbSalesReport_LoadAll(dbc, gRows, MAX_ROWS, &count)) {
        gRowCount = count;
        // Bajigur view/report tidak mengembalikan status, jadi anggap semua transaksi di report = Berhasil.
        for (int i = 0; i < gRowCount; i++) {
            if (gRows[i].Status[0] == '\0') {
                strncpy(gRows[i].Status, "Berhasil", sizeof(gRows[i].Status)-1);
                gRows[i].Status[sizeof(gRows[i].Status)-1] = '\0';
            }
        }
    } else {
        gRowCount = 0;
    }

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
            char qtyBuf[32];
            char totalBuf[48];
            char totalValBuf[48];
            snprintf(qtyBuf, sizeof(qtyBuf), "%d", r->Qty);
            snprintf(totalBuf, sizeof(totalBuf), "%.2f", r->Total);
            snprintf(totalValBuf, sizeof(totalValBuf), "%.2f", r->TotalVal);

            ok =
                ContainsInsensitive(r->Tanggal, gSearch) ||
                ContainsInsensitive(r->TransaksiID, gSearch) ||
                ContainsInsensitive(r->Type, gSearch) ||
                ContainsInsensitive(r->Item, gSearch) ||
                ContainsInsensitive(r->Customer, gSearch) ||
                ContainsInsensitive(r->Employee, gSearch) ||
                ContainsInsensitive(r->Status, gSearch) ||
                ContainsInsensitive(qtyBuf, gSearch) ||
                ContainsInsensitive(totalBuf, gSearch) ||
                ContainsInsensitive(totalValBuf, gSearch);
        }
        if (!ok) continue;

        gViewIdx[gViewCount++] = i;
    }

    SortView();

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
    char rp[64];

    DrawText("Total Transaksi", (int)c1.x + 14, (int)c1.y + 12, 18, BLACK);
    snprintf(buf, sizeof(buf), "%d", totalCount);
    DrawText(buf, (int)c1.x + 14, (int)c1.y + 42, 34, BLACK);

    DrawText("Succeed", (int)c2.x + 14, (int)c2.y + 12, 18, BLACK);
    snprintf(buf, sizeof(buf), "%d", successCount);
    DrawText(buf, (int)c2.x + 14, (int)c2.y + 42, 34, BLACK);

    DrawText("Total (succeed)", (int)c3.x + 14, (int)c3.y + 12, 18, BLACK);
    FormatRupiahFromDouble(totalSuccess, rp, (int)sizeof(rp));
    DrawText(rp, (int)c3.x + 14, (int)c3.y + 42, 30, BLACK);

    FormatRupiahFromDouble(totalAll, rp, (int)sizeof(rp));
    snprintf(buf, sizeof(buf), "Total of All: %s", rp);
    DrawText(buf, (int)c3.x + 14, (int)c3.y + 82, 16, ((Color){60,60,60,255}));
}

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
    float wStat  = 100;
    float wQty   = 50;
    float wTotal = 130; // lebih lebar karena ada "Rp ..."

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
    changed |= HeaderSortCell(cT,  "Tanggal",   SORT_TANGGAL);
    changed |= HeaderSortCell(cI,  "ID",     SORT_ID);
    changed |= HeaderSortCell(cTy, "Jenis",   SORT_TYPE);
    changed |= HeaderSortCell(cIt, "Item",   SORT_ITEM);
    changed |= HeaderSortCell(cCu, "Pelanggan", SORT_CUSTOMER);
    changed |= HeaderSortCell(cEm, "Karyawan", SORT_EMPLOYEE);
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

        char rp[64];
        FormatRupiahFromDouble(r->TotalVal, rp, (int)sizeof(rp));
        DrawText(rp, (int)cx + 4, (int)rowY + 4, 18, BLACK);
    }
}

static void ExportXLSX(const char *prefix)
{
    time_t t = time(NULL);
    struct tm *ti = localtime(&t);

    char fname[128];
    snprintf(fname, sizeof(fname), "%s_%04d%02d%02d_%02d%02d%02d.xlsx",
             prefix,
             ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
             ti->tm_hour, ti->tm_min, ti->tm_sec);

    XlsxBook *b = Xlsx_Create(fname, "Sales");
    if (!b) { SetToast("Failed to create XLSX file (check permission folder)."); return; }

    // header
    Xlsx_WriteString(b, 0, 0, "Date");
    Xlsx_WriteString(b, 0, 1, "ID");
    Xlsx_WriteString(b, 0, 2, "Type");
    Xlsx_WriteString(b, 0, 3, "Item");
    Xlsx_WriteString(b, 0, 4, "Customer");
    Xlsx_WriteString(b, 0, 5, "Employee");
    Xlsx_WriteString(b, 0, 6, "Status");
    Xlsx_WriteString(b, 0, 7, "Qty");
    Xlsx_WriteString(b, 0, 8, "Total");

    for (int i = 0; i < gViewCount; i++) {
        const SalesReportRow *r = &gRows[gViewIdx[i]];
        int row = i + 1;
        Xlsx_WriteString(b, row, 0, r->Tanggal);
        Xlsx_WriteString(b, row, 1, r->TransaksiID);
        Xlsx_WriteString(b, row, 2, r->Type);
        Xlsx_WriteString(b, row, 3, r->Item);
        Xlsx_WriteString(b, row, 4, r->Customer);
        Xlsx_WriteString(b, row, 5, r->Employee);
        Xlsx_WriteString(b, row, 6, r->Status);
        Xlsx_WriteNumber(b, row, 7, (double)r->Qty);
        Xlsx_WriteNumber(b, row, 8, r->TotalVal);
    }

    if (!Xlsx_Close(b)) {
        SetToast("Failed to finalize XLSX file.");
        return;
    }

    char msg[200];
    snprintf(msg, sizeof(msg), "Eksport xlsx Berhasil !: %s", fname);
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

static void DrawMobild(Rectangle r, const char *title, const char *value, const char *sub)
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
    float yOff = -130.0f;

    void *dbc = dbcPtr ? *(void**)dbcPtr : NULL;

    float btnW = 90, btnH = 34;
    float pagerY  = contentArea.y + contentArea.height - btnH - 160;
    float pagerTop = pagerY - 8;

    float topY = (225 + yOff);
    float marginBottom = 14.0f;
    float areaH = (pagerTop - marginBottom) - topY;
    if (areaH < 240) areaH = 240;

    Rectangle area = {
        contentArea.x + 40,
        topY,
        contentArea.width - 80,
        areaH
    };

    DrawRectangleRec(area, RAYWHITE);
    DrawRectangleLines((int)area.x, (int)area.y, (int)area.width, (int)area.height, BLACK);

    SalesInsightSummary sum;
    SalesInsightMonth  months[64];    int mCount = 0;
    SalesTopSalesRow   topSales[8];   int nTop = 0;
    SalesInsightProd   bestMobil[8];    int nMobil = 0;
    SalesInsightProd   bestAcc[8];    int nAcc = 0;

    bool okSum = DbSalesInsight_LoadSummary(dbc, &sum);
    mCount = DbSalesInsight_LoadMonthly(dbc, months, 64);

    // ikut filter From/To (kalau kosong -> all time)
    nTop  = DbSalesInsight_LoadTopSalesByRevenue(dbc, gFrom, gTo, topSales, 8);

    nMobil = DbSalesInsight_LoadBestCars(dbc, bestMobil, 8);
    nAcc = DbSalesInsight_LoadBestAccessories(dbc, bestAcc, 8);

    if (!okSum) memset(&sum, 0, sizeof(sum));
    if (mCount < 0) mCount = 0;
    if (nTop < 0) nTop = 0;
    if (nMobil < 0) nMobil = 0;
    if (nAcc < 0) nAcc = 0;

    double omzetTotal = sum.omzetCar + sum.omzetAcc;

    float pad = 16.0f;
    float x0 = area.x + pad;
    float y0 = area.y + pad;

    DrawText("Insight", (int)x0, (int)y0, 24, BLACK);
    y0 += 36;

    float gap = 14.0f;
    float cardW = (area.width - pad*2 - gap*2) / 3.0f;
    float cardH = 110.0f;

    Rectangle c1 = { x0, y0, cardW, cardH };
    Rectangle c2 = { x0 + cardW + gap, y0, cardW, cardH };
    Rectangle c3 = { x0 + (cardW + gap)*2, y0, cardW, cardH };

    char v1[64], v2[64], v3[64], sub3[64];
    char rpOmzet[64];

    snprintf(v1, sizeof(v1), "%d", sum.totalCarSold);
    snprintf(v2, sizeof(v2), "%d", sum.totalAccSold);
    FormatRupiahFromDouble(omzetTotal, rpOmzet, (int)sizeof(rpOmzet));
    snprintf(v3, sizeof(v3), "%s", rpOmzet);
    snprintf(sub3, sizeof(sub3), "Success Tx: %d", sum.successTx);

    DrawMobild(c1, "Total Mobil Terjual", v1, "");
    DrawMobild(c2, "Total Aksesoris Terjual", v2, "");
    DrawMobild(c3, "Omzet (Berhasil)", v3, sub3);

    y0 += cardH + 18;

    float boxH = 210.0f;
    float boxW = (area.width - pad*2 - gap) / 2.0f;

    Rectangle bBulan = { x0, y0, boxW, boxH };
    Rectangle bTop   = { x0 + boxW + gap, y0, boxW, boxH };

    DrawMiniHeader(bBulan, "Penjualan Bulanan (Berhasil)");
    DrawMiniHeader(bTop,   "Top Sales (By Omzet)");

    // Bulanly content
    {
        float tx = bBulan.x + 12;
        float ty = bBulan.y + 50;

        DrawText("Bulan",   (int)tx,         (int)ty, 18, BLACK);
        DrawText("Tx",      (int)(tx + 150), (int)ty, 18, BLACK);
        DrawText("Omzet", (int)(tx + 230), (int)ty, 18, BLACK);
        ty += 26;

        int show = (mCount > 6) ? 6 : mCount;
        for (int i = 0; i < show; i++) {
            char txc[16]; snprintf(txc, sizeof(txc), "%d", months[i].tx);
            char rev[64]; FormatRupiahFromDouble(months[i].omzet, rev, (int)sizeof(rev));

            DrawText(months[i].month, (int)tx, (int)ty, 18, ((Color){40,40,40,255}));
            DrawText(txc, (int)(tx + 150), (int)ty, 18, ((Color){40,40,40,255}));
            DrawText(rev, (int)(tx + 230), (int)ty, 18, ((Color){40,40,40,255}));
            ty += 24;
        }
        if (mCount == 0) DrawText("Tidak ada data.", (int)tx, (int)ty, 18, ((Color){80,80,80,255}));
    }

    // Top Sales content (Omzet desc) + tampilkan TxCount
    {
        float tx = bTop.x + 12;
        float ty = bTop.y + 50;

        DrawText("Sales",   (int)tx,         (int)ty, 18, BLACK);
        DrawText("Tx",      (int)(tx + 220), (int)ty, 18, BLACK);
        DrawText("Omzet", (int)(tx + 280), (int)ty, 18, BLACK);
        ty += 26;

        int show = (nTop > 5) ? 5 : nTop;
        for (int i = 0; i < show; i++) {
            char txc[16]; snprintf(txc, sizeof(txc), "%d", topSales[i].TxCount);
            char rev[64]; FormatRupiahFromDouble(topSales[i].Revenue, rev, (int)sizeof(rev));

            const char *nm = topSales[i].SalesName[0] ? topSales[i].SalesName : topSales[i].SalesID;

            DrawText(nm,  (int)tx, (int)ty, 18, ((Color){40,40,40,255}));
            DrawText(txc, (int)(tx + 220), (int)ty, 18, ((Color){40,40,40,255}));
            DrawText(rev, (int)(tx + 280), (int)ty, 18, ((Color){40,40,40,255}));
            ty += 24;
        }
        if (nTop == 0) DrawText("Tidak ada data.", (int)tx, (int)ty, 18, ((Color){80,80,80,255}));
    }

    y0 += boxH + 18;

    Rectangle bMobil = { x0, y0, boxW, 220.0f };
    Rectangle bAcc = { x0 + boxW + gap, y0, boxW, 220.0f };

    DrawMiniHeader(bMobil, "Best-selling Mobils (By Qty)");
    DrawMiniHeader(bAcc, "Best-selling Accessories (By Qty)");

    // Best-selling Mobils
    {
        float tx = bMobil.x + 12;
        float ty = bMobil.y + 50;

        DrawText("Mobil",   (int)tx,         (int)ty, 18, BLACK);
        DrawText("Qty",   (int)(tx + 240), (int)ty, 18, BLACK);
        DrawText("Total", (int)(tx + 300), (int)ty, 18, BLACK);
        ty += 26;

        int show = (nMobil > 6) ? 6 : nMobil;
        for (int i = 0; i < show; i++) {
            char q[16]; snprintf(q, sizeof(q), "%d", bestMobil[i].qty);
            char t[64]; FormatRupiahFromDouble(bestMobil[i].total, t, (int)sizeof(t));

            DrawText(bestMobil[i].item, (int)tx, (int)ty, 18, ((Color){40,40,40,255}));
            DrawText(q,               (int)(tx + 240), (int)ty, 18, ((Color){40,40,40,255}));
            DrawText(t,               (int)(tx + 300), (int)ty, 18, ((Color){40,40,40,255}));
            ty += 24;
        }
        if (nMobil == 0) DrawText("Tidak ada data.", (int)tx, (int)ty, 18, ((Color){80,80,80,255}));
    }

    // Best-selling Accessories
    {
        float tx = bAcc.x + 12;
        float ty = bAcc.y + 50;

        DrawText("Aksesoris", (int)tx,         (int)ty, 18, BLACK);
        DrawText("Qty",       (int)(tx + 240), (int)ty, 18, BLACK);
        DrawText("Total",     (int)(tx + 300), (int)ty, 18, BLACK);
        ty += 26;

        int show = (nAcc > 6) ? 6 : nAcc;
        for (int i = 0; i < show; i++) {
            char q[16]; snprintf(q, sizeof(q), "%d", bestAcc[i].qty);
            char t[64]; FormatRupiahFromDouble(bestAcc[i].total, t, (int)sizeof(t));

            DrawText(bestAcc[i].item, (int)tx, (int)ty, 18, ((Color){40,40,40,255}));
            DrawText(q,               (int)(tx + 240), (int)ty, 18, ((Color){40,40,40,255}));
            DrawText(t,               (int)(tx + 300), (int)ty, 18, ((Color){40,40,40,255}));
            ty += 24;
        }
        if (nAcc == 0) DrawText("Tidak ada data.", (int)tx, (int)ty, 18, ((Color){80,80,80,255}));
    }
}

// pager bawah (pindah report page 1/2) -> SELALU kelihatan, ga ngilang
static void DrawReportPager(Rectangle contentArea)
{
    float btnW = 90, btnH = 34;

    float py = contentArea.y + contentArea.height - btnH - 160;
    float px = contentArea.x + contentArea.width - (btnW * 2 + 10 + 140) - 50;

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

    if (!gUiInited) {
        UITextBoxInit(&tbSearch, (Rectangle){ contentArea.x + 40,  (int)(170 + yOff), 380, 40 }, gSearch, (int)sizeof(gSearch), false);
        // shrink a bit to make room for calendar button
        UITextBoxInit(&tbFrom,   (Rectangle){ contentArea.x + 440, (int)(170 + yOff), 120, 40 }, gFrom,   (int)sizeof(gFrom), false);
        UITextBoxInit(&tbTo,     (Rectangle){ contentArea.x + 610, (int)(170 + yOff), 120, 40 }, gTo,     (int)sizeof(gTo), false);
        UIDatePickerInit(&dpFrom, 0, 0);
        UIDatePickerInit(&dpTo, 0, 0);
        gUiInited = 1;
    }

    static bool firstLoad = true;
    if (firstLoad) {
        ReloadRows(dbcPtr);
        BuildView();
        // keep snapshot in sync
        strncpy(gPrevSearch, gSearch, sizeof(gPrevSearch) - 1);
        gPrevSearch[sizeof(gPrevSearch) - 1] = '\0';
        strncpy(gPrevFrom, gFrom, sizeof(gPrevFrom) - 1);
        gPrevFrom[sizeof(gPrevFrom) - 1] = '\0';
        strncpy(gPrevTo, gTo, sizeof(gPrevTo) - 1);
        gPrevTo[sizeof(gPrevTo) - 1] = '\0';
        firstLoad = false;
    }

    if (gNeedReload) {
        ReloadRows(dbcPtr);
        gNeedReload = 0;
        BuildView();
        // keep snapshot in sync
        strncpy(gPrevSearch, gSearch, sizeof(gPrevSearch) - 1);
        gPrevSearch[sizeof(gPrevSearch) - 1] = '\0';
        strncpy(gPrevFrom, gFrom, sizeof(gPrevFrom) - 1);
        gPrevFrom[sizeof(gPrevFrom) - 1] = '\0';
        strncpy(gPrevTo, gTo, sizeof(gPrevTo) - 1);
        gPrevTo[sizeof(gPrevTo) - 1] = '\0';
    }

    if (gReportPage == 1) {
        DrawText("Cari", (int)contentArea.x + 40, (int)(145 + yOff), 18, WHITE);
        UITextBoxUpdate(&tbSearch);
        UITextBoxDraw(&tbSearch, 18);

        DrawText("Dari", (int)contentArea.x + 440, (int)(145 + yOff), 18, WHITE);
        UITextBoxUpdate(&tbFrom);
        UITextBoxDraw(&tbFrom, 18);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), tbFrom.bounds)) dpFrom.open = true;

        DrawText("Sampai", (int)contentArea.x + 600, (int)(145 + yOff), 18, WHITE);
        UITextBoxUpdate(&tbTo);
        UITextBoxDraw(&tbTo, 18);

        if (strcmp(gPrevSearch, gSearch) != 0 || strcmp(gPrevFrom, gFrom) != 0 || strcmp(gPrevTo, gTo) != 0) {
            strncpy(gPrevSearch, gSearch, sizeof(gPrevSearch) - 1);
            gPrevSearch[sizeof(gPrevSearch) - 1] = '\0';
            strncpy(gPrevFrom, gFrom, sizeof(gPrevFrom) - 1);
            gPrevFrom[sizeof(gPrevFrom) - 1] = '\0';
            strncpy(gPrevTo, gTo, sizeof(gPrevTo) - 1);
            gPrevTo[sizeof(gPrevTo) - 1] = '\0';
            BuildView();
            gScroll = 0;
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), tbTo.bounds)) dpTo.open = true;

        if (UIButton((Rectangle){ contentArea.x + 790, (int)(170 + yOff), 90, 40 }, "Reset", 18)) {
            gSearch[0] = '\0';
            gFrom[0] = '\0';
            gTo[0] = '\0';

            dpFrom.open = 0;
            dpTo.open = 0;

            BuildView();
            gScroll = 0;
        }

        if (UIButton((Rectangle){ contentArea.x + 890, (int)(170 + yOff), 140, 40 }, "Ekspor XLSX", 18)) {
            ExportXLSX("sales_report");
        }
    }

    if (gReportPage == 1) DrawSalesReport_Page1(contentArea);
    else DrawSalesReport_Page2(contentArea, dbcPtr);

    DrawReportPager(contentArea);

    // ===== calendar overlay (HARUS setelah semua konten biar tidak ketutupan) =====
    if (UIDatePickerUpdateDraw(&dpFrom, tbFrom.bounds, gFrom, (int)sizeof(gFrom))) {
        BuildView();
        gScroll = 0;
    }
    if (UIDatePickerUpdateDraw(&dpTo, tbTo.bounds, gTo, (int)sizeof(gTo))) {
        BuildView();
        gScroll = 0;
    }

    DrawToast();
}