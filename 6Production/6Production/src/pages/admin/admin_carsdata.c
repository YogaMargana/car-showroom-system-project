/* --- START OF FILE admin_carsdata.c --- */

#include "admin_carsdata.h"
#include "ui.h"
#include "textbox.h"
#include "db_cars.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define MAX_CARS 200

#define MAX_STOCK 30

static CarData gCars[MAX_CARS];
static int gCarCount = 0;
static int gSelected = -1;
static int gScroll = 0;

/* table search */
static char gTableSearch[64] = "";
static UITextBox tbTableSearch;
static int gViewIdx[MAX_CARS];
static int gViewCount = 0;

/* form */
static char gNama[32]  = "";
static char gTipe[50]  = "";
static char gStok[20]  = "";
static char gTahun[8]  = "";
static char gHarga[32] = "";

static UITextBox tbNama, tbStok, tbTahun, tbHarga;
static int gUiInited = 0;
static int gNeedReload = 1;

/* ===== Popup + Toast ===== */
typedef enum {
    MODAL_NONE = 0,
    MODAL_ERR_INPUT,
    MODAL_DUPLICATE,
    MODAL_STOCK_LIMIT,
    MODAL_CONFIRM_TOGGLE_ACTIVE
} ModalType;

typedef enum {
    ACT_NONE = 0,
    ACT_DEACTIVATE,
    ACT_ACTIVATE
} PendingAction;

static ModalType gModal = MODAL_NONE;
static PendingAction gPendingAction = ACT_NONE;
static char gPendingId[32] = ""; // MobilID

static char gToast[128] = "";
static float gToastTimer = 0.0f;

static const Color TOAST_GREEN = {0, 120, 0, 255};

/* ===== Type Selector ===== */
static const char *kCarTypes[] = {
    "MPV", "SUV", "HATCBACK", "SEDAN", "LCGC"
};
static const int kCarTypeCount = (int)(sizeof(kCarTypes) / sizeof(kCarTypes[0]));
static int gTypeIdx = 0;

static void StripFormat(char *out, const char *in);

static int StringEqualsI(const char *a, const char *b)
{
    if (!a || !b) return 0;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return (*a == '\0' && *b == '\0');
}

static int IsDuplicateCarData(const char *tipe, const char *nama, const char *tahunDigits, const char *excludeId)
{
    for (int i = 0; i < gCarCount; i++) {
        if (excludeId && excludeId[0] && strcmp(gCars[i].MobilID, excludeId) == 0) continue;

        char yDb[32];
        StripFormat(yDb, gCars[i].TahunProduksi);

        if (StringEqualsI(gCars[i].TipeMobil, tipe) &&
            StringEqualsI(gCars[i].NamaMobil, nama) &&
            strcmp(yDb, tahunDigits) == 0) {
            return 1;
        }
    }
    return 0;
}
/* --- Helper: Hapus karakter non-digit --- */
static void StripFormat(char *out, const char *in)
{
    if (!out || !in) return;
    const char *s = in;
    char *d = out;
    while (*s) {
        if (isdigit((unsigned char)*s)) {
            *d++ = *s;
        }
        s++;
    }
    *d = '\0';
}

/* --- Helper: Memastikan input hanya angka (In-Place) --- */
static void SanitizeDigits(char *buffer)
{
    char *src = buffer;
    char *dst = buffer;
    while (*src) {
        if (isdigit((unsigned char)*src)) {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
}

/* --- Helper: Format angka dengan titik ribuan (1000 -> 1.000) --- */
static void AutoFormatPrice(char *buffer, int maxLen)
{
    char raw[64];
    StripFormat(raw, buffer);

    int len = (int)strlen(raw);
    if (len == 0) { buffer[0] = '\0'; return; }

    int numDots = (len - 1) / 3;
    int newLen = len + numDots;
    if (newLen >= maxLen) return;

    char res[64];
    int rawIdx = len - 1;
    int resIdx = newLen - 1;
    int count = 0;

    res[newLen] = '\0';

    while (rawIdx >= 0) {
        res[resIdx--] = raw[rawIdx--];
        count++;
        if (count == 3 && rawIdx >= 0) {
            res[resIdx--] = '.';
            count = 0;
        }
    }

    strcpy(buffer, res);
}

/*
  Parse harga dari DB:
  "15000000" / "15.000.000" / "15.000.000,00" / "15000000.00"
  kalau ada desimal 2 digit, buang (divide 100)
*/
static long long ParseMoneyLLCars(const char *s)
{
    if (!s) return 0;

    int L = (int)strlen(s);
    while (L > 0 && isspace((unsigned char)s[L - 1])) L--;

    int hasCents = 0;
    if (L >= 3) {
        char sep = s[L - 3];
        char d1  = s[L - 2];
        char d2  = s[L - 1];
        if ((sep == ',' || sep == '.') &&
            isdigit((unsigned char)d1) &&
            isdigit((unsigned char)d2)) {
            hasCents = 1;
        }
    }

    long long v = UIParseDigitsLL(s);
    if (hasCents) v /= 100;
    return v;
}

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
    gNama[0] = gStok[0] = gTahun[0] = gHarga[0] = '\0';

    gTypeIdx = 0;
    strncpy(gTipe, kCarTypes[gTypeIdx], sizeof(gTipe) - 1);
    gTipe[sizeof(gTipe) - 1] = '\0';

    tbNama.len = tbStok.len = tbTahun.len = tbHarga.len = 0;
}

static void CopyToForm(const CarData *c)
{
    if (!c) return;

    strncpy(gNama, c->NamaMobil, sizeof(gNama) - 1);
    gNama[sizeof(gNama) - 1] = '\0';

    gTypeIdx = 0;
    int found = 0;
    for (int t = 0; t < kCarTypeCount; t++) {
        if (StringEqualsI(c->TipeMobil, kCarTypes[t])) {
            gTypeIdx = t;
            found = 1;
            break;
        }
    }
    if (found) {
        strncpy(gTipe, kCarTypes[gTypeIdx], sizeof(gTipe) - 1);
    } else {
        strncpy(gTipe, c->TipeMobil, sizeof(gTipe) - 1);
    }
    gTipe[sizeof(gTipe) - 1] = '\0';

    strncpy(gStok, c->Stok, sizeof(gStok) - 1);
    gStok[sizeof(gStok) - 1] = '\0';
    SanitizeDigits(gStok);

    strncpy(gTahun, c->TahunProduksi, sizeof(gTahun) - 1);
    gTahun[sizeof(gTahun) - 1] = '\0';
    SanitizeDigits(gTahun);

    long long hv = ParseMoneyLLCars(c->Harga);
    snprintf(gHarga, sizeof(gHarga), "%lld", hv);
    AutoFormatPrice(gHarga, (int)sizeof(gHarga));

    tbNama.len  = (int)strlen(gNama);
    tbStok.len  = (int)strlen(gStok);
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

        /* SELALU tampilkan ACTIVE saja */
        if (gCars[i].IsActive == 0) ok = 0;

        if (ok && gTableSearch[0] != '\0')
{
    ok = UIStringContainsI(gCars[i].MobilID, gTableSearch) ||   /* <-- tambah ini */
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

/* Validate number */
static int IsNumber(const char *p)
{
    if (!p || *p == '\0') return 0;
    char temp[64];
    StripFormat(temp, p);
    return (strlen(temp) > 0);
}

static int ParseIntSafe(const char *p)
{
    char temp[64];
    StripFormat(temp, p);
    if (strlen(temp) == 0) return -1;
    return (int)strtol(temp, NULL, 10);
}

static long long ParseLLSafe(const char *p)
{
    char temp[64];
    StripFormat(temp, p);
    if (strlen(temp) == 0) return -1;
    return strtoll(temp, NULL, 10);
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
    long long harga = ParseLLSafe(gHarga);

    if (stok < 0) return 0;
    if (tahun < 1900 || tahun > 2100) return 0;
    if (harga <= 0) return 0;

    return 1;
}

static void DrawTypeSelector(Rectangle bounds, bool allowInteraction)
{
    float btnW = 36.0f;

    Rectangle left  = (Rectangle){ bounds.x, bounds.y, btnW, bounds.height };
    Rectangle right = (Rectangle){ bounds.x + bounds.width - btnW, bounds.y, btnW, bounds.height };
    Rectangle mid   = (Rectangle){ bounds.x + btnW, bounds.y, bounds.width - 2*btnW, bounds.height };

    DrawRectangleRec(mid, RAYWHITE);
    DrawRectangleLines((int)mid.x, (int)mid.y, (int)mid.width, (int)mid.height, BLACK);

    DrawRectangleRec(left, (Color){220,220,220,255});
    DrawRectangleLines((int)left.x, (int)left.y, (int)left.width, (int)left.height, BLACK);

    DrawRectangleRec(right, (Color){220,220,220,255});
    DrawRectangleLines((int)right.x, (int)right.y, (int)right.width, (int)right.height, BLACK);

    DrawText("<", (int)(left.x + 12), (int)(left.y + 8), 18, BLACK);
    DrawText(">", (int)(right.x + 12), (int)(right.y + 8), 18, BLACK);

    DrawText(gTipe, (int)(mid.x + 10), (int)(mid.y + 8), 18, BLACK);

    if (!allowInteraction) return;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 m = GetMousePosition();
        if (CheckCollisionPointRec(m, left)) {
            gTypeIdx = (gTypeIdx - 1 + kCarTypeCount) % kCarTypeCount;
            strncpy(gTipe, kCarTypes[gTypeIdx], sizeof(gTipe) - 1);
            gTipe[sizeof(gTipe) - 1] = '\0';
        } else if (CheckCollisionPointRec(m, right)) {
            gTypeIdx = (gTypeIdx + 1) % kCarTypeCount;
            strncpy(gTipe, kCarTypes[gTypeIdx], sizeof(gTipe) - 1);
            gTipe[sizeof(gTipe) - 1] = '\0';
        }
    }
}

/* TABLE: tanpa CarID & Status */
static void DrawCarsTable(Rectangle area, bool allowInteraction)
{
    DrawRectangleRec(area, RAYWHITE);
    DrawRectangleLines((int)area.x, (int)area.y, (int)area.width, (int)area.height, BLACK);

    float pad = 10.0f;
    float headerY = area.y + pad;
    float lineY = area.y + 38.0f;

   /* widths biar gampang diatur */
float wNo    = 50.0f;
float wId    = 110.0f;   /* lebar MobilID */
float wName  = 260.0f;
float wType  = 160.0f;
float wStock = 90.0f;
float wYear  = 90.0f;

float colNo    = area.x + pad;
float colId    = colNo   + wNo;
float colName  = colId   + wId;
float colType  = colName + wName;
float colStock = colType + wType;
float colYear  = colStock + wStock;
float colPrice = colYear + wYear;

DrawText("No",     (int)colNo,   (int)headerY, 18, BLACK);
DrawText("MobilID",(int)colId,   (int)headerY, 18, BLACK);   /* <-- baru */
DrawText("Name",   (int)colName, (int)headerY, 18, BLACK);
DrawText("Type",   (int)colType, (int)headerY, 18, BLACK);
DrawText("Stock",  (int)colStock,(int)headerY, 18, BLACK);
DrawText("Year",   (int)colYear, (int)headerY, 18, BLACK);
DrawText("Price",  (int)colPrice,(int)headerY, 18, BLACK);


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
        long long hv = ParseMoneyLLCars(gCars[i].Harga);
        UIFormatRupiahLL(hv, hargaFmt, (int)sizeof(hargaFmt));

       DrawText(noStr,                  (int)colNo,    (int)row.y + 4, 16, BLACK);
        DrawText(gCars[i].MobilID,       (int)colId,    (int)row.y + 4, 16, BLACK); /* <-- baru */
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
        UITextBoxInit(&tbStok,  (Rectangle){0,0,0,0}, gStok,  (int)sizeof(gStok),  false);
        UITextBoxInit(&tbTahun, (Rectangle){0,0,0,0}, gTahun, (int)sizeof(gTahun), false);
        UITextBoxInit(&tbHarga, (Rectangle){0,0,0,0}, gHarga, (int)sizeof(gHarga), false);
        UITextBoxInit(&tbTableSearch, (Rectangle){0,0,0,0}, gTableSearch, (int)sizeof(gTableSearch), false);

        ClearForm();
        gUiInited = 1;
    }

    // F5 = paksa reload data dari DB
    if (IsKeyPressed(KEY_F5)) gNeedReload = 1;

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

    Rectangle tableArea = {
        contentArea.x + 40,
        filterArea.y + filterArea.height + 8,
        contentArea.width - 80,
        280 - (44 + 8)
    };
    DrawCarsTable(tableArea, !blocked);

    Rectangle formArea = {
        contentArea.x + 40,
        tableArea.y + tableArea.height + 12,
        contentArea.width - 80,
        260
    };
    DrawRectangleRec(formArea, (Color){200, 200, 200, 255});

    if (gToast[0] != '\0') {
        int tw = MeasureText(gToast, 18);
        DrawText(gToast, (int)(formArea.x + formArea.width - tw - 20), (int)(formArea.y + 6), 18, TOAST_GREEN);
    }

    float rowY = formArea.y + 24;
    float labelX = formArea.x + 20;
    float inputStart = labelX + 140;

    DrawText("Name", (int)labelX, (int)rowY, 18, BLACK);
    tbNama.bounds = (Rectangle){ inputStart, rowY - 12, formArea.width - 500, 34 };

    rowY += 50;
    DrawText("Type", (int)labelX, (int)rowY, 18, BLACK);
    Rectangle typeBounds = (Rectangle){ inputStart, rowY - 12, formArea.width - 500, 34 };

    rowY += 50;
    DrawText("Year", (int)labelX, (int)rowY, 18, BLACK);
    tbTahun.bounds = (Rectangle){ inputStart, rowY - 12, 140, 34 };

    DrawText("Stock", (int)(labelX + 320), (int)rowY, 18, BLACK);
    tbStok.bounds = (Rectangle){ labelX + 400, rowY - 12, 140, 34 };

    rowY += 50;
    DrawText("Price", (int)labelX, (int)rowY, 18, BLACK);

    float rpWidth = 35.0f;
    DrawText("Rp.", (int)inputStart, (int)(rowY - 6), 18, BLACK);
    tbHarga.bounds = (Rectangle){ inputStart + rpWidth, rowY - 12, 250 - rpWidth, 34 };

    if (!blocked) {
        UITextBoxUpdate(&tbNama);

        UITextBoxUpdate(&tbTahun);
        SanitizeDigits(gTahun);
        tbTahun.len = (int)strlen(gTahun);

        UITextBoxUpdate(&tbStok);
        SanitizeDigits(gStok);
        tbStok.len = (int)strlen(gStok);

        UITextBoxUpdate(&tbHarga);
        AutoFormatPrice(gHarga, (int)sizeof(gHarga));
        tbHarga.len = (int)strlen(gHarga);
    }

    UITextBoxDraw(&tbNama, 18);
    DrawTypeSelector(typeBounds, !blocked);
    UITextBoxDraw(&tbTahun, 18);
    UITextBoxDraw(&tbStok, 18);
    UITextBoxDraw(&tbHarga, 18);

    float btnY = formArea.y + formArea.height + 18;

    Rectangle btnLeft1 = { formArea.x + 20,  btnY, 140, 40 };
    Rectangle btnLeft2 = { formArea.x + 180, btnY, 120, 40 };
    Rectangle btnRight = { formArea.x + formArea.width - 140, btnY, 120, 40 };

    if (!blocked) {
        if (gSelected < 0) {
            if (UIButton(btnLeft1, "Clear", 18)) {
                ClearForm();
            }

            if (UIButton(btnRight, "+ Add", 18)) {
                if (!ValidateCarForm()) {
                    gModal = MODAL_ERR_INPUT;
                } else {
                    int stokVal = ParseIntSafe(gStok);
                    if (stokVal > MAX_STOCK) {
                        gModal = MODAL_STOCK_LIMIT;
                    } else if (dbc) {
                        char stokRaw[32], tahunRaw[16], hargaRaw[64];
                        StripFormat(stokRaw, gStok);
                        StripFormat(tahunRaw, gTahun);
                        StripFormat(hargaRaw, gHarga);

                        if (IsDuplicateCarData(gTipe, gNama, tahunRaw, NULL)) {
                            gModal = MODAL_DUPLICATE;
                        }
                        else if (DbCars_Insert(dbc, gTipe, gNama, stokRaw, tahunRaw, hargaRaw)) {
                            ClearForm();
                            gNeedReload = 1;
                            SetToast("Data added successfully !");
                        } else {
                            SetToast("Insert failed!");
                        }
                    }
                }
            }
        } else {
            /* Karena tabel hanya ACTIVE, tombol ini selalu Deactivate */
            if (UIButton(btnLeft1, "Delete", 18)) {
                if (gSelected >= 0 && gSelected < gCarCount) {
                    strncpy(gPendingId, gCars[gSelected].MobilID, sizeof(gPendingId)-1);
                    gPendingId[sizeof(gPendingId)-1] = '\0';
                    gPendingAction = ACT_DEACTIVATE;
                    gModal = MODAL_CONFIRM_TOGGLE_ACTIVE;
                }
            }

            if (UIButton(btnLeft2, "Edit", 18)) {
                if (!ValidateCarForm()) {
                    gModal = MODAL_ERR_INPUT;
                } else {
                    int stokVal = ParseIntSafe(gStok);
                    if (stokVal > MAX_STOCK) {
                        gModal = MODAL_STOCK_LIMIT;
                    } else if (dbc && gSelected >= 0 && gSelected < gCarCount) {
                        char stokRaw[32], tahunRaw[16], hargaRaw[64];
                        StripFormat(stokRaw, gStok);
                        StripFormat(tahunRaw, gTahun);
                        StripFormat(hargaRaw, gHarga);

                        if (IsDuplicateCarData(gTipe, gNama, tahunRaw, gCars[gSelected].MobilID)) {
                            gModal = MODAL_DUPLICATE;
                        }
                        else if (DbCars_Update(dbc,
                                          gCars[gSelected].MobilID,
                                          gTipe,
                                          gNama,
                                          stokRaw,
                                          tahunRaw,
                                          hargaRaw)) {
                            gNeedReload = 1;
                            SetToast("Data edited successfully !");
                        } else {
                            SetToast("Update failed!");
                        }
                    }
                }
            }
        }
    }

    /* ===== MODAL DRAW ===== */
    if (gModal == MODAL_ERR_INPUT) {
        UIModalResult r = UIDrawModalOK("There is an incorrect input, please check again !", "OK", 18);
        if (r == UI_MODAL_OK) gModal = MODAL_NONE;
    }
    else if (gModal == MODAL_DUPLICATE) {
        UIModalResult r = UIDrawModalOK("Data must not be the same !", "OK", 18);
        if (r == UI_MODAL_OK) gModal = MODAL_NONE;
    }
    else if (gModal == MODAL_STOCK_LIMIT) {
        UIModalResult r = UIDrawModalOK("Batas stok 30!", "OK", 18);
        if (r == UI_MODAL_OK) gModal = MODAL_NONE;
    }
    else if (gModal == MODAL_CONFIRM_TOGGLE_ACTIVE) {
        const char *msg = "Are you sure you want to delete?";

        UIModalResult r = UIDrawModalYesNo(msg, "Ya", "Tidak", 18);
        if (r == UI_MODAL_NO) {
            gModal = MODAL_NONE;
        } else if (r == UI_MODAL_YES) {
            int ok = 0;
            if (dbc && gPendingId[0] != '\0') {
                ok = DbCars_Deactivate(dbc, gPendingId);
                if (ok) SetToast("Mobil Deleted (Deactivated)");
                else    SetToast("Delete failed!");
            }

            if (ok) {
                gSelected = -1;
                ClearForm();
                gNeedReload = 1;
            }

            gPendingId[0] = '\0';
            gPendingAction = ACT_NONE;
            gModal = MODAL_NONE;
        }
    }
}