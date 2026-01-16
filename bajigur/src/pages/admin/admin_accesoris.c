#include "admin_accesoris.h"
#include "ui.h"
#include "textbox.h"
#include "db_accesoris.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define MAX_EMP 200

static Accessoris gEmp[MAX_EMP];
static int gEmpCount = 0;
static int gSelected = -1;
static int gScroll = 0;

/* table search */
static char gTableSearch[64] = "";
static UITextBox tbTableSearch;
static int gViewIdx[MAX_EMP];
static int gViewCount = 0;

static char gNamaAksesoris[50] = "";
static char gMerkAksesoris[64] = "";
static char gStok[20] = "";
static char gHarga[15] = "";

static int gUiInited = 0;
static UITextBox tbNamaAksesoris, tbMerkAksesoris, tbStok, tbHarga;

static int gNeedReload = 1;

/* ===== Popup + Toast ===== */
typedef enum {
    MODAL_NONE = 0,
    MODAL_ERR_INPUT,
    MODAL_CONFIRM_DELETE
} ModalType;

static ModalType gModal = MODAL_NONE;
static char gPendingDeleteId[8] = ""; // A00001

static char gToast[128] = "";
static float gToastTimer = 0.0f;

static const Color TOAST_GREEN = {0, 120, 0, 255};

static int ClampInt(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

/* Validate that string is a (non-negative) integer */
static int IsNumber(const char *p)
{
    if (!p || *p == '\0')
        return 0;
    const char *s = p;
    if (*s == '+')
        s++;
    if (!*s)
        return 0;
    while (*s)
    {
        if (!isdigit((unsigned char)*s))
            return 0;
        s++;
    }
    return 1;
}

static int ParseIntSafe(const char *p)
{
    if (!IsNumber(p))
        return 0;
    return (int)strtol(p, NULL, 10);
}

static void SetToast(const char *msg)
{
    if (!msg) msg = "";
    strncpy(gToast, msg, sizeof(gToast) - 1);
    gToast[sizeof(gToast) - 1] = '\0';
    gToastTimer = 2.5f;
}

static int ValidateAccessoryForm(void)
{
    if (strlen(gNamaAksesoris) == 0) return 0;
    if (strlen(gMerkAksesoris) == 0) return 0;
    if (strlen(gStok) == 0 || !IsNumber(gStok)) return 0;
    if (strlen(gHarga) == 0 || !IsNumber(gHarga)) return 0;

    int stokVal = ParseIntSafe(gStok);
    int hargaVal = ParseIntSafe(gHarga);
    if (stokVal < 0) return 0;
    if (hargaVal <= 0) return 0;

    return 1;
}

static void ClearForm(void)
{
    gNamaAksesoris[0] = gMerkAksesoris[0] = gStok[0] = gHarga[0] = '\0';
    tbNamaAksesoris.len = tbMerkAksesoris.len = tbStok.len = tbHarga.len = 0;
}

static void CopyToForm(const Accessoris *e)
{
    if (!e)
        return;

    strncpy(gNamaAksesoris, e->NamaAksesoris, sizeof(gNamaAksesoris) - 1);
    gNamaAksesoris[sizeof(gNamaAksesoris) - 1] = '\0';

    strncpy(gMerkAksesoris, e->MerkAksesoris, sizeof(gMerkAksesoris) - 1);
    gMerkAksesoris[sizeof(gMerkAksesoris) - 1] = '\0';

    snprintf(gStok, sizeof(gStok), "%d", e->Stok);
    snprintf(gHarga, sizeof(gHarga), "%d", e->Harga);

    tbNamaAksesoris.len = (int)strlen(gNamaAksesoris);
    tbMerkAksesoris.len = (int)strlen(gMerkAksesoris);
    tbStok.len = (int)strlen(gStok);
    tbHarga.len = (int)strlen(gHarga);
}

static void ReloadAccessoriss(void *dbcPtr)
{
    void *dbc = NULL;
    if (dbcPtr)
        dbc = *(void **)dbcPtr;

    int count = 0;
    if (DbAccesoriss_LoadAll(dbc, gEmp, MAX_EMP, &count))
    {
        gEmpCount = count;

        int visibleGuess = 8;
        int maxScroll = gEmpCount - visibleGuess;
        if (maxScroll < 0)
            maxScroll = 0;
        gScroll = ClampInt(gScroll, 0, maxScroll);

        if (gSelected >= gEmpCount)
            gSelected = -1;
    }
    else
    {
        gEmpCount = 0;
        gScroll = 0;
        gSelected = -1;
    }
}


static void BuildAccesorissView(void)
{
    gViewCount = 0;

    for (int i = 0; i < gEmpCount; i++)
    {
        int ok = 1;
        if (gTableSearch[0] != '\0')
        {
            char stokStr[16];
            char hargaStr[32];
            snprintf(stokStr, sizeof(stokStr), "%d", gEmp[i].Stok);
            snprintf(hargaStr, sizeof(hargaStr), "%d", gEmp[i].Harga);

            ok = UIStringContainsI(gEmp[i].AksesorisID, gTableSearch) ||
                 UIStringContainsI(gEmp[i].NamaAksesoris, gTableSearch) ||
                 UIStringContainsI(gEmp[i].MerkAksesoris, gTableSearch) ||
                 UIStringContainsI(stokStr, gTableSearch) ||
                 UIStringContainsI(hargaStr, gTableSearch);
        }

        if (ok)
        {
            if (gViewCount < MAX_EMP)
                gViewIdx[gViewCount++] = i;
        }
    }

    if (gSelected >= 0)
    {
        int found = 0;
        for (int v = 0; v < gViewCount; v++)
        {
            if (gViewIdx[v] == gSelected) { found = 1; break; }
        }
        if (!found)
        {
            gSelected = -1;
            ClearForm();
        }
    }

    if (gScroll < 0) gScroll = 0;
    if (gScroll > gViewCount) gScroll = gViewCount;
}


static void DrawAccessorissTable(Rectangle area, bool allowInteraction)
{
    DrawRectangleRec(area, RAYWHITE);
    DrawRectangleLines((int)area.x, (int)area.y, (int)area.width, (int)area.height, BLACK);

    float pad = 10.0f;
    float headerY = area.y + pad;
    float lineY = area.y + 38.0f;

    float colNo   = area.x + pad + 0;
    float colId   = area.x + pad + 50;
    float colName = area.x + pad + 200;
    float colMerk = area.x + pad + 430;
    float colStok = area.x + pad + 650;
    float colHarga= area.x + pad + 770;

    DrawText("No", (int)colNo, (int)headerY, 18, BLACK);
    DrawText("AccessoriesID", (int)colId, (int)headerY, 18, BLACK);
    DrawText("Accessories Name", (int)colName, (int)headerY, 18, BLACK);
    DrawText("Merk", (int)colMerk, (int)headerY, 18, BLACK);
    DrawText("Stock", (int)colStok, (int)headerY, 18, BLACK);
    DrawText("Price", (int)colHarga, (int)headerY, 18, BLACK);

    DrawLine((int)(area.x + 5), (int)lineY, (int)(area.x + area.width - 5), (int)lineY, BLACK);

    int rowH = 26;
    int visibleRows = (int)((area.height - 45.0f) / (float)rowH);
    if (visibleRows < 1) visibleRows = 1;

    int maxScroll = gViewCount - visibleRows;
    if (maxScroll < 0) maxScroll = 0;
    gScroll = ClampInt(gScroll, 0, maxScroll);

    int start = gScroll;
    int end = start + visibleRows;
    if (end > gViewCount) end = gViewCount;

    if (allowInteraction && CheckCollisionPointRec(GetMousePosition(), area))
    {
        float wheel = GetMouseWheelMove();
        if (wheel != 0)
        {
            gScroll -= (int)wheel;
            gScroll = ClampInt(gScroll, 0, maxScroll);
        }
    }

    int baseY = (int)(area.y + 45);

    for (int vi = start; vi < end; vi++)
    {
        int i = gViewIdx[vi];
        int drawRow = vi - start;
        Rectangle row = {
            area.x + 5,
            (float)(baseY + drawRow * rowH),
            area.width - 10,
            (float)rowH
        };

        if (i == gSelected)
        {
            DrawRectangleRec(row, (Color){200, 220, 255, 255});
        }

        if (allowInteraction && CheckCollisionPointRec(GetMousePosition(), row) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            if (gSelected == i)
            {
                gSelected = -1;
                ClearForm();
            }
            else
            {
                gSelected = i;
                CopyToForm(&gEmp[i]);
            }
        }

        char noStr[8];
        snprintf(noStr, sizeof(noStr), "%d", vi + 1);

        char stokStr[16];
        snprintf(stokStr, sizeof(stokStr), "%d", gEmp[i].Stok);

        char hargaFmt[64];
        UIFormatRupiahLL((long long)gEmp[i].Harga, hargaFmt, (int)sizeof(hargaFmt));

        DrawText(noStr, (int)colNo, (int)row.y + 4, 16, BLACK);
        DrawText(gEmp[i].AksesorisID, (int)colId, (int)row.y + 4, 16, BLACK);
        DrawText(gEmp[i].NamaAksesoris, (int)colName, (int)row.y + 4, 16, BLACK);
        DrawText(gEmp[i].MerkAksesoris, (int)colMerk, (int)row.y + 4, 16, BLACK);
        DrawText(stokStr, (int)colStok, (int)row.y + 4, 16, BLACK);
        DrawText(hargaFmt, (int)colHarga, (int)row.y + 4, 16, BLACK);
    }

    if (gViewCount == 0)
    {
        DrawText("No accessories data found.",
                 (int)area.x + 12,
                 (int)area.y + (int)area.height - 30,
                 16, GRAY);
    }
}

void AdminAccessorissPage(Rectangle contentArea, void *dbcPtr)
{
    void *dbc = NULL;
    if (dbcPtr)
        dbc = *(void **)dbcPtr;

    // toast timer
    if (gToastTimer > 0.0f) {
        gToastTimer -= GetFrameTime();
        if (gToastTimer <= 0.0f) {
            gToastTimer = 0.0f;
            gToast[0] = '\0';
        }
    }

    bool blocked = (gModal != MODAL_NONE);

    if (!gUiInited)
    {
        UITextBoxInit(&tbNamaAksesoris, (Rectangle){0, 0, 0, 0}, gNamaAksesoris, (int)sizeof(gNamaAksesoris), false);
        UITextBoxInit(&tbMerkAksesoris, (Rectangle){0, 0, 0, 0}, gMerkAksesoris, (int)sizeof(gMerkAksesoris), false);
        UITextBoxInit(&tbStok, (Rectangle){0, 0, 0, 0}, gStok, (int)sizeof(gStok), false);
        UITextBoxInit(&tbHarga, (Rectangle){0, 0, 0, 0}, gHarga, (int)sizeof(gHarga), false);
        UITextBoxInit(&tbTableSearch, (Rectangle){0, 0, 0, 0}, gTableSearch, (int)sizeof(gTableSearch), false);

        gUiInited = 1;
    }

    if (gNeedReload)
    {
        ReloadAccessoriss(dbcPtr);
        BuildAccesorissView();
        gNeedReload = 0;
    }

    DrawText("Accessoris Data", (int)contentArea.x + 40, (int)contentArea.y + 35, 28, RAYWHITE);

    Rectangle filterArea = { contentArea.x + 40, contentArea.y + 90, contentArea.width - 80, 44 };
    DrawRectangleRec(filterArea, (Color){200, 200, 200, 255});

    DrawText("Search", (int)filterArea.x + 12, (int)filterArea.y + 12, 18, BLACK);
    tbTableSearch.bounds = (Rectangle){ filterArea.x + 90, filterArea.y + 6, 360, 32 };
    if (!blocked) UITextBoxUpdate(&tbTableSearch);
    UITextBoxDraw(&tbTableSearch, 18);

    BuildAccesorissView();

    Rectangle tableArea = { contentArea.x + 40, filterArea.y + filterArea.height + 8, contentArea.width - 80, 280 - (44 + 8) };
    DrawAccessorissTable(tableArea, !blocked);

    Rectangle formArea = {
        contentArea.x + 40,
        tableArea.y + tableArea.height + 12,
        contentArea.width - 80,
        260};
    DrawRectangleRec(formArea, (Color){200, 200, 200, 255});

    // toast
    if (gToast[0] != '\0') {
        int tw = MeasureText(gToast, 18);
        DrawText(gToast, (int)(formArea.x + formArea.width - tw - 20), (int)(formArea.y + 6), 18, TOAST_GREEN);
    }

    float labelX = formArea.x + 20;
    float inputX = formArea.x + 180;
    float rowY = formArea.y + 18;

    float rowH = 40;
    float boxH = 32;
    float boxW1 = 360;
    float boxW2 = 220;

    DrawText("Accessories Name", (int)labelX, (int)rowY + 6, 18, BLACK);
    tbNamaAksesoris.bounds = (Rectangle){inputX, rowY, boxW1, boxH};

    rowY += rowH;
    DrawText("Brand", (int)labelX, (int)rowY + 6, 18, BLACK);
    tbMerkAksesoris.bounds = (Rectangle){inputX, rowY, boxW1, boxH};

    rowY += rowH;
    DrawText("Stock", (int)labelX, (int)rowY + 6, 18, BLACK);
    tbStok.bounds = (Rectangle){inputX, rowY, boxW2, boxH};

    rowY += rowH;
    DrawText("Price", (int)labelX, (int)rowY + 6, 18, BLACK);
    tbHarga.bounds = (Rectangle){inputX, rowY, boxW2, boxH};

    if (!blocked) {
        UITextBoxUpdate(&tbNamaAksesoris);
        UITextBoxUpdate(&tbMerkAksesoris);
        UITextBoxUpdate(&tbStok);
        UITextBoxUpdate(&tbHarga);
    }

    UITextBoxDraw(&tbNamaAksesoris, 18);
    UITextBoxDraw(&tbMerkAksesoris, 18);
    UITextBoxDraw(&tbStok, 18);
    UITextBoxDraw(&tbHarga, 18);

    float btnY = formArea.y + formArea.height + 18;

    Rectangle btnLeft1 = {formArea.x + 20, btnY, 120, 40};
    Rectangle btnLeft2 = {formArea.x + 160, btnY, 120, 40};
    Rectangle btnRight = {formArea.x + formArea.width - 140, btnY, 120, 40};

    if (!blocked)
    {
        if (gSelected < 0)
        {
            // ADD MODE: only Add + Clear
            if (UIButton(btnLeft1, "Clear", 18))
                ClearForm();

            if (UIButton(btnRight, "+ Add", 18))
            {
                if (!ValidateAccessoryForm())
                {
                    gModal = MODAL_ERR_INPUT;
                }
                else if (dbc)
                {
                    int stokVal = ParseIntSafe(gStok);
                    int hargaVal = ParseIntSafe(gHarga);
                    if (DbAccesoriss_Insert(dbc, gNamaAksesoris, gMerkAksesoris, stokVal, hargaVal))
                    {
                        ClearForm();
                        gNeedReload = 1;
                        SetToast("Data added successfully !");
                    }
                }
            }
        }
        else
        {
            // EDIT MODE: only Edit + Delete
            if (UIButton(btnLeft1, "Delete", 18))
            {
                if (gSelected >= 0 && gSelected < gEmpCount)
                {
                    strncpy(gPendingDeleteId, gEmp[gSelected].AksesorisID, sizeof(gPendingDeleteId) - 1);
                    gPendingDeleteId[sizeof(gPendingDeleteId) - 1] = '\0';
                    gModal = MODAL_CONFIRM_DELETE;
                }
            }

            if (UIButton(btnLeft2, "Edit", 18))
            {
                if (!ValidateAccessoryForm())
                {
                    gModal = MODAL_ERR_INPUT;
                }
                else if (dbc && gSelected >= 0 && gSelected < gEmpCount)
                {
                    int stokVal = ParseIntSafe(gStok);
                    int hargaVal = ParseIntSafe(gHarga);
                    if (DbAccesoriss_Update(dbc, gEmp[gSelected].AksesorisID, gNamaAksesoris, gMerkAksesoris, stokVal, hargaVal))
                    {
                        gNeedReload = 1;
                        SetToast("Data edited successfully !");
                    }
                }
            }
        }
    }

    if (gEmpCount == 0)
    {
        DrawText("No Accessoris data found.",
                 (int)tableArea.x + 12,
                 (int)tableArea.y + (int)tableArea.height - 30,
                 16, GRAY);
    }

    /* ===== MODAL DRAW (always last) ===== */
    if (gModal == MODAL_ERR_INPUT) {
        UIModalResult r = UIDrawModalOK("There is an incorrect input, please check again !", "OK", 18);
        if (r == UI_MODAL_OK) gModal = MODAL_NONE;
    } else if (gModal == MODAL_CONFIRM_DELETE) {
        UIModalResult r = UIDrawModalYesNo("Are you sure you want to delete?", "Yes", "No", 18);
        if (r == UI_MODAL_NO) {
            gModal = MODAL_NONE;
        } else if (r == UI_MODAL_YES) {
            if (dbc && gPendingDeleteId[0] != '\0') {
                if (DbAccesoriss_Delete(dbc, gPendingDeleteId)) {
                    gSelected = -1;
                    ClearForm();
                    gNeedReload = 1;
                }
            }
            gPendingDeleteId[0] = '\0';
            gModal = MODAL_NONE;
        }
    }
}
