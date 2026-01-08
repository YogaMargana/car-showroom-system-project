#include "sales_test_drive.h"
#include "ui.h"
#include "textbox.h"
#include "db_testdrive.h"
#include "db_lookup.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

#define MAX_TD      200
#define MAX_LOOKUP  250
#define MAX_VIEW    MAX_TD

static TestDrive gTd[MAX_TD];
static int gTdCount = 0;

static int gSelected = -1;   /* index into gTd */
static int gScroll = 0;      /* scroll on filtered view */

/* ===== Lookups for searchable pickers ===== */
static LookupItem gMobils[MAX_LOOKUP];
static int gMobilCount = 0;

static LookupItem gCustomers[MAX_LOOKUP];
static int gCustomerCount = 0;

static LookupItem gSalesEmployees[MAX_LOOKUP];
static int gSalesCount = 0;

/* ===== Filtered view indices ===== */
static int gViewIdx[MAX_VIEW];
static int gViewCount = 0;

/* ===== Form buffers ===== */
static char gMobilID[16] = "";
static char gKaryawanID[16] = "";
static char gPelangganID[16] = "";
static char gTanggal[16] = ""; /* YYYY-MM-DD */

/* Originals snapshot (for edit-only conditional validation) */
static char gOrigKaryawanID[16] = "";
static char gOrigTanggal[16] = "";

/* Status select (left/right) */
static const char *kStatuses[] = {"Schedule", "On Going", "Success"};
static const int kStatusCount = 3;
static int gStatusIndex = 0;

/* ===== UI inputs ===== */
static UITextBox tbMobil, tbKaryawan, tbPelanggan, tbTanggal;
static UITextBox tbTableSearch;

static char gTableSearch[64] = "";

/* Status filter dropdown (table) */
static const char *kStatusFilterItems[] = {"All", "Schedule", "On Going", "Success"};
static const int kStatusFilterCount = 4;
static int gStatusFilterIndex = 0;
static bool gStatusFilterOpen = false;

/* Picker open states */
static bool gPickMobilOpen = false;
static bool gPickKaryawanOpen = false;
static bool gPickPelangganOpen = false;

static int gUiInited = 0;
static int gNeedReload = 1;

/* ===== Popup + Toast ===== */
typedef enum {
    MODAL_NONE = 0,
    MODAL_ERR_INPUT,
    MODAL_CONFIRM_DELETE
} ModalType;

static ModalType gModal = MODAL_NONE;
static char gPendingDeleteId[16] = ""; /* TDID */

static char gToast[128] = "";
static float gToastTimer = 0.0f;

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

static void CloseAllPickers(void)
{
    gPickMobilOpen = false;
    gPickKaryawanOpen = false;
    gPickPelangganOpen = false;
}

static void ClearForm(void)
{
    gMobilID[0] = gKaryawanID[0] = gPelangganID[0] = gTanggal[0] = '\0';
    gOrigKaryawanID[0] = gOrigTanggal[0] = '\0';
    tbMobil.len = tbKaryawan.len = tbPelanggan.len = tbTanggal.len = 0;
    gStatusIndex = 0;
    CloseAllPickers();
}

static int StatusToIndex(const char *s)
{
    for (int i = 0; i < kStatusCount; i++) {
        if (s && strcmp(s, kStatuses[i]) == 0) return i;
    }
    return 0;
}

static void CopyToForm(const TestDrive *td)
{
    strncpy(gMobilID, td->MobilID, sizeof(gMobilID) - 1);           gMobilID[sizeof(gMobilID) - 1] = '\0';
    strncpy(gKaryawanID, td->KaryawanID, sizeof(gKaryawanID) - 1);  gKaryawanID[sizeof(gKaryawanID) - 1] = '\0';
    strncpy(gPelangganID, td->PelangganID, sizeof(gPelangganID) - 1); gPelangganID[sizeof(gPelangganID) - 1] = '\0';
    strncpy(gTanggal, td->Tanggal, sizeof(gTanggal) - 1);           gTanggal[sizeof(gTanggal) - 1] = '\0';

    /* Snapshot originals for conditional validation on edit */
    strncpy(gOrigKaryawanID, gKaryawanID, sizeof(gOrigKaryawanID) - 1);
    gOrigKaryawanID[sizeof(gOrigKaryawanID) - 1] = '\0';
    strncpy(gOrigTanggal, gTanggal, sizeof(gOrigTanggal) - 1);
    gOrigTanggal[sizeof(gOrigTanggal) - 1] = '\0';

    gStatusIndex = StatusToIndex(td->Status);

    tbMobil.len = (int)strlen(gMobilID);
    tbKaryawan.len = (int)strlen(gKaryawanID);
    tbPelanggan.len = (int)strlen(gPelangganID);
    tbTanggal.len = (int)strlen(gTanggal);

    CloseAllPickers();
}

static void ReloadTestDrives(void *dbcPtr)
{
    void *dbc = NULL;
    if (dbcPtr) dbc = *(void**)dbcPtr;

    int count = 0;
    if (DbTestDrive_LoadAll(dbc, gTd, MAX_TD, &count)) {
        gTdCount = count;
        if (gSelected >= gTdCount) gSelected = -1;
    } else {
        gTdCount = 0;
        gSelected = -1;
    }
}

static void ReloadLookups(void *dbcPtr)
{
    void *dbc = NULL;
    if (dbcPtr) dbc = *(void**)dbcPtr;
    if (!dbc) return;

    (void)DbLookup_LoadMobilList(dbc, gMobils, MAX_LOOKUP, &gMobilCount);
    (void)DbLookup_LoadCustomerList(dbc, gCustomers, MAX_LOOKUP, &gCustomerCount);
    (void)DbLookup_LoadSalesEmployeeList(dbc, gSalesEmployees, MAX_LOOKUP, &gSalesCount);
}

static int StrContainsI(const char *hay, const char *needle)
{
    if (!needle || needle[0] == '\0') return 1;
    if (!hay) return 0;

    char nlow[128];
    size_t nl = strlen(needle);
    if (nl >= sizeof(nlow)) nl = sizeof(nlow) - 1;
    for (size_t i = 0; i < nl; i++) nlow[i] = (char)tolower((unsigned char)needle[i]);
    nlow[nl] = '\0';

    const char *p = hay;
    while (*p) {
        size_t j = 0;
        while (p[j] && nlow[j] && (char)tolower((unsigned char)p[j]) == nlow[j]) j++;
        if (nlow[j] == '\0') return 1;
        p++;
    }
    return 0;
}

static int IsIdInList(const char *id, const LookupItem *list, int count)
{
    if (!id || id[0] == '\0') return 0;

    char tmp[32];
    strncpy(tmp, id, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = '\0';
    TrimInPlace(tmp);

    for (int i = 0; i < count; i++) {
        if (StrEqI(tmp, list[i].id)) return 1;
    }
    return 0;
}

static int ParseDateYMD(const char *s, int *y, int *m, int *d)
{
    if (!s) return 0;
    int yy = 0, mm = 0, dd = 0;
    if (sscanf(s, "%d-%d-%d", &yy, &mm, &dd) != 3) return 0;
    if (yy < 1900 || yy > 2100) return 0;
    if (mm < 1 || mm > 12) return 0;
    if (dd < 1 || dd > 31) return 0;

    /* Validate calendar date using mktime normalization check */
    struct tm tmv;
    memset(&tmv, 0, sizeof(tmv));
    tmv.tm_year = yy - 1900;
    tmv.tm_mon  = mm - 1;
    tmv.tm_mday = dd;   
    tmv.tm_isdst = -1;

    time_t t = mktime(&tmv);
    if (t == (time_t)-1) return 0;

    struct tm *chk = localtime(&t);
    if (!chk) return 0;
    if (chk->tm_year != tmv.tm_year || chk->tm_mon != tmv.tm_mon || chk->tm_mday != tmv.tm_mday) return 0;

    if (y) *y = yy;
    if (m) *m = mm;
    if (d) *d = dd;
    return 1;
}


static int IsDateValid(const char *s)
{
    int y=0,m=0,d=0;
    return ParseDateYMD(s, &y, &m, &d);
}

static int IsDateTodayOrFuture(const char *s)
{
    int y, m, d;
    if (!ParseDateYMD(s, &y, &m, &d)) return 0;

    time_t now = time(NULL); // date TD ambil dari raw time
    struct tm *lt = localtime(&now);
    if (!lt) return 0;

    int cy = lt->tm_year + 1900;
    int cm = lt->tm_mon + 1;
    int cd = lt->tm_mday;

    if (y < cy) return 0;
    if (y == cy && m < cm) return 0;
    if (y == cy && m == cm && d < cd) return 0;
    return 1;
}

static void BuildView(void)
{
    gViewCount = 0;

    const char *q = gTableSearch;

    const char *statusFilter = NULL;
    if (gStatusFilterIndex > 0 && gStatusFilterIndex < kStatusFilterCount) {
        statusFilter = kStatusFilterItems[gStatusFilterIndex];
    }

    for (int i = 0; i < gTdCount; i++) {
        if (statusFilter && strcmp(gTd[i].Status, statusFilter) != 0) continue;

        if (q && q[0]) {
            if (!StrContainsI(gTd[i].TestDriveID, q) &&
                !StrContainsI(gTd[i].MobilID, q) &&
                !StrContainsI(gTd[i].KaryawanID, q) &&
                !StrContainsI(gTd[i].PelangganID, q) &&
                !StrContainsI(gTd[i].Tanggal, q) &&
                !StrContainsI(gTd[i].Status, q)) {
                continue;
            }
        }

        if (gViewCount < MAX_VIEW) gViewIdx[gViewCount++] = i;
    }

    /* If selected item isn't visible, auto-deselect to avoid confusion */
    if (gSelected >= 0) {
        int found = 0;
        for (int i = 0; i < gViewCount; i++) {
            if (gViewIdx[i] == gSelected) { found = 1; break; }
        }
        if (!found) {
            gSelected = -1;
            ClearForm();
        }
    }
}

static void DrawToast(Rectangle anchor)
{
    if (gToastTimer <= 0.0f || gToast[0] == '\0') return;
    gToastTimer -= GetFrameTime();
    if (gToastTimer <= 0.0f) {
        gToast[0] = '\0';
        return;
    }

    int fs = 18;
    int pad = 12;
    int w = MeasureText(gToast, fs) + pad * 2;
    int h = 36;

    Rectangle r = { anchor.x + anchor.width - (float)w - 20.0f, anchor.y + anchor.height - (float)h - 12.0f, (float)w, (float)h };
    DrawRectangleRec(r, (Color){0, 110, 0, 255});
    DrawRectangleLines((int)r.x, (int)r.y, (int)r.width, (int)r.height, BLACK);
    DrawText(gToast, (int)r.x + pad, (int)r.y + 8, fs, RAYWHITE);
}

/* Searchable picker dropdown (filtered by query typed in textbox) */
static void DrawPicker(Rectangle tbBounds,
                       UITextBox *tb,
                       char *outId,
                       int outIdSize,
                       const LookupItem *items,
                       int itemCount,
                       bool *openFlag,
                       float maxWidth)
{
    Vector2 m = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(m, tbBounds)) {
        CloseAllPickers();
        *openFlag = true;
        tb->active = true;
    }

    /* Close when click outside textbox + dropdown */
    /* dropdown rect computed below; if open, allow click inside */

    if (!(*openFlag)) return;

    const char *q = tb->buffer;
    int matches[12];
    int mc = 0;
    for (int i = 0; i < itemCount && mc < (int)(sizeof(matches)/sizeof(matches[0])); i++) {
        if (!q || q[0] == '\0' || StrContainsI(items[i].id, q) || StrContainsI(items[i].label, q)) {
            matches[mc++] = i;
        }
    }

    float rowH = 26.0f;
    float dropW = tbBounds.width;
    if (maxWidth > dropW) dropW = maxWidth;
    float dropH = (float)(mc == 0 ? 1 : mc) * rowH + 10.0f;

    Rectangle drop = { tbBounds.x, tbBounds.y + tbBounds.height + 6.0f, dropW, dropH };

    /* Close on outside click */
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        bool inside = CheckCollisionPointRec(m, tbBounds) || CheckCollisionPointRec(m, drop);
        if (!inside) {
            *openFlag = false;
            tb->active = false;
            return;
        }
    }

    DrawRectangleRec(drop, RAYWHITE);
    DrawRectangleLines((int)drop.x, (int)drop.y, (int)drop.width, (int)drop.height, BLACK);

    if (mc == 0) {
        DrawText("No results", (int)drop.x + 10, (int)drop.y + 8, 16, GRAY);
        return;
    }

    for (int r = 0; r < mc; r++) {
        int idx = matches[r];
        Rectangle row = { drop.x + 5, drop.y + 5 + (float)r * rowH, drop.width - 10, rowH };

        bool hover = CheckCollisionPointRec(m, row);
        if (hover) DrawRectangleRec(row, (Color){220, 230, 255, 255});

        DrawText(items[idx].label, (int)row.x + 8, (int)row.y + 4, 16, BLACK);

        if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            strncpy(outId, items[idx].id, (size_t)outIdSize - 1);
            outId[outIdSize - 1] = '\0';
            tb->len = (int)strlen(outId);
            *openFlag = false;
            tb->active = false;
        }
    }
}

static int HasChanged(const char *now, const char *orig)
{
    const char *a = now ? now : "";
    const char *b = orig ? orig : "";
    return strcmp(a, b) != 0;
}

static int ValidateForm(int requireFutureDate, int enforceSalesOnly)
{
    int isEdit = (gSelected >= 0);

    int requireFutureDate = 1;      // default untuk ADD
    int enforceSalesOnly  = 1;      // default untuk ADD

    if (isEdit) {
        requireFutureDate = HasChanged(gTanggal, gOrigTanggal) ? 1 : 0;
        enforceSalesOnly  = HasChanged(gKaryawanID, gOrigKaryawanID) ? 1 : 0;
    }

    if (!ValidateForm(requireFutureDate, enforceSalesOnly)) {
        gModal = MODAL_ERR_INPUT;
        return;
    }
}

static void TrimInPlace(char *s)
{
    if (!s) return;
    int n = (int)strlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r' || s[n-1] == '\n')) {
        s[n-1] = '\0';
        n--;
    }
    int i = 0;
    while (s[i] == ' ' || s[i] == '\t') i++;
    if (i > 0) memmove(s, s+i, strlen(s+i)+1);
}

static int StrEqI(const char *a, const char *b)
{
    if (!a || !b) return 0;
    while (*a && *b) {
        char ca = (char)tolower((unsigned char)*a);
        char cb = (char)tolower((unsigned char)*b);
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

static void DrawTestDriveTable(Rectangle area, bool allowInteraction)
{
    BuildView();

    DrawRectangleRec(area, RAYWHITE);
    DrawRectangleLines((int)area.x, (int)area.y, (int)area.width, (int)area.height, BLACK);

    float pad = 10.0f;
    float headerY = area.y + pad;
    float lineY = area.y + 38.0f;

    float colNo   = area.x + pad + 0;
    float colId   = area.x + pad + 50;
    float colMob  = area.x + pad + 170;
    float colKar  = area.x + pad + 290;
    float colPel  = area.x + pad + 430;
    float colTgl  = area.x + pad + 570;
    float colStat = area.x + pad + 700;

    DrawText("No",          (int)colNo,   (int)headerY, 18, BLACK);
    DrawText("TDID",        (int)colId,   (int)headerY, 18, BLACK);
    DrawText("MobilID",     (int)colMob,  (int)headerY, 18, BLACK);
    DrawText("SalesID",     (int)colKar,  (int)headerY, 18, BLACK);
    DrawText("CustomerID",  (int)colPel,  (int)headerY, 18, BLACK);
    DrawText("Date",        (int)colTgl,  (int)headerY, 18, BLACK);
    DrawText("Status",      (int)colStat, (int)headerY, 18, BLACK);

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
                CopyToForm(&gTd[i]);
            }
        }

        char noStr[16];
        snprintf(noStr, sizeof(noStr), "%d", vi + 1);

        DrawText(noStr,             (int)colNo,   (int)row.y + 4, 16, BLACK);
        DrawText(gTd[i].TestDriveID,(int)colId,   (int)row.y + 4, 16, BLACK);
        DrawText(gTd[i].MobilID,    (int)colMob,  (int)row.y + 4, 16, BLACK);
        DrawText(gTd[i].KaryawanID, (int)colKar,  (int)row.y + 4, 16, BLACK);
        DrawText(gTd[i].PelangganID,(int)colPel,  (int)row.y + 4, 16, BLACK);
        DrawText(gTd[i].Tanggal,    (int)colTgl,  (int)row.y + 4, 16, BLACK);
        DrawText(gTd[i].Status,     (int)colStat, (int)row.y + 4, 16, BLACK);
    }

    if (gViewCount == 0) {
        DrawText("No test drive data found.", (int)area.x + 12, (int)area.y + (int)area.height - 30, 16, GRAY);
    }
}

static void DrawStatusSelector(Rectangle bounds, int enabled)
{
    DrawRectangleLines((int)bounds.x, (int)bounds.y, (int)bounds.width, (int)bounds.height, WHITE);

    Rectangle left = { bounds.x, bounds.y, 40, bounds.height };
    Rectangle right = { bounds.x + bounds.width - 40, bounds.y, 40, bounds.height };
    Rectangle mid = { bounds.x + 40, bounds.y, bounds.width - 80, bounds.height };

    if (enabled)
    {
        if (UIButton(left, "<", 18)) {
            gStatusIndex--;
            if (gStatusIndex < 0) gStatusIndex = kStatusCount - 1;
        }
        if (UIButton(right, ">", 18)) {
            gStatusIndex++;
            if (gStatusIndex >= kStatusCount) gStatusIndex = 0;
        }
    }
    else
    {
        /* Disabled arrows (visual only) */
        DrawRectangleRec(left, (Color){220, 220, 220, 255});
        DrawRectangleLines((int)left.x, (int)left.y, (int)left.width, (int)left.height, WHITE);
        DrawText("<", (int)left.x + 14, (int)left.y + 6, 18, GRAY);

        DrawRectangleRec(right, (Color){220, 220, 220, 255});
        DrawRectangleLines((int)right.x, (int)right.y, (int)right.width, (int)right.height, WHITE);
        DrawText(">", (int)right.x + 14, (int)right.y + 6, 18, GRAY);
    }

    DrawRectangleRec(mid, (Color){240, 240, 240, 255});
    DrawRectangleLines((int)mid.x, (int)mid.y, (int)mid.width, (int)mid.height, WHITE);

    const char *txt = kStatuses[gStatusIndex];
    int fs = 18;
    int tw = MeasureText(txt, fs);
    int tx = (int)(mid.x + (mid.width - tw) / 2);
    int ty = (int)(mid.y + (mid.height - fs) / 2);
    DrawText(txt, tx, ty, fs, BLUE);
}

void SalesTestDrivePage(Rectangle contentArea, void *dbcPtr)
{
    void *dbc = NULL;
    if (dbcPtr) dbc = *(void**)dbcPtr;

    if (!gUiInited) {
        UITextBoxInit(&tbMobil,      (Rectangle){0,0,0,0}, gMobilID,      (int)sizeof(gMobilID),      false);
        UITextBoxInit(&tbKaryawan,   (Rectangle){0,0,0,0}, gKaryawanID,   (int)sizeof(gKaryawanID),   false);
        UITextBoxInit(&tbPelanggan,  (Rectangle){0,0,0,0}, gPelangganID,  (int)sizeof(gPelangganID),  false);
        UITextBoxInit(&tbTanggal,    (Rectangle){0,0,0,0}, gTanggal,      (int)sizeof(gTanggal),      false);
        UITextBoxInit(&tbTableSearch,(Rectangle){0,0,0,0}, gTableSearch,  (int)sizeof(gTableSearch),  false);
        gUiInited = 1;
    }

    if (gNeedReload) {
        ReloadTestDrives(dbcPtr);
        ReloadLookups(dbcPtr);
        gNeedReload = 0;
    }

    /* Header */
    DrawText("Test Drive", (int)contentArea.x + 40, (int)contentArea.y + 35, 28, RAYWHITE);

    /* Filter bar */
    Rectangle filterBar = { contentArea.x + 40, contentArea.y + 78, contentArea.width - 80, 52 };
    DrawRectangleRec(filterBar, (Color){30, 30, 30, 120});
    DrawRectangleLines((int)filterBar.x, (int)filterBar.y, (int)filterBar.width, (int)filterBar.height, WHITE);

    float fx = filterBar.x + 14;
    float fy = filterBar.y + 10;
    DrawText("Search", (int)fx, (int)fy + 6, 18, RAYWHITE);

    tbTableSearch.bounds = (Rectangle){ fx + 80, fy, 260, 34 };
    UITextBoxUpdate(&tbTableSearch);
    UITextBoxDraw(&tbTableSearch, 18);

    DrawText("Status", (int)(tbTableSearch.bounds.x + tbTableSearch.bounds.width + 30), (int)fy + 6, 18, RAYWHITE);

    Rectangle dd = { tbTableSearch.bounds.x + tbTableSearch.bounds.width + 105, fy, 220, 34 };

    bool comboConsumed = false;
    if (StatusFilterCombo(dd, 18, &gStatusFilterIndex, &gStatusFilterOpen, &comboConsumed, 0)) {
        gScroll = 0;
    }

    Rectangle tableArea = { contentArea.x + 40, filterBar.y + filterBar.height + 10, contentArea.width - 80, 280 };

    bool allowInteraction = (gModal == MODAL_NONE) && !comboConsumed && !gStatusFilterOpen;
    DrawTestDriveTable(tableArea, allowInteraction);

    /* Draw dropdown overlay after table so it isn't covered */
    StatusFilterCombo(dd, 18, &gStatusFilterIndex, &gStatusFilterOpen, NULL, 1);

/* Form area */
    Rectangle formArea = {
        contentArea.x + 40,
        tableArea.y + tableArea.height + 12,
        contentArea.width - 80,
        220
    };
    DrawRectangleRec(formArea, (Color){200, 200, 200, 255});
    DrawRectangleLines((int)formArea.x, (int)formArea.y, (int)formArea.width, (int)formArea.height, BLACK);

    float labelX = formArea.x + 20;
    float col1X = labelX + 160;
    float col2LabelX = formArea.x + (formArea.width / 2) + 10;
    float col2X = col2LabelX + 160;

    float rowY = formArea.y + 18;

    DrawText("Mobil ID", (int)labelX, (int)rowY + 8, 18, BLACK);
    tbMobil.bounds = (Rectangle){ col1X, rowY, 200, 34 };

    DrawText("Customer ID", (int)col2LabelX, (int)rowY + 8, 18, BLACK);
    tbPelanggan.bounds = (Rectangle){ col2X, rowY, 200, 34 };

    rowY += 52;
    DrawText("Sales ID", (int)labelX, (int)rowY + 8, 18, BLACK);
    tbKaryawan.bounds = (Rectangle){ col1X, rowY, 200, 34 };

    DrawText("Date (YYYY-MM-DD)", (int)col2LabelX, (int)rowY + 8, 18, BLACK);
    tbTanggal.bounds = (Rectangle){ col2X, rowY, 200, 34 };

    rowY += 52;
    DrawText("Status", (int)labelX, (int)rowY + 8, 18, BLACK);
    Rectangle statusBox = { col1X, rowY, 240, 34 };
    /* Add mode: status must be locked to "Dijadwalkan" */
    if (gSelected < 0) gStatusIndex = 0;
    DrawStatusSelector(statusBox, (gSelected >= 0) && allowInteraction);
    if (gSelected < 0)
        DrawText("Locked: Dijadwalkan", (int)(col1X + 255), (int)rowY + 10, 16, BLACK);
    else
        DrawText("Use < > to choose", (int)(col1X + 255), (int)rowY + 10, 16, BLACK);

    /* Textbox updates */
    if (allowInteraction) {
        UITextBoxUpdate(&tbMobil);
        UITextBoxUpdate(&tbPelanggan);
        UITextBoxUpdate(&tbKaryawan);
        UITextBoxUpdate(&tbTanggal);
    }

    UITextBoxDraw(&tbMobil, 18);
    UITextBoxDraw(&tbPelanggan, 18);
    UITextBoxDraw(&tbKaryawan, 18);
    UITextBoxDraw(&tbTanggal, 18);

    /* Picker dropdowns */
    if (allowInteraction) {
        DrawPicker(tbMobil.bounds, &tbMobil, gMobilID, (int)sizeof(gMobilID), gMobils, gMobilCount, &gPickMobilOpen, 360.0f);
        DrawPicker(tbPelanggan.bounds, &tbPelanggan, gPelangganID, (int)sizeof(gPelangganID), gCustomers, gCustomerCount, &gPickPelangganOpen, 360.0f);
        DrawPicker(tbKaryawan.bounds, &tbKaryawan, gKaryawanID, (int)sizeof(gKaryawanID), gSalesEmployees, gSalesCount, &gPickKaryawanOpen, 360.0f);
    }

    /* Dynamic buttons */
    float btnY = formArea.y + formArea.height + 18;

    Rectangle btnLeft1 = { formArea.x + 20, btnY, 130, 40 };
    Rectangle btnLeft2 = { formArea.x + 170, btnY, 130, 40 };

    if (gSelected < 0) {
        /* Add + Clear */
        if (UIButton(btnLeft1, "Clear", 18) && allowInteraction) {
            ClearForm();
        }
        if (UIButton((Rectangle){ formArea.x + formArea.width - 150, btnY, 130, 40 }, "+ Add", 18) && allowInteraction) {
            /* Insert: status is always locked to "Dijadwalkan" */
            gStatusIndex = 0;
            if (!dbc || !ValidateForm(1, 1)) {
                gModal = MODAL_ERR_INPUT;
            } else {
                if (DbTestDrive_Insert(dbc, gMobilID, gKaryawanID, gPelangganID, gTanggal, kStatuses[gStatusIndex])) {
                    SetToast("Data added successfully !");
                    ClearForm();
                    gNeedReload = 1;
                } else {
                    gModal = MODAL_ERR_INPUT;
                }
            }
        }
    } else {
        /* Edit + Delete */
        if (UIButton(btnLeft1, "Edit", 18) && allowInteraction) {
            int dateChanged = HasChanged(gTanggal, gOrigTanggal);
            int salesChanged = HasChanged(gKaryawanID, gOrigKaryawanID);
            int requireFutureDate = dateChanged ? 1 : 0;
            int enforceSalesOnly = salesChanged ? 1 : 0;
            if (!dbc || !ValidateForm(requireFutureDate, enforceSalesOnly)) {
                gModal = MODAL_ERR_INPUT;
            } else {
                if (DbTestDrive_Update(dbc,
                                      gTd[gSelected].TestDriveID,
                                      gMobilID,
                                      gKaryawanID,
                                      gPelangganID,
                                      gTanggal,
                                      kStatuses[gStatusIndex])) {
                    SetToast("Data edited successfully !");
                    /* Refresh originals so subsequent edits don't incorrectly re-trigger validations */
                    strncpy(gOrigKaryawanID, gKaryawanID, sizeof(gOrigKaryawanID) - 1);
                    gOrigKaryawanID[sizeof(gOrigKaryawanID) - 1] = '\0';
                    strncpy(gOrigTanggal, gTanggal, sizeof(gOrigTanggal) - 1);
                    gOrigTanggal[sizeof(gOrigTanggal) - 1] = '\0';
                    gNeedReload = 1;
                } else {
                    gModal = MODAL_ERR_INPUT;
                }
            }
        }

        if (UIButton(btnLeft2, "Delete", 18) && allowInteraction) {
            strncpy(gPendingDeleteId, gTd[gSelected].TestDriveID, sizeof(gPendingDeleteId) - 1);
            gPendingDeleteId[sizeof(gPendingDeleteId) - 1] = '\0';
            gModal = MODAL_CONFIRM_DELETE;
        }
    }

    /* ===== Modals ===== */
    if (gModal == MODAL_ERR_INPUT) {
        UIModalResult r = UIDrawModalOK("There is an incorrect input, please check again !", "OK", 18);
        if (r == UI_MODAL_OK) gModal = MODAL_NONE;
    } else if (gModal == MODAL_CONFIRM_DELETE) {
        UIModalResult r = UIDrawModalYesNo("Are you sure you want to delete?", "Yes", "No", 18);
        if (r == UI_MODAL_YES) {
            if (dbc && gPendingDeleteId[0]) {
                if (DbTestDrive_Delete(dbc, gPendingDeleteId)) {
                    SetToast("Data deleted successfully !");
                    gSelected = -1;
                    ClearForm();
                    gNeedReload = 1;
                }
            }
            gPendingDeleteId[0] = '\0';
            gModal = MODAL_NONE;
        } else if (r == UI_MODAL_NO) {
            gPendingDeleteId[0] = '\0';
            gModal = MODAL_NONE;
        }
    }

    DrawToast(contentArea);
}

/* Status filter combo with click-consume + overlay drawing (prevents being covered by table) */
static int StatusFilterCombo(Rectangle bounds, int fontSize,
                             int *selectedIndex, bool *open,
                             bool *consumedClick, int overlayOnly)
{
    Vector2 mouse = GetMousePosition();
    bool changed = false;

    float rowH = (float)(fontSize + 10);
    Rectangle listArea = (Rectangle){
        bounds.x,
        bounds.y + bounds.height + 4,
        bounds.width,
        (float)kStatusFilterCount * rowH
    };

    /* Input handling (only on first pass) */
    if (!overlayOnly)
    {
        bool hoverBase = CheckCollisionPointRec(mouse, bounds);

        if (hoverBase && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            *open = !(*open);
            if (consumedClick) *consumedClick = true;
        }

        if (*open)
        {
            /* Select item */
            for (int i = 0; i < kStatusFilterCount; i++)
            {
                Rectangle itemRect = { listArea.x + 1, listArea.y + (float)i * rowH, listArea.width - 2, rowH };
                bool hoverItem = CheckCollisionPointRec(mouse, itemRect);

                if (hoverItem && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                {
                    if (*selectedIndex != i)
                    {
                        *selectedIndex = i;
                        changed = true;
                    }
                    *open = false;
                    if (consumedClick) *consumedClick = true;
                }
            }

            /* Click outside closes */
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                bool inside = hoverBase || CheckCollisionPointRec(mouse, listArea);
                if (!inside) *open = false;
                else if (consumedClick) *consumedClick = true;
            }
        }

        /* Draw header */
        DrawRectangleRec(bounds, RAYWHITE);
        DrawRectangleLines((int)bounds.x, (int)bounds.y, (int)bounds.width, (int)bounds.height, BLACK);

        const char *label = "Select";
        if (*selectedIndex >= 0 && *selectedIndex < kStatusFilterCount) label = kStatusFilterItems[*selectedIndex];

        DrawText(label, (int)bounds.x + 10, (int)bounds.y + (int)((bounds.height - fontSize) / 2), fontSize, BLACK);
        DrawText(*open ? "^" : "v", (int)(bounds.x + bounds.width - 18), (int)bounds.y + 6, fontSize, BLACK);
    }

    /* Draw dropdown overlay (second pass) */
    if (overlayOnly && *open)
    {
        DrawRectangleRec(listArea, RAYWHITE);
        DrawRectangleLines((int)listArea.x, (int)listArea.y, (int)listArea.width, (int)listArea.height, BLACK);

        for (int i = 0; i < kStatusFilterCount; i++)
        {
            Rectangle itemRect = { listArea.x + 1, listArea.y + (float)i * rowH, listArea.width - 2, rowH };
            bool hoverItem = CheckCollisionPointRec(mouse, itemRect);

            if (hoverItem)
                DrawRectangleRec(itemRect, (Color){220, 230, 255, 255});

            DrawText(kStatusFilterItems[i], (int)itemRect.x + 10, (int)itemRect.y + 5, fontSize, BLACK);
        }
    }

    return changed ? 1 : 0;
}