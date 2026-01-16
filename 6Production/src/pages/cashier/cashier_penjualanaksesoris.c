#include "raylib.h"
#include "ui.h"
#include "db_penjualanaksesoris.h"
#include "role_menu.h"
#include "cashier_penjualanaksesoris.h"

#include <string.h>
#include <stdio.h>

void AdminPenjualanaksesorisPage(AppState *app, Rectangle contentArea, void *dbcPtr);

static void ReloadRows(void *dbcPtr);
static void DrawTablePaged(Rectangle tableArea, bool enabled, void *dbcPtr);
static int ClampInt(int v, int lo, int hi);

static void FormatRupiah(const char *src, char *dst, int dstSize);
static bool SmallButton(Rectangle r, const char *text, Color bg, Color fg);

#define MAX_ROWS 200
static PenjualanAksesorisdata gRows[MAX_ROWS];
static int gRowCount = 0;
static int gSelected = -1;

#define ROWS_PER_PAGE 10
static int gPageIndex = 0;

static float gReloadTimer = 0.0f;
static const float RELOAD_INTERVAL = 2.0f;

static int gUiInited = 0;
static int gNeedReload = 1;

// horizontal scroll
static float gScrollX = 0.0f;
static float gMaxScrollX = 0.0f;

// drag state
static bool gDraggingH = false;

// double click state
static float gClickTimer = 0.0f;
static int gLastClickRow = -1;
static const float DOUBLE_CLICK_WINDOW = 0.25f;

typedef enum
{
    MODAL_NONE = 0,
    MODAL_CONFIRM_DELETE
} ModalType;

static ModalType gModal = MODAL_NONE;
static char gPendingDeleteId[32] = "";

static const int FONT_TITLE = 28;
static const int FONT_HEADER = 16;
static const int FONT_ROW = 14;

static const float HEADER_H = 40.0f;
static const float ROW_H = 32.0f;
static const float TOP_PAD = 6.0f;

static const Color CLR_HEADER_BG = (Color){70, 130, 180, 255};
static const Color CLR_ROW_SEL = (Color){200, 220, 255, 255};
static const Color CLR_GRID = (Color){180, 180, 180, 255};
static const Color CLR_BTN_EDIT = (Color){70, 130, 180, 255};
static const Color CLR_BTN_DEL = (Color){180, 60, 60, 255};
static const Color CLR_BTN_ADD = (Color){60, 160, 90, 255};

static int ClampInt(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static void ReloadRows(void *dbcPtr)
{
    void *dbc = NULL;
    if (dbcPtr)
        dbc = *(void **)dbcPtr;

    int count = 0;
    if (DbPenjualanAksesoris_LoadAll(dbc, gRows, MAX_ROWS, &count))
    {
        gRowCount = count;
        if (gSelected >= gRowCount)
            gSelected = -1;

        int totalPages = (gRowCount + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE;
        if (totalPages < 1)
            totalPages = 1;
        gPageIndex = ClampInt(gPageIndex, 0, totalPages - 1);
    }
    else
    {
        gRowCount = 0;
        gSelected = -1;
        gPageIndex = 0;
    }
}

static void FormatRupiah(const char *src, char *dst, int dstSize)
{
    if (!dst || dstSize <= 0)
        return;
    dst[0] = '\0';

    if (!src || src[0] == '\0')
    {
        snprintf(dst, dstSize, "Rp0");
        return;
    }

    bool isNegative = false;
    int i = 0;

    if (src[0] == '-')
    {
        isNegative = true;
        i = 1;
    }

    char digits[64];
    int n = 0;

    for (; src[i] && n < (int)sizeof(digits) - 1; i++)
    {
        if (src[i] == '.' || src[i] == ',')
            break; // ⬅️ FIX UTAMA (STOP DESIMAL)

        if (src[i] >= '0' && src[i] <= '9')
            digits[n++] = src[i];
    }
    digits[n] = '\0';

    if (n == 0)
    {
        snprintf(dst, dstSize, "Rp0");
        return;
    }

    int start = 0;
    while (start < n - 1 && digits[start] == '0')
        start++;

    const char *num = digits + start;
    int len = strlen(num);
    int out = 0;

    if (isNegative)
        dst[out++] = '-';

    out += snprintf(dst + out, dstSize - out, "Rp");

    int first = len % 3;
    if (first == 0)
        first = 3;

    for (int j = 0; j < first; j++)
        dst[out++] = num[j];

    int idx = first;
    while (idx < len)
    {
        dst[out++] = '.';
        for (int k = 0; k < 3 && idx < len; k++)
            dst[out++] = num[idx++];
    }

    dst[out] = '\0';
}

static bool SmallButton(Rectangle r, const char *text, Color bg, Color fg)
{
    Vector2 m = GetMousePosition();
    bool hover = CheckCollisionPointRec(m, r);

    Color fill = bg;
    if (hover)
    {
        int rr = bg.r + 20;
        if (rr > 255)
            rr = 255;
        int gg = bg.g + 20;
        if (gg > 255)
            gg = 255;
        int bb = bg.b + 20;
        if (bb > 255)
            bb = 255;
        fill = (Color){(unsigned char)rr, (unsigned char)gg, (unsigned char)bb, 255};
    }

    DrawRectangleRec(r, fill);
    DrawRectangleLines((int)r.x, (int)r.y, (int)r.width, (int)r.height, BLACK);

    int fs = 13;
    int tw = MeasureText(text, fs);
    DrawText(text, (int)(r.x + (r.width - tw) / 2), (int)(r.y + (r.height - fs) / 2), fs, fg);

    return hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

static void DrawTablePaged(Rectangle tableArea, bool enabled, void *dbcPtr)
{
    void *dbc = NULL;
    if (dbcPtr)
        dbc = *(void **)dbcPtr;

    if (!enabled)
    {
        DrawRectangleRec(tableArea, (Color){240, 240, 240, 255});
        DrawRectangleLines((int)tableArea.x, (int)tableArea.y, (int)tableArea.width, (int)tableArea.height, GRAY);
        return;
    }

    int totalPages = (gRowCount + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE;
    if (totalPages < 1)
        totalPages = 1;
    gPageIndex = ClampInt(gPageIndex, 0, totalPages - 1);

    int startRow = gPageIndex * ROWS_PER_PAGE;
    int endRow = startRow + ROWS_PER_PAGE;
    if (endRow > gRowCount)
        endRow = gRowCount;

    int rowsShown = endRow - startRow;
    if (rowsShown < 1)
        rowsShown = 1;

    float whiteH = HEADER_H + TOP_PAD + (rowsShown * ROW_H) + 2.0f;
    if (whiteH > tableArea.height)
        whiteH = tableArea.height;

    DrawRectangleRec((Rectangle){tableArea.x, tableArea.y, tableArea.width, whiteH}, RAYWHITE);

    Rectangle header = {tableArea.x, tableArea.y, tableArea.width, HEADER_H};
    DrawRectangleRec(header, CLR_HEADER_BG);
    DrawRectangleLines((int)header.x, (int)header.y, (int)header.width, (int)header.height, BLACK);

    Rectangle content = {tableArea.x, tableArea.y + HEADER_H, tableArea.width, whiteH - HEADER_H};
    DrawRectangleLines((int)content.x, (int)content.y, (int)content.width, (int)content.height, BLACK);

    // kolom: ID Aksesoris Sales Kasir Pelanggan juml Tanggal Status Total Uang NoTrans Kembali Action
    float colW[] = {
        70,  // ID
        120, // NoTrans
        110, // Tanggal
        140, // Aksesoris
        120, // Sales
        120, // Kasir
        120, // Pelanggan
        50,  // juml
        130, // Total
        130, // Uang
        130, // Kembali
        110, // Status
    };
    const int COLS = sizeof(colW) / sizeof(colW[0]);

    const char *hdr[] = {
        "ID", "NoTrans", "Tanggal", "Aksesoris", "Sales", "Kasir",
        "Pelanggan", "JumlahProduk", "Total", "Uang", "Kembali", "Status"};
    float totalW = 0;
    for (int i = 0; i < COLS; i++)
        totalW += colW[i];

    float viewW = tableArea.width - 16;
    gMaxScrollX = totalW > viewW ? totalW - viewW : 0;

    Vector2 mouse = GetMousePosition();
    bool overTable = CheckCollisionPointRec(mouse, content) || CheckCollisionPointRec(mouse, header);

    if (overTable && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        gDraggingH = true;
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
        gDraggingH = false;

    if (gDraggingH && overTable)
    {
        Vector2 d = GetMouseDelta();
        if (d.x > 1 || d.x < -1)
            gScrollX -= d.x;
    }

    if (gScrollX < 0)
        gScrollX = 0;
    if (gScrollX > gMaxScrollX)
        gScrollX = gMaxScrollX;

    BeginScissorMode((int)tableArea.x, (int)tableArea.y, (int)tableArea.width, (int)whiteH);

    float baseX = tableArea.x + 8 - gScrollX;
    float x = baseX;

    for (int i = 0; i < COLS; i++)
    {
        DrawText(
            hdr[i],
            (int)x + 4,
            (int)(tableArea.y + (HEADER_H - FONT_HEADER) / 2),
            FONT_HEADER,
            RAYWHITE);
        x += colW[i];
    }

    float y0 = content.y + TOP_PAD;

    if (gClickTimer > 0)
        gClickTimer -= GetFrameTime();
    if (gClickTimer < 0)
        gClickTimer = 0;

    for (int row = startRow; row < endRow; row++)
    {
        float rowY = y0 + (row - startRow) * ROW_H;
        bool selected = (gSelected == row);

        Rectangle rowRect = {
            content.x + 2,
            rowY,
            content.width - 4,
            ROW_H - 2};

        DrawRectangleRec(rowRect, selected ? CLR_ROW_SEL : RAYWHITE);
        DrawRectangleLinesEx(rowRect, 1, CLR_GRID);

        const PenjualanAksesorisdata *p = &gRows[row];

        char rupTotal[64], rupUang[64], rupKembali[64];
        FormatRupiah(p->Total, rupTotal, sizeof(rupTotal));
        FormatRupiah(p->Uang, rupUang, sizeof(rupUang));
        FormatRupiah(p->Kembalian, rupKembali, sizeof(rupKembali));

        int textY = rowY + (ROW_H - FONT_ROW) / 2;
        float xx = baseX + 4;

        DrawText(p->PenjualanAksesorisID, xx, textY, FONT_ROW, BLACK);
        xx += colW[0];
        DrawText(p->NoTransaksi, xx, textY, FONT_ROW, BLACK);
        xx += colW[1];
        DrawText(p->TanggalTransaksi, xx, textY, FONT_ROW, BLACK);
        xx += colW[2];
        DrawText(p->AksesorisID, xx, textY, FONT_ROW, BLACK);
        xx += colW[3];
        DrawText(p->SalesNama, xx, textY, FONT_ROW, BLACK);
        xx += colW[4];
        DrawText(p->KasirNama, xx, textY, FONT_ROW, BLACK);
        xx += colW[5];
        DrawText(p->PelangganID, xx, textY, FONT_ROW, BLACK);
        xx += colW[6];
        DrawText(p->JumlahProduk, xx, textY, FONT_ROW, BLACK);
        xx += colW[7];
        DrawText(rupTotal, xx, textY, FONT_ROW, BLACK);
        xx += colW[8];
        DrawText(rupUang, xx, textY, FONT_ROW, BLACK);
        xx += colW[9];
        DrawText(rupKembali, xx, textY, FONT_ROW, BLACK);
        xx += colW[10];
        DrawText(p->StatusPembayaran, xx, textY, FONT_ROW, BLACK);
        xx += colW[11];

//         float actionX = xx;

//         float actionColCenter = actionX + colW[12] / 2 - 35;  // 70/2 = 35 (lebar tombol/2)
// Rectangle btnDel = {actionX + 0, rowY + 6, 70, 20};  // +10 = padding kiri

//         bool clickedAction = false;

//         if (SmallButton(btnDel, "Hapus", CLR_BTN_DEL, RAYWHITE))
//         {
//             clickedAction = true;
//             strncpy(gPendingDeleteId, p->PenjualanAksesorisID, sizeof(gPendingDeleteId) - 1);
//             gPendingDeleteId[sizeof(gPendingDeleteId) - 1] = '\0';
//             gModal = MODAL_CONFIRM_DELETE;
//         }

if (CheckCollisionPointRec(mouse, rowRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
{
    if (gClickTimer > 0 && gLastClickRow == row)
    {
        gSelected = row;
        gClickTimer = 0;
        gLastClickRow = -1;
    }
    else
    {
        gClickTimer = DOUBLE_CLICK_WINDOW;
        gLastClickRow = row;
    }
}
    }

    EndScissorMode();
}

void AdminPenjualanaksesorisPage(AppState *app, Rectangle contentArea, void *dbcPtr)
{
    if (!gUiInited)
    {
        gUiInited = 1;
        gNeedReload = 1;
        gReloadTimer = 0.0f;
    }

    gReloadTimer += GetFrameTime();
    if (gReloadTimer >= RELOAD_INTERVAL)
    {
        gReloadTimer = 0.0f;
        gNeedReload = 1;
    }

    if (gNeedReload)
    {
        ReloadRows(dbcPtr);
        gNeedReload = 0;

        if (gScrollX > gMaxScrollX)
            gScrollX = gMaxScrollX;
        if (gScrollX < 0)
            gScrollX = 0;
    }

    bool blocked = (gModal != MODAL_NONE);

    DrawText("Penjualan Aksesoris", (int)contentArea.x + 40, (int)contentArea.y + 35, FONT_TITLE, RAYWHITE);

    Rectangle tableArea = {
        contentArea.x + 40,
        contentArea.y + 90,
        contentArea.width - 80,
        HEADER_H + TOP_PAD + (ROWS_PER_PAGE * ROW_H) + 10};

    DrawTablePaged(tableArea, !blocked, dbcPtr);

    int totalPages = (gRowCount + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE;
    if (totalPages < 1)
        totalPages = 1;
    gPageIndex = ClampInt(gPageIndex, 0, totalPages - 1);

    float ctrlY = tableArea.y + tableArea.height + 10;

    Rectangle btnPrev = {tableArea.x + tableArea.width - 260, ctrlY, 120, 36};
    Rectangle btnNext = {tableArea.x + tableArea.width - 130, ctrlY, 120, 36};
    Rectangle btnAdd = {tableArea.x + tableArea.width - 390, ctrlY, 120, 36};

    if (!blocked)
    {
        if (SmallButton(btnAdd, "+ Add", CLR_BTN_ADD, RAYWHITE))
        {
            if (app)
                app->halamanSekarang = HAL_CASHIER_INPUT_PENJUALAN_AKSESORIS;
        }

        if (UIButton(btnPrev, "Prev", 18))
            if (gPageIndex > 0)
                gPageIndex--;

        if (UIButton(btnNext, "Next", 18))
            if (gPageIndex < totalPages - 1)
                gPageIndex++;
    }

    char pageInfo[80];
    snprintf(pageInfo, sizeof(pageInfo), "Page %d/%d | Rows: %d", gPageIndex + 1, totalPages, gRowCount);
    DrawText(pageInfo, (int)tableArea.x + 10, (int)ctrlY + 9, 18, RAYWHITE);

    if (gModal == MODAL_CONFIRM_DELETE)
    {
        UIModalResult r = UIDrawModalYesNo("Hapus data ini?", "Yes", "No", 18);
        if (r == UI_MODAL_YES)
        {
            void *dbc = NULL;
            if (dbcPtr)
                dbc = *(void **)dbcPtr;

            if (dbc && gPendingDeleteId[0] != '\0')
            {
                if (DbPenjualanAksesoris_Delete(dbc, gPendingDeleteId))
                {
                    gNeedReload = 1;
                    gSelected = -1;
                }
            }

            gPendingDeleteId[0] = '\0';
            gModal = MODAL_NONE;
        }
        else if (r == UI_MODAL_NO)
        {
            gPendingDeleteId[0] = '\0';
            gModal = MODAL_NONE;
        }
    }
}
