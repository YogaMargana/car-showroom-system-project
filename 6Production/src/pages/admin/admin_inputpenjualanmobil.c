#include "admin_inputpenjualanmobil.h"

#include "raylib.h"

#include "ui.h"

#include "textbox.h"

#include "db_cardata.h"

#include "db_customers.h"

#include "db_employees.h"

#include "db_penjualanmobil.h"

#include "models.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#define MAX_CARS 200
#define MAX_CART 200
#define MAX_LOOKUP 250
#define MAX_CUSTOMERS 250
#define MAX_EMPLOYEES 250
#define PAGE_SIZE_MOBIL 3
#define HASIL_ROW_H 26

typedef struct
{
    char id[32];
    char label[96];
} LookupItem;

// =======================================================
// PROTOTYPES
// =======================================================
static int ClampInt(int v, int lo, int hi);
static int CeilDivInt(int a, int b);
static int StrContainsI(const char *hay, const char *needle);

static void CloseAllPickers(void);
static bool SimpleButton(Rectangle r, const char *label, int fs);

static void BeginClip(Rectangle r);
static void DrawTextClipped(Rectangle clip, const char *text, int fontSize, Color color, int scrollX);

static void DrawReadOnlyBox(Rectangle bounds, const char *text, int fontSize);

static long long ToLLSafe(const char *s);
static void FormatRupiah(long long value, char *out, int outSize);
static long long GetCarHargaLL(const Car *c);
static long long CalcTotalCart(void);

static void ReloadCars(void *dbcPtr);
static void ReloadLookups(void *dbcPtr);

static int PassSearch(const Car *c);
static void DrawSearchBox(Rectangle r);

static int FindCartIndexByCar(int carIndex);
static int GetQtyByCar(int carIndex);
static void CartSetQty(int carIndex, int qty);
static void CartInc(int carIndex);
static void CartDec(int carIndex);

static bool MiniButtonColored(Rectangle r, const char *label, Color base, Color hotCol);

static void DrawCarsTable(Rectangle area, bool allowInteraction);
static void DrawHasilInputTableScroll(Rectangle area);

static void DrawPicker(Rectangle tbBounds,
                       UITextBox *tb,
                       char *outId,
                       int outIdSize,
                       char *displayBuf,
                       int displayBufSize,
                       const LookupItem *items,
                       int itemCount,
                       bool *openFlag,
                       float maxWidth,
                       int *scrollPtr);

static void DrawRightForm(Rectangle rightPanel, void *dbcPtr);

static void PayPopup_Reset(void);
static void PayPopup_HandleInput(long long total);
static void PayPopup_Draw(Rectangle screenArea, long long total, bool *confirmed);

static bool SaveCartToDb(void *dbcVoid);
static void ResetInputPage(void);

// =====================
// DATA MOBIL (KIRI)
// =====================
static Car gCars[MAX_CARS];
static int gCarCount = 0;
static int gSelected = -1;
static int gPage = 0;
static int gInited = 0;
static int gNeedReload = 1;

// search (NamaMobil only)
static char gSearch[32] = "";
static int gSearchLen = 0;

// horizontal scroll offsets (SHIFT + wheel)
static int gScrollNameX = 0;
static int gScrollActionX = 0;

// =====================
// HASIL INPUT TABLE (PUTIH) – scroll
// =====================
static int gHasilScroll = 0;

// =====================
// CART
// =====================
typedef struct
{
    int carIndex;
    int qty;
} CartItem;

static CartItem gCart[MAX_CART];
static int gCartCount = 0;

// =====================
// LOOKUP LISTS (KANAN)
// =====================
static LookupItem gKasirs[MAX_LOOKUP];
static int gKasirCount = 0;

static LookupItem gPelanggans[MAX_LOOKUP];
static int gPelangganCount = 0;

static LookupItem gSalesList[MAX_LOOKUP];
static int gSalesCount = 0;

// raw db rows
static Customer gCustRows[MAX_CUSTOMERS];
static int gCustRowCount = 0;

static Employee gEmpRows[MAX_EMPLOYEES];
static int gEmpRowCount = 0;

// =====================
// FORM VALUES (KANAN)
// =====================
static char gNoTransaksi[24] = "";
static char gKasirID[32] = "";
static char gPelangganID[32] = "";
static char gSalesID[32] = "";

// textbox buffer untuk select (display+search)
static char gKasirDisplay[64] = "";
static char gPelangganDisplay[64] = "";
static char gSalesDisplay[64] = "";

// UI textbox
static int gUiInited = 0;

// tbNoTransaksi TIDAK dipakai untuk input (hanya display read-only)
static UITextBox tbKasir, tbPelanggan, tbSales;

// picker open states
static bool gPickKasirOpen = false;
static bool gPickPelangganOpen = false;
static bool gPickSalesOpen = false;

// scroll per picker
static int gPickKasirScroll = 0;
static int gPickPelangganScroll = 0;
static int gPickSalesScroll = 0;

// =====================
// POPUP BAYAR
// =====================
static bool gPayPopupOpen = false;
static char gUangBuf[32] = "";
static int gUangLen = 0;
static long long gUangValue = 0;
static long long gKembalianValue = 0;

// Status popup (ditampilkan di bagian akhir popup)
static char gStatusBayar[16] = "KURANG";

// =====================
// UTIL
// =====================
static int ClampInt(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static int CeilDivInt(int a, int b)
{
    if (b <= 0)
        return 0;
    if (a <= 0)
        return 0;
    return (a + b - 1) / b;
}

static int StrContainsI(const char *hay, const char *needle)
{
    if (!needle || needle[0] == '\0')
        return 1;
    if (!hay)
        return 0;

    char nlow[128];
    size_t nl = strlen(needle);
    if (nl >= sizeof(nlow))
        nl = sizeof(nlow) - 1;

    for (size_t i = 0; i < nl; i++)
        nlow[i] = (char)tolower((unsigned char)needle[i]);
    nlow[nl] = '\0';

    const char *p = hay;
    while (*p)
    {
        size_t j = 0;
        while (p[j] && nlow[j] && (char)tolower((unsigned char)p[j]) == nlow[j])
            j++;
        if (nlow[j] == '\0')
            return 1;
        p++;
    }
    return 0;
}

static void CloseAllPickers(void)
{
    gPickKasirOpen = false;
    gPickPelangganOpen = false;
    gPickSalesOpen = false;
}

static bool SimpleButton(Rectangle r, const char *label, int fs)
{
    Vector2 m = GetMousePosition();
    bool hot = CheckCollisionPointRec(m, r);

    DrawRectangleRec(r, hot ? (Color){245, 245, 245, 255} : RAYWHITE);
    DrawRectangleLinesEx(r, 2.0f, BLACK);

    int tw = MeasureText(label, fs);
    int tx = (int)(r.x + (r.width - tw) / 2);
    int ty = (int)(r.y + (r.height - fs) / 2);
    DrawText(label, tx, ty, fs, BLACK);

    return hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

// =====================
// CLIP HELPERS
// =====================
static void BeginClip(Rectangle r)
{
    int x = (int)r.x, y = (int)r.y, w = (int)r.width, h = (int)r.height;
    if (w < 1)
        w = 1;
    if (h < 1)
        h = 1;
    BeginScissorMode(x, y, w, h);
}

static void DrawTextClipped(Rectangle clip, const char *text, int fontSize, Color color, int scrollX)
{
    if (clip.width < 1 || clip.height < 1)
        return;
    BeginClip(clip);
    DrawText(text, (int)clip.x - scrollX, (int)clip.y, fontSize, color);
    EndScissorMode();
}

// =====================
// READONLY BOX (NO TRANSAKSI)
// =====================
static void DrawReadOnlyBox(Rectangle bounds, const char *text, int fontSize)
{
    DrawRectangleRec(bounds, (Color){245, 245, 245, 255});
    DrawRectangleLinesEx(bounds, 2.0f, BLACK);

    const char *t = (text && text[0]) ? text : "-";
    DrawText(t, (int)bounds.x + 10, (int)bounds.y + 8, fontSize, BLACK);
}

// =====================
// RUPIAH + TOTAL CART
// =====================
static long long ToLLSafe(const char *s)
{
    if (!s || s[0] == '\0')
        return 0;
    return strtoll(s, NULL, 10);
}

static void FormatRupiah(long long value, char *out, int outSize)
{
    if (!out || outSize <= 0)
        return;

    long long v = value;
    if (v < 0)
        v = -v;

    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%lld", v);

    int len = (int)strlen(tmp);
    int w = 0;

    out[w++] = 'R';
    out[w++] = 'p';
    out[w++] = ' ';

    int firstGroup = len % 3;
    if (firstGroup == 0)
        firstGroup = 3;

    for (int i = 0; i < len && w < outSize - 1; i++)
    {
        out[w++] = tmp[i];

        int pos = i + 1;
        if (pos == len)
            break;

        if (pos == firstGroup || (pos > firstGroup && ((pos - firstGroup) % 3 == 0)))
        {
            if (w < outSize - 1)
                out[w++] = '.';
        }
    }

    out[w] = '\0';
}

static long long GetCarHargaLL(const Car *c)
{
    return ToLLSafe(c->Harga);
}

static long long CalcTotalCart(void)
{
    long long total = 0;

    for (int i = 0; i < gCartCount; i++)
    {
        int carIndex = gCart[i].carIndex;
        int qty = gCart[i].qty;
        if (carIndex < 0 || carIndex >= gCarCount)
            continue;
        if (qty <= 0)
            continue;

        long long harga = GetCarHargaLL(&gCars[carIndex]);
        total += harga * (long long)qty;
    }

    return total;
}

// =====================
// DB LOAD
// =====================
static void ReloadCars(void *dbcPtr)
{
    void *dbc = NULL;
    if (dbcPtr)
        dbc = *(void **)dbcPtr;

    int count = 0;
    if (DbCarData_LoadAll(dbc, gCars, MAX_CARS, &count))
    {
        gCarCount = count;

        int maxPage = CeilDivInt(gCarCount, PAGE_SIZE_MOBIL) - 1;
        if (maxPage < 0)
            maxPage = 0;
        gPage = ClampInt(gPage, 0, maxPage);

        if (gSelected >= gCarCount)
            gSelected = -1;
    }
    else
    {
        gCarCount = 0;
        gPage = 0;
        gSelected = -1;
    }
}

static void ReloadLookups(void *dbcPtr)
{
    void *dbc = NULL;
    if (dbcPtr)
        dbc = *(void **)dbcPtr;
    if (!dbc)
        return;

    gKasirCount = 0;
    gPelangganCount = 0;
    gSalesCount = 0;

    int cc = 0;
    if (DbCustomers_LoadAll(dbc, gCustRows, MAX_CUSTOMERS, &cc))
    {
        gCustRowCount = cc;
        for (int i = 0; i < gCustRowCount && gPelangganCount < MAX_LOOKUP; i++)
        {
            snprintf(gPelanggans[gPelangganCount].id, sizeof(gPelanggans[gPelangganCount].id),
                     "%s", gCustRows[i].PelangganID);
            snprintf(gPelanggans[gPelangganCount].label, sizeof(gPelanggans[gPelangganCount].label),
                     "%s - %s", gCustRows[i].PelangganID, gCustRows[i].Nama);
            gPelangganCount++;
        }
    }

    int ec = 0;
    if (DbEmployees_LoadAll(dbc, gEmpRows, MAX_EMPLOYEES, &ec))
    {
        gEmpRowCount = ec;
        for (int i = 0; i < gEmpRowCount; i++)
        {
            if (strcmp(gEmpRows[i].Posisi, "Kasir") == 0)
            {
                if (gKasirCount < MAX_LOOKUP)
                {
                    snprintf(gKasirs[gKasirCount].id, sizeof(gKasirs[gKasirCount].id),
                             "%s", gEmpRows[i].KaryawanID);
                    snprintf(gKasirs[gKasirCount].label, sizeof(gKasirs[gKasirCount].label),
                             "%s - %s", gEmpRows[i].KaryawanID, gEmpRows[i].Nama);
                    gKasirCount++;
                }
            }
            else if (strcmp(gEmpRows[i].Posisi, "Sales") == 0)
            {
                if (gSalesCount < MAX_LOOKUP)
                {
                    snprintf(gSalesList[gSalesCount].id, sizeof(gSalesList[gSalesCount].id),
                             "%s", gEmpRows[i].KaryawanID);
                    snprintf(gSalesList[gSalesCount].label, sizeof(gSalesList[gSalesCount].label),
                             "%s - %s", gEmpRows[i].KaryawanID, gEmpRows[i].Nama);
                    gSalesCount++;
                }
            }
        }
    }
}

// =====================
// SEARCH
// =====================
static int PassSearch(const Car *c)
{
    if (!gSearch[0])
        return 1;
    if (!c)
        return 0;

    char hay[256];
    snprintf(hay, sizeof(hay), "%s", c->NamaMobil);
    for (int i = 0; hay[i]; i++)
        hay[i] = (char)tolower((unsigned char)hay[i]);

    char needle[32];
    int i = 0;
    for (; gSearch[i] && i < (int)sizeof(needle) - 1; i++)
        needle[i] = (char)tolower((unsigned char)gSearch[i]);
    needle[i] = '\0';

    return (strstr(hay, needle) != NULL);
}

static void DrawSearchBox(Rectangle r)
{
    DrawRectangleRec(r, RAYWHITE);
    DrawRectangleLinesEx(r, 2.0f, BLACK);

    DrawText(gSearch[0] ? gSearch : "search nama mobil",
             (int)r.x + 10, (int)r.y + 10, 18,
             gSearch[0] ? BLACK : GRAY);

    Vector2 m = GetMousePosition();
    if (!CheckCollisionPointRec(m, r))
        return;

    int key = GetCharPressed();
    while (key > 0)
    {
        if (key >= 32 && key <= 126)
        {
            if (gSearchLen < (int)sizeof(gSearch) - 1)
            {
                gSearch[gSearchLen++] = (char)key;
                gSearch[gSearchLen] = '\0';
                gPage = 0;
            }
        }
        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE))
    {
        if (gSearchLen > 0)
        {
            gSearch[--gSearchLen] = '\0';
            gPage = 0;
        }
    }
}

// =====================
// CART
// =====================
static int FindCartIndexByCar(int carIndex)
{
    for (int i = 0; i < gCartCount; i++)
        if (gCart[i].carIndex == carIndex)
            return i;
    return -1;
}

static int GetQtyByCar(int carIndex)
{
    int ci = FindCartIndexByCar(carIndex);
    return (ci >= 0) ? gCart[ci].qty : 0;
}

static void CartSetQty(int carIndex, int qty)
{
    if (qty < 0)
        qty = 0;

    int ci = FindCartIndexByCar(carIndex);

    if (qty == 0)
    {
        if (ci >= 0)
        {
            for (int k = ci; k < gCartCount - 1; k++)
                gCart[k] = gCart[k + 1];
            gCartCount--;
        }
        return;
    }

    if (ci >= 0)
    {
        gCart[ci].qty = qty;
        return;
    }

    if (gCartCount < MAX_CART)
    {
        gCart[gCartCount].carIndex = carIndex;
        gCart[gCartCount].qty = qty;
        gCartCount++;
    }
}

static void CartInc(int carIndex) { CartSetQty(carIndex, GetQtyByCar(carIndex) + 1); }
static void CartDec(int carIndex) { CartSetQty(carIndex, GetQtyByCar(carIndex) - 1); }

// =====================
// MINI BUTTON
// =====================
static bool MiniButtonColored(Rectangle r, const char *label, Color base, Color hotCol)
{
    Vector2 m = GetMousePosition();
    bool hot = CheckCollisionPointRec(m, r);

    DrawRectangleRec(r, hot ? hotCol : base);
    DrawRectangleLinesEx(r, 2.0f, BLACK);

    int fs = 18;
    int tw = MeasureText(label, fs);
    DrawText(label, (int)(r.x + (r.width - tw) / 2), (int)(r.y + 2), fs, RAYWHITE);

    return hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

// =====================
// TABLE KIRI (mobil)
// =====================
static void DrawCarsTable(Rectangle area, bool allowInteraction)
{
    const float pad = 10.0f;
    float headerY = area.y + pad;
    float headerH = 30.0f;
    float sepY = headerY + headerH + 5.0f;
    int rowH = 30;
    float baseY = sepY + 6;

    int filtered[MAX_CARS];
    int fcnt = 0;
    for (int i = 0; i < gCarCount && fcnt < MAX_CARS; i++)
        if (PassSearch(&gCars[i]))
            filtered[fcnt++] = i;

    int totalPages = CeilDivInt(fcnt, PAGE_SIZE_MOBIL);
    if (totalPages < 1)
        totalPages = 1;

    gPage = ClampInt(gPage, 0, totalPages - 1);

    int start = gPage * PAGE_SIZE_MOBIL;
    int end = start + PAGE_SIZE_MOBIL;
    if (end > fcnt)
        end = fcnt;

    float pagerH = 34.0f;
    float contentH = (baseY - area.y) + (float)PAGE_SIZE_MOBIL * rowH + 10.0f + pagerH + 10.0f;
    if (contentH > area.height)
        contentH = area.height;

    Rectangle bg = (Rectangle){area.x, area.y, area.width, contentH};
    DrawRectangleRec(bg, RAYWHITE);
    DrawRectangleLinesEx(bg, 2.0f, BLACK);

    float colNo = area.x + pad + 0;
    float colId = area.x + pad + 50;
    float colName = area.x + pad + 150;
    float colStock = area.x + pad + 380;
    float colHarga = area.x + pad + 460;
    float colAct = area.x + pad + 640;

    DrawText("No", (int)colNo, (int)headerY, 18, BLACK);
    DrawText("ID", (int)colId, (int)headerY, 18, BLACK);
    DrawText("Nama", (int)colName, (int)headerY, 18, BLACK);
    DrawText("Stok", (int)colStock, (int)headerY, 18, BLACK);
    DrawText("Harga", (int)colHarga, (int)headerY, 18, BLACK);

    Rectangle actionHeaderClip = (Rectangle){colAct, headerY, 90, 20};
    DrawTextClipped(actionHeaderClip, "Action", 18, BLACK, gScrollActionX);

    DrawLineEx((Vector2){area.x + 6, sepY},
               (Vector2){area.x + area.width - 6, sepY},
               2.0f, (Color){160, 160, 160, 255});

    Vector2 m = GetMousePosition();
    float wheel = GetMouseWheelMove();
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

    if (allowInteraction && shift && wheel != 0)
    {
        Rectangle nameZone = (Rectangle){colName, baseY, 220, (float)PAGE_SIZE_MOBIL * rowH};
        Rectangle actZone = (Rectangle){colAct, baseY, 90, (float)PAGE_SIZE_MOBIL * rowH};

        if (CheckCollisionPointRec(m, nameZone))
            gScrollNameX = ClampInt(gScrollNameX - (int)wheel * 20, 0, 2000);
        if (CheckCollisionPointRec(m, actZone))
            gScrollActionX = ClampInt(gScrollActionX - (int)wheel * 20, 0, 200);
    }

    for (int slot = 0; slot < PAGE_SIZE_MOBIL; slot++)
    {
        float y = baseY + (float)slot * rowH;
        Rectangle row = (Rectangle){area.x + 6, y, area.width - 12, (float)rowH};

        int k = start + slot;
        bool hasRow = (k < end);

        if (hasRow)
        {
            int i = filtered[k];

            if (i == gSelected)
                DrawRectangleRec(row, (Color){200, 220, 255, 255});

            if (allowInteraction && CheckCollisionPointRec(m, row) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                gSelected = i;

            char noStr[8];
            snprintf(noStr, sizeof(noStr), "%d", k + 1);

            DrawText(noStr, (int)colNo, (int)row.y + 6, 16, BLACK);
            DrawText(gCars[i].MobilID, (int)colId, (int)row.y + 6, 16, BLACK);

            Rectangle nameClip = (Rectangle){colName, row.y + 6, 220, 18};
            DrawTextClipped(nameClip, gCars[i].NamaMobil, 16, BLACK, gScrollNameX);

            Rectangle stokClip = (Rectangle){colStock, row.y + 6, 60, 18};
            DrawTextClipped(stokClip, gCars[i].Stok, 16, BLACK, 0);

            char hargaRp[64];
            FormatRupiah(GetCarHargaLL(&gCars[i]), hargaRp, (int)sizeof(hargaRp));
            Rectangle hargaClip = (Rectangle){colHarga, row.y + 6, 160, 18};
            DrawTextClipped(hargaClip, hargaRp, 16, BLACK, 0);

            int qty = GetQtyByCar(i);

            Rectangle actClip = (Rectangle){colAct, row.y + 4, 90, 22};
            BeginClip(actClip);

            Rectangle bMinus = (Rectangle){colAct - gScrollActionX, row.y + 4, 22, 22};
            Rectangle bQty = (Rectangle){colAct + 26 - gScrollActionX, row.y + 4, 26, 22};
            Rectangle bPlus = (Rectangle){colAct + 56 - gScrollActionX, row.y + 4, 22, 22};

            if (MiniButtonColored(bMinus, "-", (Color){200, 40, 40, 255}, (Color){230, 70, 70, 255}))
                CartDec(i);

            DrawRectangleRec(bQty, (Color){240, 240, 240, 255});
            DrawRectangleLinesEx(bQty, 1.0f, BLACK);

            char qStr[8];
            snprintf(qStr, sizeof(qStr), "%d", qty);
            DrawText(qStr, (int)bQty.x + 8, (int)bQty.y + 3, 18, BLACK);

            if (MiniButtonColored(bPlus, "+", (Color){40, 160, 60, 255}, (Color){70, 190, 90, 255}))
                CartInc(i);

            EndScissorMode();
        }

        float yline = y + (float)rowH;
        DrawLineEx((Vector2){area.x + 6, yline},
                   (Vector2){area.x + area.width - 6, yline},
                   1.0f, (Color){200, 200, 200, 255});
    }

    float pagerY = baseY + (float)PAGE_SIZE_MOBIL * rowH + 10.0f;

    Rectangle btnPrev = (Rectangle){area.x + 10, pagerY, 90, pagerH};
    Rectangle btnNext = (Rectangle){area.x + area.width - 10 - 90, pagerY, 90, pagerH};

    if (SimpleButton(btnPrev, "Prev", 18))
        gPage = ClampInt(gPage - 1, 0, totalPages - 1);

    if (SimpleButton(btnNext, "Next", 18))
        gPage = ClampInt(gPage + 1, 0, totalPages - 1);

    char pageInfo[64];
    snprintf(pageInfo, sizeof(pageInfo), "Page %d/%d", gPage + 1, totalPages);
    int tw = MeasureText(pageInfo, 18);
    int tx = (int)(area.x + (area.width - tw) / 2);
    DrawText(pageInfo, tx, (int)pagerY + 8, 18, BLACK);
}

// =====================
// TABLE HASIL INPUT putih + scroll
// =====================
static void DrawHasilInputTableScroll(Rectangle area)
{
    int count = gCartCount;
    const float pad = 10.0f;
    const float headerH = 28.0f;

    float headerY = area.y + pad;
    float rowsY = headerY + headerH + 6.0f;

    float left = area.x + 12;
    float right = area.x + area.width - 12;

    float wNo = 40;
    float wQty = 60;
    float wHarga = 120;
    float gap = 10;

    float colNoX = left;
    float colNamaX = colNoX + wNo + gap;
    float colHargaX = right - wHarga;
    float colQtyX = colHargaX - gap - wQty;
    float wNama = colQtyX - gap - colNamaX;
    if (wNama < 60)
        wNama = 60;

    int visibleRows = (int)((area.height - (rowsY - area.y) - pad) / (float)HASIL_ROW_H);
    if (visibleRows < 1)
        visibleRows = 1;

    int maxScroll = count - visibleRows;
    if (maxScroll < 0)
        maxScroll = 0;

    Vector2 m = GetMousePosition();
    float wheel = GetMouseWheelMove();
    if (wheel != 0 && CheckCollisionPointRec(m, area))
    {
        gHasilScroll -= (int)wheel;
        gHasilScroll = ClampInt(gHasilScroll, 0, maxScroll);
    }

    int shown = count - gHasilScroll;
    if (shown < 0)
        shown = 0;
    if (shown > visibleRows)
        shown = visibleRows;

    float contentH = (rowsY - area.y) + (float)shown * HASIL_ROW_H + pad;
    if (contentH < (rowsY - area.y) + pad)
        contentH = (rowsY - area.y) + pad;
    if (contentH > area.height)
        contentH = area.height;

    Rectangle bg = (Rectangle){area.x, area.y, area.width, contentH};
    DrawRectangleRec(bg, RAYWHITE);
    DrawRectangleLinesEx(bg, 2.0f, BLACK);

    DrawText("NO", (int)colNoX, (int)headerY + 4, 18, BLACK);
    DrawText("NAMA", (int)colNamaX, (int)headerY + 4, 18, BLACK);
    DrawText("QTY", (int)colQtyX, (int)headerY + 4, 18, BLACK);
    DrawText("HARGA", (int)colHargaX, (int)headerY + 4, 18, BLACK);

    DrawLine((int)area.x + 10, (int)(headerY + headerH),
             (int)(area.x + area.width - 10), (int)(headerY + headerH),
             (Color){200, 200, 200, 255});

    Rectangle clip = (Rectangle){area.x, rowsY, area.width, contentH - (rowsY - area.y)};
    if (clip.width < 1)
        clip.width = 1;
    if (clip.height < 1)
        clip.height = 1;

    BeginScissorMode((int)clip.x, (int)clip.y, (int)clip.width, (int)clip.height);

    int start = gHasilScroll;
    int end = start + visibleRows;
    if (end > count)
        end = count;

    for (int i = start; i < end; i++)
    {
        int row = i - start;
        float y = rowsY + row * HASIL_ROW_H;

        int carIndex = gCart[i].carIndex;
        int qty = gCart[i].qty;

        const char *nama = "-";
        long long harga = 0;

        if (carIndex >= 0 && carIndex < gCarCount)
        {
            nama = gCars[carIndex].NamaMobil;
            harga = GetCarHargaLL(&gCars[carIndex]);
        }

        char no[8];
        snprintf(no, sizeof(no), "%d", i + 1);

        char qtyStr[16];
        snprintf(qtyStr, sizeof(qtyStr), "%d", qty);

        char hargaRp[64];
        FormatRupiah(harga, hargaRp, (int)sizeof(hargaRp));

        DrawText(no, (int)colNoX, (int)y + 4, 16, BLACK);

        Rectangle namaClip = (Rectangle){colNamaX, y + 4, wNama, 18};
        DrawTextClipped(namaClip, nama, 16, BLACK, 0);

        DrawText(qtyStr, (int)colQtyX, (int)y + 4, 16, BLACK);
        DrawText(hargaRp, (int)colHargaX, (int)y + 4, 16, BLACK);

        DrawLine((int)area.x + 10, (int)(y + HASIL_ROW_H),
                 (int)(area.x + area.width - 10), (int)(y + HASIL_ROW_H),
                 (Color){200, 200, 200, 255});
    }

    EndScissorMode();
}

// =====================
// PICKER
// =====================
#define PICKER_VISIBLE 5
#define PICKER_ROW_H 26

static void DrawPicker(Rectangle tbBounds,
                       UITextBox *tb,
                       char *outId,
                       int outIdSize,
                       char *displayBuf,
                       int displayBufSize,
                       const LookupItem *items,
                       int itemCount,
                       bool *openFlag,
                       float maxWidth,
                       int *scrollPtr)
{
    Vector2 m = GetMousePosition();

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(m, tbBounds))
    {
        CloseAllPickers();
        *openFlag = true;
        tb->active = true;
    }

    if (!*openFlag)
        return;

    const char *q = tb->buffer;

    int matches[MAX_LOOKUP];
    int mc = 0;
    for (int i = 0; i < itemCount && mc < MAX_LOOKUP; i++)
    {
        if (!q || q[0] == '\0' || StrContainsI(items[i].id, q) || StrContainsI(items[i].label, q))
            matches[mc++] = i;
    }

    float dropW = tbBounds.width;
    if (maxWidth > 0 && dropW > maxWidth)
        dropW = maxWidth;

    int visible = (mc < PICKER_VISIBLE) ? mc : PICKER_VISIBLE;
    if (visible < 1)
        visible = 1;

    int maxScroll = mc - visible;
    if (maxScroll < 0)
        maxScroll = 0;

    *scrollPtr = ClampInt(*scrollPtr, 0, maxScroll);

    float dropH = (float)visible * PICKER_ROW_H + 10.0f;

    Rectangle drop = (Rectangle){tbBounds.x, tbBounds.y + tbBounds.height + 6.0f, dropW, dropH};

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        bool inside = CheckCollisionPointRec(m, tbBounds) || CheckCollisionPointRec(m, drop);
        if (!inside)
        {
            *openFlag = false;
            tb->active = false;
            return;
        }
    }

    float wheel = GetMouseWheelMove();
    if (wheel != 0 && CheckCollisionPointRec(m, drop))
    {
        *scrollPtr -= (int)wheel;
        *scrollPtr = ClampInt(*scrollPtr, 0, maxScroll);
    }

    DrawRectangleRec(drop, RAYWHITE);
    DrawRectangleLinesEx(drop, 2.0f, BLACK);

    if (mc == 0)
    {
        DrawText("No results", (int)drop.x + 10, (int)drop.y + 8, 16, GRAY);
        return;
    }

    int start = *scrollPtr;
    int end = start + visible;
    if (end > mc)
        end = mc;

    for (int r = start; r < end; r++)
    {
        int idx = matches[r];
        int row = r - start;

        Rectangle rowRec = (Rectangle){drop.x + 5, drop.y + 5 + (float)row * PICKER_ROW_H,
                                       drop.width - 10, PICKER_ROW_H};

        bool hover = CheckCollisionPointRec(m, rowRec);
        if (hover)
            DrawRectangleRec(rowRec, (Color){220, 230, 255, 255});

        DrawText(items[idx].label, (int)rowRec.x + 8, (int)rowRec.y + 4, 16, BLACK);

        if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            strncpy(outId, items[idx].id, (size_t)outIdSize - 1);
            outId[outIdSize - 1] = '\0';

            snprintf(displayBuf, (size_t)displayBufSize, "%s", items[idx].label);
            snprintf(tb->buffer, (size_t)displayBufSize, "%s", displayBuf);
            tb->len = (int)strlen(tb->buffer);

            *openFlag = false;
            tb->active = false;
        }
    }
}

// =====================
// FORM PANEL KANAN
// =====================
static void DrawRightForm(Rectangle rightPanel, void *dbcPtr)
{
    if (!gUiInited)
    {
        ReloadLookups(dbcPtr);

        UITextBoxInit(&tbKasir, (Rectangle){0, 0, 0, 0},
                      gKasirDisplay, (int)sizeof(gKasirDisplay),
                      false);

        UITextBoxInit(&tbPelanggan, (Rectangle){0, 0, 0, 0},
                      gPelangganDisplay, (int)sizeof(gPelangganDisplay),
                      false);

        UITextBoxInit(&tbSales, (Rectangle){0, 0, 0, 0},
                      gSalesDisplay, (int)sizeof(gSalesDisplay),
                      false);

        gUiInited = 1;
    }

    Rectangle form = (Rectangle){rightPanel.x, rightPanel.y, rightPanel.width, 190};
    DrawRectangleRec(form, RAYWHITE);
    DrawRectangleLinesEx(form, 2.0f, BLACK);

    float labelX = form.x + 20;
    float col1X = labelX + 170; // kolom input kiri
    float rowY = form.y + 18;

    // 1) KASIR (PALING ATAS)
    DrawText("Kasir", (int)labelX, (int)rowY + 8, 18, BLACK);
    tbKasir.bounds = (Rectangle){col1X, rowY, 420, 34}; // dibuat lebih panjang biar enak
    rowY += 45;

    // 2) PELANGGAN
    DrawText("Pelanggan", (int)labelX, (int)rowY + 8, 18, BLACK);
    tbPelanggan.bounds = (Rectangle){col1X, rowY, 420, 34};
    rowY += 45;

    // 3) SALES
    DrawText("Sales", (int)labelX, (int)rowY + 8, 18, BLACK);
    tbSales.bounds = (Rectangle){col1X, rowY, 420, 34};

    UITextBoxUpdate(&tbKasir);
    UITextBoxUpdate(&tbPelanggan);
    UITextBoxUpdate(&tbSales);

    UITextBoxDraw(&tbKasir, 18);
    UITextBoxDraw(&tbPelanggan, 18);
    UITextBoxDraw(&tbSales, 18);
}

// =====================
// POPUP BAYAR
// =====================
static void PayPopup_Reset(void)
{
    gPayPopupOpen = true;
    gUangBuf[0] = '\0';
    gUangLen = 0;
    gUangValue = 0;
    gKembalianValue = 0;
    snprintf(gStatusBayar, sizeof(gStatusBayar), "KURANG");
}

static void PayPopup_HandleInput(long long total)
{
    int ch = GetCharPressed();
    while (ch > 0)
    {
        if (ch >= '0' && ch <= '9')
        {
            if (gUangLen < (int)sizeof(gUangBuf) - 1)
            {
                gUangBuf[gUangLen++] = (char)ch;
                gUangBuf[gUangLen] = '\0';
            }
        }
        ch = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE))
    {
        if (gUangLen > 0)
            gUangBuf[--gUangLen] = '\0';
    }

    if (IsKeyPressed(KEY_ESCAPE))
    {
        gPayPopupOpen = false;
        return;
    }

    gUangValue = ToLLSafe(gUangBuf);
    gKembalianValue = gUangValue - total;

    if (gKembalianValue >= 0)
        snprintf(gStatusBayar, sizeof(gStatusBayar), "LUNAS");
    else
        snprintf(gStatusBayar, sizeof(gStatusBayar), "KURANG");
}

static void PayPopup_Draw(Rectangle screenArea, long long total, bool *confirmed)
{
    if (confirmed)
        *confirmed = false;

    DrawRectangle(0, 0, (int)screenArea.width, (int)screenArea.height, (Color){0, 0, 0, 160});

    float w = 520;
    float h = 330;

    Rectangle box = (Rectangle){
        screenArea.x + (screenArea.width - w) / 2.0f,
        screenArea.y + (screenArea.height - h) / 2.0f,
        w, h};

    DrawRectangleRec(box, RAYWHITE);
    DrawRectangleLinesEx(box, 2.0f, BLACK);

    float x = box.x + 24;
    float y = box.y + 20;

    DrawText("Masukan jumlah uang", (int)x, (int)y, 22, BLACK);

    y += 45;
    Rectangle uangBox = (Rectangle){x, y, box.width - 48, 42};
    DrawRectangleRec(uangBox, RAYWHITE);
    DrawRectangleLinesEx(uangBox, 2.0f, BLACK);

    char uangLabel[64];
    snprintf(uangLabel, sizeof(uangLabel), "%s", (gUangBuf[0] == '\0') ? "0" : gUangBuf);
    DrawText(uangLabel, (int)uangBox.x + 12, (int)uangBox.y + 10, 20, BLACK);

    y += 60;

    char totalRp[64], kembaliRp[64];
    FormatRupiah(total, totalRp, (int)sizeof(totalRp));
    FormatRupiah(gKembalianValue, kembaliRp, (int)sizeof(kembaliRp));

    DrawText("Total", (int)x, (int)y, 20, BLACK);
    DrawText(totalRp, (int)x + 120, (int)y, 20, BLACK);

    y += 34;

    DrawText("Kembalian", (int)x, (int)y, 20, BLACK);
    Color kcol = (gKembalianValue >= 0) ? DARKGREEN : MAROON;
    DrawText(kembaliRp, (int)x + 120, (int)y, 20, kcol);

    y += 34;

    DrawText("Status", (int)x, (int)y, 20, BLACK);
    Color scol = (gKembalianValue >= 0) ? DARKGREEN : MAROON;
    DrawText(gStatusBayar, (int)x + 120, (int)y, 20, scol);

    Rectangle btnArea = (Rectangle){box.x + 24, box.y + box.height - 60, box.width - 48, 42};
    float gap = 12.0f;
    float halfW = (btnArea.width - gap) / 2.0f;

    Rectangle btnBatal = (Rectangle){btnArea.x, btnArea.y, halfW, btnArea.height};
    Rectangle btnOK = (Rectangle){btnArea.x + halfW + gap, btnArea.y, halfW, btnArea.height};

    if (SimpleButton(btnBatal, "Batal", 20))
    {
        gPayPopupOpen = false;
        return;
    }

    if (SimpleButton(btnOK, "OK", 20))
    {
        if (confirmed)
            *confirmed = true;
        gPayPopupOpen = false;
        return;
    }
}

// =====================
// SAVE CART -> DB
// =====================
static bool SaveCartToDb(void *dbcVoid)
{
    if (!dbcVoid)
        return false;
    if (gCartCount <= 0)
        return false;

    long long grand = CalcTotalCart(); // total semua item

    for (int i = 0; i < gCartCount; i++)
    {
        int carIndex = gCart[i].carIndex;
        int qty = gCart[i].qty;
        if (qty <= 0)
            continue;

        char qtyStr[16], totalStr[32], uangStr[32];
        snprintf(qtyStr, sizeof(qtyStr), "%d", qty);
        snprintf(totalStr, sizeof(totalStr), "%lld", grand);
        snprintf(uangStr, sizeof(uangStr), "%lld", gUangValue);

        if (!DbPenjualanMobil_Insert(dbcVoid,
                                     gNoTransaksi,
                                     gCars[carIndex].MobilID,
                                     gSalesID, gKasirID, gPelangganID,
                                     qtyStr, totalStr, uangStr))
            return false;
    }

    return true;
}

static void ResetInputPage(void)
{
    gCartCount = 0;
    gSelected = -1;

    gNoTransaksi[0] = '\0';
    gKasirID[0] = '\0';
    gPelangganID[0] = '\0';
    gSalesID[0] = '\0';

    gKasirDisplay[0] = '\0';
    gPelangganDisplay[0] = '\0';
    gSalesDisplay[0] = '\0';

    // tbNoTransaksi tidak dipakai untuk input
    tbKasir.len = 0;
    tbPelanggan.len = 0;
    tbSales.len = 0;

    gUangBuf[0] = '\0';
    gUangLen = 0;
    gUangValue = 0;
    gKembalianValue = 0;

    snprintf(gStatusBayar, sizeof(gStatusBayar), "KURANG");

    gPickKasirOpen = false;
    gPickPelangganOpen = false;
    gPickSalesOpen = false;

    gHasilScroll = 0;
}

// =====================
// PAGE (MAIN)
// =====================
void AdminInputPenjualanMobilPage(AppState *app, Rectangle leftArea, Rectangle rightArea, void *dbcPtr)
{
    if (!gInited)
    {
        gInited = 1;
        gNeedReload = 1;
    }

    if (gNeedReload)
    {
        ReloadCars(dbcPtr);
        ReloadLookups(dbcPtr);
        gNeedReload = 0;
    }

    // ===== panel kiri =====
    float padL = 20;
    Rectangle leftPanel = (Rectangle){
        leftArea.x + padL,
        leftArea.y + padL,
        leftArea.width - padL * 2,
        leftArea.height - padL * 2};

    Rectangle searchBox = (Rectangle){leftPanel.x, leftPanel.y, leftPanel.width, 50};
    DrawSearchBox(searchBox);

    Rectangle tableMobil = (Rectangle){leftPanel.x, leftPanel.y + 70, leftPanel.width, leftPanel.height - 70};
    DrawCarsTable(tableMobil, true);

    // ===== panel kanan =====
    float padR = 20;
    Rectangle rightPanel = (Rectangle){
        rightArea.x + padR,
        rightArea.y + padR,
        rightArea.width - padR * 2,
        rightArea.height - padR * 2};

    DrawRightForm(rightPanel, dbcPtr);

    Rectangle hasil = (Rectangle){rightPanel.x, rightPanel.y + 240, rightPanel.width, 170};
    DrawHasilInputTableScroll(hasil);

    Rectangle totalRec = (Rectangle){rightPanel.x, rightPanel.y + 420, rightPanel.width, 90};
    DrawRectangleRec(totalRec, RAYWHITE);
    DrawRectangleLinesEx(totalRec, 2.0f, BLACK);

    long long grand = CalcTotalCart();
    char totalRp[64];
    FormatRupiah(grand, totalRp, (int)sizeof(totalRp));

    char line[96];
    snprintf(line, sizeof(line), "Total: %s", totalRp);

    int fs = 30;
    int tw = MeasureText(line, fs);
    int tx = (int)(totalRec.x + (totalRec.width - tw) / 2);
    int ty = (int)(totalRec.y + (totalRec.height - fs) / 2);
    DrawText(line, tx, ty, fs, BLACK);

    // tombol bawah
    Rectangle btnArea = (Rectangle){
        rightPanel.x,
        rightPanel.y + rightPanel.height - 50,
        rightPanel.width,
        40};

    float gap = 12.0f;
    float halfW = (btnArea.width - gap) / 2.0f;

    Rectangle btnKembali = (Rectangle){btnArea.x, btnArea.y, halfW, btnArea.height};
    Rectangle btnSelanjutnya = (Rectangle){btnArea.x + halfW + gap, btnArea.y, halfW, btnArea.height};

    if (!gPayPopupOpen)
    {
        if (UIButton(btnKembali, "Kembali", 20))
            app->halamanSekarang = HAL_CASHIER_PENJUALAN_MOBIL;

        if (UIButton(btnSelanjutnya, "Selanjutnya", 20))
        {
            // Generate NoTransaksi otomatis sekali per transaksi
            void *dbc = NULL;
            if (dbcPtr)
                dbc = *(void **)dbcPtr;

            if (dbc && gNoTransaksi[0] == '\0')
            {
                DbPenjualanMobil_CreateNoTransaksi(dbc, gNoTransaksi, (int)sizeof(gNoTransaksi));
            }

            PayPopup_Reset();
        }
    }
    else
    {
        UIButton(btnKembali, "Kembali", 20);
        UIButton(btnSelanjutnya, "Selanjutnya", 20);
    }

    // dropdown paling depan
    DrawPicker(tbKasir.bounds, &tbKasir,
               gKasirID, (int)sizeof(gKasirID),
               gKasirDisplay, (int)sizeof(gKasirDisplay),
               gKasirs, gKasirCount, &gPickKasirOpen, 420.0f, &gPickKasirScroll);

    DrawPicker(tbPelanggan.bounds, &tbPelanggan,
               gPelangganID, (int)sizeof(gPelangganID),
               gPelangganDisplay, (int)sizeof(gPelangganDisplay),
               gPelanggans, gPelangganCount, &gPickPelangganOpen, 420.0f, &gPickPelangganScroll);

    DrawPicker(tbSales.bounds, &tbSales,
               gSalesID, (int)sizeof(gSalesID),
               gSalesDisplay, (int)sizeof(gSalesDisplay),
               gSalesList, gSalesCount, &gPickSalesOpen, 420.0f, &gPickSalesScroll);

    // popup bayar
    if (gPayPopupOpen)
    {
        PayPopup_HandleInput(grand);

        bool ok = false;
        Rectangle screenArea = (Rectangle){0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()};
        PayPopup_Draw(screenArea, grand, &ok);

        if (ok)
        {
            void *dbc = NULL;
            if (dbcPtr)
                dbc = *(void **)dbcPtr;

            if (dbc)
            {
                if (SaveCartToDb(dbc))
                {
                    ResetInputPage();
                    app->halamanSekarang = HAL_CASHIER_PENJUALAN_MOBIL;
                }
                else
                {
                    // gagal simpan -> tetap di halaman ini
                }
            }
        }
    }
}
