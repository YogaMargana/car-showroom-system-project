#include "admin_carsdata.h"
#include "ui.h"
#include "textbox.h"
#include "db_cars.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define MAX_CARS 200

static CarData gCars[MAX_CARS];
static int gCarCount = 0;
static int gSelected = -1;
static int gScroll = 0;

/* table search */
static char gTableSearch[64] = "";
static UITextBox tbTableSearch;
static int gViewIdx[MAX_CARS];
static int gViewCount = 0;

static char gNama[32] = "";
static char gTipe[50] = "";
static char gStok[8] = "";
static char gTahun[8] = "";
static char gHarga[32] = "";

static UITextBox tbNama, tbTipe, tbStok, tbTahun, tbHarga;
static int gUiInited = 0;
static int gNeedReload = 1;

/* ===== Popup + Toast ===== */
typedef enum {
    MODAL_NONE = 0,
    MODAL_ERR_INPUT,
    MODAL_CONFIRM_DELETE
} ModalType;

static ModalType gModal = MODAL_NONE;
static char gPendingDeleteId[8] = ""; // MobilID (M00001)

static char gToast[128] = "";
static float gToastTimer = 0.0f;

static const Color TOAST_GREEN = {0, 120, 0, 255};

static void SetToast(const char *msg)
{
    strncpy(gToast, msg ? msg : "", sizeof(gToast) - 1);
    gToast[sizeof(gToast) - 1] = '\0';
    gToastTimer = 2.5f;
}

static int ClampInt(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void ClearForm(void)
{
    gNama[0] = gTipe[0] = gStok[0] = gTahun[0] = gHarga[0] = '\0';
    tbNama.len = tbTipe.len = tbStok.len = tbTahun.len = tbHarga.len = 0;
}

static void CopyToForm(const CarData *c)
{
    strncpy(gNama, c->NamaMobil, sizeof(gNama) - 1); gNama[sizeof(gNama) - 1] = '\0';
    strncpy(gTipe, c->TipeMobil, sizeof(gTipe) - 1); gTipe[sizeof(gTipe) - 1] = '\0';
    strncpy(gStok, c->Stok, sizeof(gStok) - 1);      gStok[sizeof(gStok) - 1] = '\0';
    strncpy(gTahun, c->TahunProduksi, sizeof(gTahun) - 1); gTahun[sizeof(gTahun) - 1] = '\0';
    long long hv = UIParseDigitsLL(c->Harga);
    snprintf(gHarga, sizeof(gHarga), "%lld", hv);
    tbNama.len = (int)strlen(gNama);
    tbTipe.len = (int)strlen(gTipe);
    tbStok.len = (int)strlen(gStok);
    tbTahun.len = (int)strlen(gTahun);
    tbHarga.len = (int)strlen(gHarga);
}

static void ReloadCars(void *dbcPtr)
{
    void *dbc = NULL;
    if (dbcPtr) dbc = *(void**)dbcPtr;

    int count = 0;
    if (DbCars_LoadAll(dbc, gCars, MAX_CARS, &count)) {
        gCarCount = count;

        int visibleGuess = 8;
        int maxScroll = gCarCount - visibleGuess;
        if (maxScroll < 0) maxScroll = 0;

        gScroll = ClampInt(gScroll, 0, maxScroll);
        if (gSelected >= gCarCount) gSelected = -1;
    } else {
        gCarCount = 0;
        gScroll = 0;
        gSelected = -1;
    }
}

static void BuildCarView(void)
{
    gViewCount = 0;

    for (int i = 0; i < gCarCount; i++)
    {
        int ok = 1;
        if (gTableSearch[0] != '\0')
        {
            ok = UIStringContainsI(gCars[i].MobilID, gTableSearch) ||
                 UIStringContainsI(gCars[i].NamaMobil, gTableSearch) ||
                 UIStringContainsI(gCars[i].TipeMobil, gTableSearch) ||
                 UIStringContainsI(gCars[i].Stok, gTableSearch) ||
                 UIStringContainsI(gCars[i].TahunProduksi, gTableSearch) ||
                 UIStringContainsI(gCars[i].Harga, gTableSearch);
        }

        if (ok)
        {
            if (gViewCount < MAX_CARS)
                gViewIdx[gViewCount++] = i;
        }
    }

    // deselect if selected row not visible
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

    // clamp scroll based on filtered view
    if (gScroll < 0) gScroll = 0;
    if (gScroll > gViewCount) gScroll = gViewCount;
}

/* Validate number (non-negative integer) */
static int IsNumber(const char *p)
{
    if (!p || *p == '\0') return 0;
    const char *s = p;
    if (*s == '+') s++;
    if (!*s) return 0;
    while (*s) {
        if (!isdigit((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

static int ParseIntSafe(const char *p)
{
    if (!IsNumber(p)) return -1;
    return (int)strtol(p, NULL, 10);
}

static int ValidateCarForm(void)
{
    if (strlen(gNama) == 0) return 0;
    if (strlen(gTipe) == 0) return 0;
    if (strlen(gStok) == 0 || !IsNumber(gStok)) return 0;
    if (strlen(gTahun) == 0 || !IsNumber(gTahun)) return 0;
    if (strlen(gHarga) == 0 || !IsNumber(gHarga)) return 0;

    int stok = ParseIntSafe(gStok);
    int tahun = ParseIntSafe(gTahun);
    int harga = ParseIntSafe(gHarga);

    if (stok < 0) return 0;
    if (tahun < 1900 || tahun > 2100) return 0;
    if (harga <= 0) return 0;

    return 1;
}

static void DrawCarsTable(Rectangle area, bool allowInteraction)
{
    DrawRectangleRec(area, RAYWHITE);
    DrawRectangleLines((int)area.x, (int)area.y, (int)area.width, (int)area.height, BLACK);

    float pad = 10.0f;
    float headerY = area.y + pad;
    float lineY = area.y + 38.0f;

    float colNo    = area.x + pad + 0;
    float colId    = area.x + pad + 50;
    float colName  = area.x + pad + 180;
    float colType  = area.x + pad + 420;
    float colStock = area.x + pad + 590;
    float colYear  = area.x + pad + 690;
    float colPrice = area.x + pad + 800;

    DrawText("No",    (int)colNo,    (int)headerY, 18, BLACK);
    DrawText("CarID", (int)colId,    (int)headerY, 18, BLACK);
    DrawText("Name",  (int)colName,  (int)headerY, 18, BLACK);
    DrawText("Type",  (int)colType,  (int)headerY, 18, BLACK);
    DrawText("Stock", (int)colStock, (int)headerY, 18, BLACK);
    DrawText("Year",  (int)colYear,  (int)headerY, 18, BLACK);
    DrawText("Price", (int)colPrice, (int)headerY, 18, BLACK);

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

    if (allowInteraction && CheckCollisionPointRec(GetMousePosition(), area)) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            gScroll -= (int)wheel;
            gScroll = ClampInt(gScroll, 0, maxScroll);
        }
    }

    int baseY = (int)(area.y + 45);

    for (int vi = start; vi < end; vi++) {
        int i = gViewIdx[vi];
        int drawRow = vi - start;
        Rectangle row = {
            area.x + 5,
            (float)(baseY + drawRow * rowH),
            area.width - 10,
            (float)rowH
        };

        if (i == gSelected) {
            DrawRectangleRec(row, (Color){200, 220, 255, 255});
        }

        if (allowInteraction && CheckCollisionPointRec(GetMousePosition(), row) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (gSelected == i) {
                gSelected = -1;
                ClearForm();
            } else {
                gSelected = i;
                CopyToForm(&gCars[i]);
            }
        }

        char noStr[8];
        snprintf(noStr, sizeof(noStr), "%d", vi + 1);

        char hargaFmt[64];
        UIFormatRupiahStr(gCars[i].Harga, hargaFmt, (int)sizeof(hargaFmt));

        DrawText(noStr,                  (int)colNo,    (int)row.y + 4, 16, BLACK);
        DrawText(gCars[i].MobilID,       (int)colId,    (int)row.y + 4, 16, BLACK);
        DrawText(gCars[i].NamaMobil,     (int)colName,  (int)row.y + 4, 16, BLACK);
        DrawText(gCars[i].TipeMobil,     (int)colType,  (int)row.y + 4, 16, BLACK);
        DrawText(gCars[i].Stok,          (int)colStock, (int)row.y + 4, 16, BLACK);
        DrawText(gCars[i].TahunProduksi, (int)colYear,  (int)row.y + 4, 16, BLACK);
        DrawText(hargaFmt,               (int)colPrice, (int)row.y + 4, 16, BLACK);
    }

    if (gViewCount == 0) {
        DrawText("No car data found.", (int)area.x + 12, (int)area.y + (int)area.height - 30, 16, GRAY);
    }
}

void AdminCarsDataPage(Rectangle contentArea, void *dbcPtr)
{
    void *dbc = NULL;
    if (dbcPtr) dbc = *(void**)dbcPtr;

    if (!gUiInited) {
        UITextBoxInit(&tbNama,  (Rectangle){0,0,0,0}, gNama,  (int)sizeof(gNama),  false);
        UITextBoxInit(&tbTipe,  (Rectangle){0,0,0,0}, gTipe,  (int)sizeof(gTipe),  false);
        UITextBoxInit(&tbStok,  (Rectangle){0,0,0,0}, gStok,  (int)sizeof(gStok),  false);
        UITextBoxInit(&tbTahun, (Rectangle){0,0,0,0}, gTahun, (int)sizeof(gTahun), false);
        UITextBoxInit(&tbHarga, (Rectangle){0,0,0,0}, gHarga, (int)sizeof(gHarga), false);
        UITextBoxInit(&tbTableSearch, (Rectangle){0,0,0,0}, gTableSearch, (int)sizeof(gTableSearch), false);
        gUiInited = 1;
    }

    if (gNeedReload) {
        ReloadCars(dbcPtr);
        BuildCarView();
        gNeedReload = 0;
    }

    if (gToastTimer > 0.0f) {
        gToastTimer -= GetFrameTime();
        if (gToastTimer <= 0.0f) {
            gToast[0] = '\0';
            gToastTimer = 0.0f;
        }
    }

    bool blocked = (gModal != MODAL_NONE);

    DrawText("Car Data", (int)contentArea.x + 40, (int)contentArea.y + 35, 28, RAYWHITE);

    Rectangle filterArea = { contentArea.x + 40, contentArea.y + 90, contentArea.width - 80, 44 };
    DrawRectangleRec(filterArea, (Color){200, 200, 200, 255});

    DrawText("Search", (int)filterArea.x + 12, (int)filterArea.y + 12, 18, BLACK);
    tbTableSearch.bounds = (Rectangle){ filterArea.x + 90, filterArea.y + 6, 360, 32 };

    if (!blocked) UITextBoxUpdate(&tbTableSearch);
    UITextBoxDraw(&tbTableSearch, 18);

    BuildCarView();

    Rectangle tableArea = { contentArea.x + 40, filterArea.y + filterArea.height + 8, contentArea.width - 80, 280 - (44 + 8) };
    DrawCarsTable(tableArea, !blocked);

    Rectangle formArea = {
        contentArea.x + 40,
        tableArea.y + tableArea.height + 12,
        contentArea.width - 80,
        260
    };
    DrawRectangleRec(formArea, (Color){200, 200, 200, 255});

    // toast
    if (gToast[0] != '\0') {
        int tw = MeasureText(gToast, 18);
        DrawText(gToast, (int)(formArea.x + formArea.width - tw - 20), (int)(formArea.y + 6), 18, TOAST_GREEN);
    }

    float rowY = formArea.y + 24;
    float labelX = formArea.x + 20;
    float inputStart = labelX + 140;

    DrawText("Name", (int)labelX, (int)rowY, 18, BLACK);
    tbNama.bounds = (Rectangle){ inputStart, rowY - 12, formArea.width - 200, 34 };

    rowY += 50;
    DrawText("Type", (int)labelX, (int)rowY, 18, BLACK);
    tbTipe.bounds = (Rectangle){ inputStart, rowY - 12, formArea.width - 200, 34 };

    rowY += 50;
    DrawText("Year", (int)labelX, (int)rowY, 18, BLACK);
    tbTahun.bounds = (Rectangle){ inputStart, rowY - 12, 140, 34 };

    DrawText("Stock", (int)(labelX + 320), (int)rowY, 18, BLACK);
    tbStok.bounds = (Rectangle){ labelX + 400, rowY - 12, 140, 34 };

    rowY += 50;
    DrawText("Price", (int)labelX, (int)rowY, 18, BLACK);
    tbHarga.bounds = (Rectangle){ inputStart, rowY - 12, 250, 34 };

    if (!blocked) {
        UITextBoxUpdate(&tbNama);
        UITextBoxUpdate(&tbTipe);
        UITextBoxUpdate(&tbTahun);
        UITextBoxUpdate(&tbStok);
        UITextBoxUpdate(&tbHarga);
    }

    UITextBoxDraw(&tbNama, 18);
    UITextBoxDraw(&tbTipe, 18);
    UITextBoxDraw(&tbTahun, 18);
    UITextBoxDraw(&tbStok, 18);
    UITextBoxDraw(&tbHarga, 18);

    float btnY = formArea.y + formArea.height + 18;

    Rectangle btnLeft1 = { formArea.x + 20,  btnY, 120, 40 };
    Rectangle btnLeft2 = { formArea.x + 160, btnY, 120, 40 };
    Rectangle btnRight = { formArea.x + formArea.width - 140, btnY, 120, 40 };

    if (!blocked) {
        if (gSelected < 0) {
            // ADD MODE: only Add + Clear
            if (UIButton(btnLeft1, "Clear", 18)) {
                ClearForm();
            }

            if (UIButton(btnRight, "+ Add", 18)) {
                if (!ValidateCarForm()) {
                    gModal = MODAL_ERR_INPUT;
                } else if (dbc) {
                    if (DbCars_Insert(dbc, gTipe, gNama, gStok, gTahun, gHarga)) {
                        ClearForm();
                        gNeedReload = 1;
                        SetToast("Data added successfully !");
                    }
                }
            }
        } else {
            // EDIT MODE: only Edit + Delete
            if (UIButton(btnLeft1, "Delete", 18)) {
                if (gSelected >= 0 && gSelected < gCarCount) {
                    strncpy(gPendingDeleteId, gCars[gSelected].MobilID, sizeof(gPendingDeleteId)-1);
                    gPendingDeleteId[sizeof(gPendingDeleteId)-1] = '\0';
                    gModal = MODAL_CONFIRM_DELETE;
                }
            }

            if (UIButton(btnLeft2, "Edit", 18)) {
                if (!ValidateCarForm()) {
                    gModal = MODAL_ERR_INPUT;
                } else if (dbc && gSelected >= 0 && gSelected < gCarCount) {
                    if (DbCars_Update(dbc,
                                      gCars[gSelected].MobilID,
                                      gTipe,
                                      gNama,
                                      gStok,
                                      gTahun,
                                      gHarga)) {
                        gNeedReload = 1;
                        SetToast("Data edited successfully !");
                    }
                }
            }
        }
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
                if (DbCars_Delete(dbc, gPendingDeleteId)) {
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
