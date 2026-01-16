#include "sales_customer.h"
#include "ui.h"
#include "textbox.h"
#include "db_customers.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define MAX_CUSTOMERS 200

static Customer gCustomers[MAX_CUSTOMERS];
static int gCustomerCount = 0;
static int gSelectedRow = -1;

// scrolling
static int gScroll = 0;

/* table search */
static char gTableSearch[64] = "";
static UITextBox tbTableSearch;
static int gViewIdx[MAX_CUSTOMERS];
static int gViewCount = 0;

// form buffers (terikat ke UITextBox)
static char gNama[50] = "";
static char gNoKtp[50] = "";
static char gEmail[30] = "";
static char gNoHp[15] = "";
static char gAlamat[128] = "";

// textbox objects
static int gUiInited = 0;
static UITextBox tbNama, tbNoKtp, tbEmail, tbNoHp, tbAlamat;

// reload flag
static int gNeedReload = 1;

/* ===== Popup + Toast ===== */
typedef enum {
    MODAL_NONE = 0,
    MODAL_ERR_INPUT,
    MODAL_CONFIRM_DELETE
} ModalType;

static ModalType gModal = MODAL_NONE;
static char gPendingDeleteId[8] = ""; // PelangganID (P00001)

static char gToast[128] = "";
static float gToastTimer = 0.0f;

static const Color TOAST_GREEN = {0, 120, 0, 255};

static int ClampInt(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void SetToast(const char *msg)
{
    if (!msg) msg = "";
    strncpy(gToast, msg, sizeof(gToast) - 1);
    gToast[sizeof(gToast) - 1] = '\0';
    gToastTimer = 2.5f;
}

static void UpdateToast(void)
{
    if (gToastTimer > 0.0f) {
        gToastTimer -= GetFrameTime();
        if (gToastTimer <= 0.0f) {
            gToastTimer = 0.0f;
            gToast[0] = '\0';
        }
    }
}

static void ClearForm(void)
{
    gNama[0] = '\0';
    gNoKtp[0] = '\0';
    gEmail[0] = '\0';
    gNoHp[0] = '\0';
    gAlamat[0] = '\0';

    tbNama.len = 0;
    tbNoKtp.len = 0;
    tbEmail.len = 0;
    tbNoHp.len = 0;
    tbAlamat.len = 0;
}

static void CopyCustomerToForm(const Customer *c)
{
    strncpy(gNama, c->Nama, sizeof(gNama) - 1); gNama[sizeof(gNama) - 1] = '\0';
    strncpy(gNoKtp, c->NoKTP, sizeof(gNoKtp) - 1); gNoKtp[sizeof(gNoKtp) - 1] = '\0';
    strncpy(gEmail, c->Email, sizeof(gEmail) - 1); gEmail[sizeof(gEmail) - 1] = '\0';
    strncpy(gNoHp, c->NoHp, sizeof(gNoHp) - 1); gNoHp[sizeof(gNoHp) - 1] = '\0';
    strncpy(gAlamat, c->Alamat, sizeof(gAlamat) - 1); gAlamat[sizeof(gAlamat) - 1] = '\0';

    tbNama.len = (int)strlen(gNama);
    tbNoKtp.len = (int)strlen(gNoKtp);
    tbEmail.len = (int)strlen(gEmail);
    tbNoHp.len = (int)strlen(gNoHp);
    tbAlamat.len = (int)strlen(gAlamat);
}

static int IsDigitsOnly(const char *s)
{
    if (!s || *s == '\0') return 0;
    for (const char *p = s; *p; p++) {
        if (!isdigit((unsigned char)*p)) return 0;
    }
    return 1;
}

static int IsValidEmail(const char *s)
{
    if (!s || *s == '\0') return 0;
    const char *at = strchr(s, '@');
    if (!at || at == s) return 0;
    const char *dot = strchr(at + 1, '.');
    if (!dot || dot == at + 1) return 0;
    if (*(dot + 1) == '\0') return 0;
    return 1;
}

static bool ValidateCustomerForm(void)
{
    if (strlen(gNama) == 0) return false;
    if (strlen(gNoKtp) == 0) return false;
    if (strlen(gEmail) == 0) return false;
    if (strlen(gNoHp) == 0) return false;
    if (strlen(gAlamat) == 0) return false;

    // KTP: numeric (umumnya 16 digit)
    if (!IsDigitsOnly(gNoKtp)) return false;
    int ktpLen = (int)strlen(gNoKtp);
    if (ktpLen != 16) return false;

    // Phone: numeric, panjang masuk akal (10-15)
    if (!IsDigitsOnly(gNoHp)) return false;
    int hpLen = (int)strlen(gNoHp);
    if (hpLen < 10 || hpLen > 15) return false;

    if (!IsValidEmail(gEmail)) return false;

    return true;
}

// dbcPtr = &koneksiDB (alamat variabel handle), jadi harus dereference dulu
static void ReloadCustomers(void *dbcPtr)
{
    void *dbc = NULL;
    if (dbcPtr) dbc = *(void**)dbcPtr;

    int count = 0;
    if (DbCustomers_LoadAll(dbc, gCustomers, MAX_CUSTOMERS, &count)) {
        gCustomerCount = count;

        int visibleRowsGuess = 8;
        int maxScroll = gCustomerCount - visibleRowsGuess;
        if (maxScroll < 0) maxScroll = 0;

        gScroll = ClampInt(gScroll, 0, maxScroll);
        if (gSelectedRow >= gCustomerCount) gSelectedRow = -1;
    } else {
        gCustomerCount = 0;
        gScroll = 0;
        gSelectedRow = -1;
    }
}

static void BuildCustomersView(void)
{
    gViewCount = 0;
    for (int i = 0; i < gCustomerCount; i++)
    {
        bool ok = true;
        if (gTableSearch[0])
        {
            ok = UIStringContainsI(gCustomers[i].PelangganID, gTableSearch) ||
                 UIStringContainsI(gCustomers[i].Nama, gTableSearch) ||
                 UIStringContainsI(gCustomers[i].NoKTP, gTableSearch) ||
                 UIStringContainsI(gCustomers[i].Email, gTableSearch) ||
                 UIStringContainsI(gCustomers[i].NoHp, gTableSearch) ||
                 UIStringContainsI(gCustomers[i].Alamat, gTableSearch);
        }
        if (ok)
            gViewIdx[gViewCount++] = i;
    }

    if (gSelectedRow >= 0)
    {
        bool found = false;
        for (int v = 0; v < gViewCount; v++)
        {
            if (gViewIdx[v] == gSelectedRow)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            gSelectedRow = -1;
            ClearForm();
        }
    }

    // scroll clamp akan dihitung lagi di DrawCustomersTable() berdasarkan tinggi table
    // di sini cukup pastikan gScroll tidak negatif
    if (gScroll < 0) gScroll = 0;
}

static void DrawTextEllipsis(const char *text, int x, int y, int fontSize, int maxWidth, Color color)
{
    if (!text) text = "";
    if (maxWidth <= 0) return;

    if (MeasureText(text, fontSize) <= maxWidth) {
        DrawText(text, x, y, fontSize, color);
        return;
    }

    char buf[256];
    size_t n = strlen(text);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, text, n);
    buf[n] = '\0';

    while (n > 0)
    {
        if (n >= 2) {
            buf[n-2] = '.';
            buf[n-1] = '.';
            buf[n] = '\0';
        } else {
            buf[0] = '.';
            buf[1] = '\0';
        }

        if (MeasureText(buf, fontSize) <= maxWidth) break;
        n--;
        if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    }
    DrawText(buf, x, y, fontSize, color);
}

static void DrawCustomersTable(Rectangle area, bool allowInteraction)
{
    DrawRectangleRec(area, RAYWHITE);
    DrawRectangleLines((int)area.x, (int)area.y, (int)area.width, (int)area.height, BLACK);

    const float pad = 10.0f;
    const float headerY = area.y + pad;
    const float lineY = area.y + 38.0f;

    // kolom dibuat dinamis agar tidak "nabrak" di resolusi berbeda
    float x0 = area.x + pad;
    float w = area.width - pad * 2;

    float wNo = 40;
    float wId = 115;
    float wPhone = 120;
    float wRemain = w - (wNo + wId + wPhone);
    if (wRemain < 200) wRemain = 200; // fallback minimal

    float wName = wRemain * 0.30f;
    float wKtp  = wRemain * 0.28f;
    float wEmail = wRemain - wName - wKtp;

    float colNo    = x0;
    float colId    = colNo + wNo;
    float colName  = colId + wId;
    float colKTP   = colName + wName;
    float colEmail = colKTP + wKtp;
    float colPhone = colEmail + wEmail;

    DrawText("No", (int)colNo, (int)headerY, 18, BLACK);
    DrawText("Customer ID", (int)colId, (int)headerY, 18, BLACK);
    DrawText("Name", (int)colName, (int)headerY, 18, BLACK);
    DrawText("No. KTP", (int)colKTP, (int)headerY, 18, BLACK);
    DrawText("Email", (int)colEmail, (int)headerY, 18, BLACK);
    DrawText("Phone", (int)colPhone, (int)headerY, 18, BLACK);

    DrawLine((int)area.x, (int)lineY, (int)(area.x + area.width), (int)lineY, GRAY);

    const float rowH = 32.0f;
    int visibleRows = (int)((area.height - 52.0f) / rowH);
    if (visibleRows < 3) visibleRows = 3;

    int maxScroll = gViewCount - visibleRows;
    if (maxScroll < 0) maxScroll = 0;
    gScroll = ClampInt(gScroll, 0, maxScroll);

    if (allowInteraction && CheckCollisionPointRec(GetMousePosition(), area))
    {
        float wheel = GetMouseWheelMove();
        if (wheel > 0) gScroll--;
        if (wheel < 0) gScroll++;
        gScroll = ClampInt(gScroll, 0, maxScroll);
    }

    float rowY = lineY + 8;

    int start = gScroll;
    int end = start + visibleRows;
    if (end > gViewCount) end = gViewCount;

    for (int v = start; v < end; v++)
    {
        int i = gViewIdx[v];
        Rectangle row = { area.x + 4, rowY - 2, area.width - 8, rowH - 4 };

        bool hover = allowInteraction && CheckCollisionPointRec(GetMousePosition(), row);
        if (i == gSelectedRow)
            DrawRectangleRec(row, (Color){220, 240, 255, 255});
        else if (hover)
            DrawRectangleRec(row, (Color){245, 245, 245, 255});

        if (allowInteraction && hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            gSelectedRow = i;
            CopyCustomerToForm(&gCustomers[i]);
        }

        char noStr[8];
        snprintf(noStr, sizeof(noStr), "%d", v + 1);

        int y = (int)rowY + 2;
        DrawText(noStr, (int)colNo, y, 18, BLACK);
        DrawTextEllipsis(gCustomers[i].PelangganID, (int)colId, y, 18, (int)(wId - 8), BLACK);
        DrawTextEllipsis(gCustomers[i].Nama,        (int)colName, y, 18, (int)(wName - 8), BLACK);
        DrawTextEllipsis(gCustomers[i].NoKTP,       (int)colKTP, y, 18, (int)(wKtp - 8), BLACK);
        DrawTextEllipsis(gCustomers[i].Email,       (int)colEmail, y, 18, (int)(wEmail - 8), BLACK);
        DrawTextEllipsis(gCustomers[i].NoHp,        (int)colPhone, y, 18, (int)(wPhone - 8), BLACK);

        rowY += rowH;
    }

    if (gViewCount == 0)
        DrawText("No customers data found.", (int)(area.x + 20), (int)(lineY + 20), 18, DARKGRAY);
}


void SalesCustomersPage(Rectangle contentArea, void *dbcPtr)
{
    void *dbc = NULL;
    if (dbcPtr) dbc = *(void**)dbcPtr;

    if (!gUiInited) {
        UITextBoxInit(&tbNama,   (Rectangle){0,0,0,0}, gNama,   (int)sizeof(gNama),   false);
        UITextBoxInit(&tbNoKtp,  (Rectangle){0,0,0,0}, gNoKtp,  (int)sizeof(gNoKtp),  false);
        UITextBoxInit(&tbEmail,  (Rectangle){0,0,0,0}, gEmail,  (int)sizeof(gEmail),  false);
        UITextBoxInit(&tbNoHp,   (Rectangle){0,0,0,0}, gNoHp,   (int)sizeof(gNoHp),   false);
        UITextBoxInit(&tbAlamat, (Rectangle){0,0,0,0}, gAlamat, (int)sizeof(gAlamat), false);
        UITextBoxInit(&tbTableSearch, (Rectangle){0,0,0,0}, gTableSearch, (int)sizeof(gTableSearch), false);
        gUiInited = 1;
    }

    if (gNeedReload) {
        ReloadCustomers(dbcPtr);
        BuildCustomersView();
        gNeedReload = 0;
    }

    UpdateToast();

    bool blocked = (gModal != MODAL_NONE);

    DrawText("Customers Data", (int)contentArea.x + 40, (int)contentArea.y + 35, 28, RAYWHITE);

    Rectangle filterArea = { contentArea.x + 40, contentArea.y + 90, contentArea.width - 80, 44 };
    DrawRectangleRec(filterArea, (Color){200, 200, 200, 255});
    DrawText("Search", (int)filterArea.x + 12, (int)filterArea.y + 12, 18, BLACK);
    tbTableSearch.bounds = (Rectangle){ filterArea.x + 90, filterArea.y + 6, 360, 32 };
    if (!blocked) UITextBoxUpdate(&tbTableSearch);
    UITextBoxDraw(&tbTableSearch, 18);

    BuildCustomersView();

    // Layout adaptif: table mengisi ruang sampai form + tombol
    float bottomPad = 20;
    float btnH = 40;
    float gapBtn = 20;
    float formH = 240;

    float btnY = contentArea.y + contentArea.height - bottomPad - btnH;
    Rectangle formArea = { contentArea.x + 40, btnY - gapBtn - formH, contentArea.width - 80, formH };
    Rectangle tableArea = { contentArea.x + 40, filterArea.y + filterArea.height + 8,
                            contentArea.width - 80, formArea.y - 12 - (filterArea.y + filterArea.height + 8) };
    if (tableArea.height < 140) tableArea.height = 140;

    DrawCustomersTable(tableArea, !blocked);
    DrawRectangleRec(formArea, (Color){200, 200, 200, 255});

    // toast
    if (gToast[0] != '\0') {
        int tw = MeasureText(gToast, 18);
        DrawText(gToast, (int)(formArea.x + formArea.width - tw - 20), (int)(formArea.y + 6), 18, TOAST_GREEN);
    }

    float fx = formArea.x + 20;
    float fy = formArea.y + 20;

    DrawText("Name", (int)fx, (int)fy, 18, BLACK);
    tbNama.bounds = (Rectangle){ fx + 120, fy - 6, 330, 34 };

    fy += 45;
    DrawText("No KTP", (int)fx, (int)fy, 18, BLACK);
    tbNoKtp.bounds = (Rectangle){ fx + 120, fy - 6, 330, 34 };

    fy += 45;
    DrawText("Email", (int)fx, (int)fy, 18, BLACK);
    tbEmail.bounds = (Rectangle){ fx + 120, fy - 6, 330, 34 };

    fy += 45;
    DrawText("Phone", (int)fx, (int)fy, 18, BLACK);
    tbNoHp.bounds = (Rectangle){ fx + 120, fy - 6, 330, 34 };

    fy += 45;
    DrawText("Address", (int)fx, (int)fy, 18, BLACK);
    tbAlamat.bounds = (Rectangle){ fx + 120, fy - 6, 650, 34 };

    if (!blocked) {
        UITextBoxUpdate(&tbNama);
        UITextBoxUpdate(&tbNoKtp);
        UITextBoxUpdate(&tbEmail);
        UITextBoxUpdate(&tbNoHp);
        UITextBoxUpdate(&tbAlamat);
    }

    UITextBoxDraw(&tbNama, 18);
    UITextBoxDraw(&tbNoKtp, 18);
    UITextBoxDraw(&tbEmail, 18);
    UITextBoxDraw(&tbNoHp, 18);
    UITextBoxDraw(&tbAlamat, 18);

    Rectangle btnLeft1 = { formArea.x + 20,  btnY, 120, 40 };
    Rectangle btnLeft2 = { formArea.x + 160, btnY, 120, 40 };
    Rectangle btnRight = { formArea.x + formArea.width - 140, btnY, 120, 40 };

    if (!blocked) {
        if (gSelectedRow < 0) {
            // ADD MODE: only Add + Clear
            if (UIButton(btnLeft1, "Clear", 18)) {
                ClearForm();
            }

            if (UIButton(btnRight, "+ Add", 18)) {
                if (!ValidateCustomerForm()) {
                    gModal = MODAL_ERR_INPUT;
                } else if (dbc) {
                    if (DbCustomers_Insert(dbc, gNoKtp, gNama, gEmail, gNoHp, gAlamat)) {
                        ClearForm();
                        gNeedReload = 1;
                        SetToast("Data added successfully !");
                    }
                }
            }
        } else {
            // EDIT MODE: only Edit + Delete
            if (UIButton(btnLeft1, "Delete", 18)) {
                if (gSelectedRow >= 0 && gSelectedRow < gCustomerCount) {
                    strncpy(gPendingDeleteId, gCustomers[gSelectedRow].PelangganID, sizeof(gPendingDeleteId)-1);
                    gPendingDeleteId[sizeof(gPendingDeleteId)-1] = '\0';
                    gModal = MODAL_CONFIRM_DELETE;
                }
            }

            if (UIButton(btnLeft2, "Edit", 18)) {
                if (!ValidateCustomerForm()) {
                    gModal = MODAL_ERR_INPUT;
                } else if (dbc && gSelectedRow >= 0 && gSelectedRow < gCustomerCount) {
                    if (DbCustomers_Update(dbc, gCustomers[gSelectedRow].PelangganID, gNoKtp, gNama, gEmail, gNoHp, gAlamat)) {
                        gNeedReload = 1;
                        SetToast("Data edited successfully !");
                    }
                }
            }
        }
    }

    if (gCustomerCount == 0) {
        DrawText("No customer data found.",
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
                if (DbCustomers_Delete(dbc, gPendingDeleteId)) {
                    gSelectedRow = -1;
                    ClearForm();
                    gNeedReload = 1;
                }
            }
            gPendingDeleteId[0] = '\0';
            gModal = MODAL_NONE;
        }
    }
}
