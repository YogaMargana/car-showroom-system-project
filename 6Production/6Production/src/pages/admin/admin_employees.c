#include "admin_employees.h"
#include "ui.h"
#include "textbox.h"
#include "db_employees.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define MAX_EMP 200

static Employee gEmp[MAX_EMP];
static int gEmpCount = 0;
static int gSelected = -1;
static int gScroll = 0;

/* table search */
static char gTableSearch[64] = "";
static UITextBox tbTableSearch;
static int gViewIdx[MAX_EMP];
static int gViewCount = 0;

static char gNama[50] = "";
static char gPosisi[20] = ""; // diisi dari dropdown
static char gNoHP[15] = "";
static char gEmail[50] = "";
static char gUsername[50] = "";
static char gPassword[255] = "";

static int gUiInited = 0;
static UITextBox tbNama, tbNoHP, tbEmail, tbUsername, tbPassword;

static int gNeedReload = 1;

/* ===== Role dropdown ===== */
static const char *kRoles[] = {"Kasir", "Admin", "Sales"};
static int gRoleIndex = -1;

/* ===== Popup + Toast ===== */
typedef enum
{
    MODAL_NONE = 0,
    MODAL_ERR_INPUT,
    MODAL_DUPLICATE,
    MODAL_CONFIRM_DELETE
} ModalType;

static ModalType gModal = MODAL_NONE;
static char gPendingDeleteId[8] = ""; // K00001

static char gToast[128] = "";
static float gToastTimer = 0.0f;

static const Color TOAST_GREEN = {0, 120, 0, 255};

static int StringEqualsI(const char *a, const char *b)
{
    if (!a || !b)
        return 0;
    while (*a && *b)
    {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        a++;
        b++;
    }
    return (*a == '\0' && *b == '\0');
}

static int IsDuplicateEmployee(const char *nohp, const char *email, const char *username, const char *excludeId)
{
    for (int i = 0; i < gEmpCount; i++)
    {
        if (excludeId && excludeId[0] && strcmp(gEmp[i].KaryawanID, excludeId) == 0)
            continue;

        if (strcmp(gEmp[i].NoHP, nohp) == 0)
            return 1;
        if (StringEqualsI(gEmp[i].Email, email))
            return 1;
        if (StringEqualsI(gEmp[i].Username, username))
            return 1;
    }
    return 0;
}

static int ClampInt(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static void SetToast(const char *msg)
{
    if (!msg)
    {
        gToast[0] = '\0';
        gToastTimer = 0.0f;
        return;
    }
    strncpy(gToast, msg, sizeof(gToast) - 1);
    gToast[sizeof(gToast) - 1] = '\0';
    gToastTimer = 2.5f;
}

static int IsDigitsOnly(const char *s)
{
    if (!s || *s == '\0')
        return 0;
    for (const char *p = s; *p; p++)
    {
        if (!isdigit((unsigned char)*p))
            return 0;
    }
    return 1;
}

static int IsValidEmail(const char *s)
{
    if (!s || *s == '\0')
        return 0;
    const char *at = strchr(s, '@');
    if (!at || at == s)
        return 0;
    const char *dot = strrchr(s, '.');
    if (!dot || dot < at + 2)
        return 0;
    if (*(dot + 1) == '\0')
        return 0;
    return 1;
}

static void SyncRoleIndexFromPosisi(void)
{
    gRoleIndex = -1;
    for (int i = 0; i < 3; i++)
    {
        if (strcmp(gPosisi, kRoles[i]) == 0)
        {
            gRoleIndex = i;
            break;
        }
    }
}

static void SyncPosisiFromRoleIndex(void)
{
    if (gRoleIndex >= 0 && gRoleIndex < 3)
    {
        strncpy(gPosisi, kRoles[gRoleIndex], sizeof(gPosisi) - 1);
        gPosisi[sizeof(gPosisi) - 1] = '\0';
    }
    else
    {
        gPosisi[0] = '\0';
    }
}

static bool ArrowButton(Rectangle r, const char *label, bool enabled)
{
    if (!enabled)
    {
        DrawRectangleLines((int)r.x, (int)r.y, (int)r.width, (int)r.height, GRAY);
        DrawText(label, (int)(r.x + r.width / 2 - MeasureText(label, 18) / 2), (int)(r.y + (r.height - 18) / 2), 18, GRAY);
        return false;
    }

    Vector2 m = GetMousePosition();
    bool hover = CheckCollisionPointRec(m, r);
    if (hover)
        DrawRectangleRec(r, (Color){230, 230, 230, 255});
    DrawRectangleLines((int)r.x, (int)r.y, (int)r.width, (int)r.height, BLACK);

    int tw = MeasureText(label, 18);
    int tx = (int)(r.x + (r.width - tw) / 2);
    int ty = (int)(r.y + (r.height - 18) / 2);
    DrawText(label, tx, ty, 18, BLACK);

    return hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

static void DrawRoleSelector(Rectangle bounds, bool enabled)
{
    DrawRectangleRec(bounds, RAYWHITE);
    DrawRectangleLines((int)bounds.x, (int)bounds.y, (int)bounds.width, (int)bounds.height, BLACK);

    float btnW = 34.0f;
    Rectangle left = {bounds.x + 2, bounds.y + 2, btnW, bounds.height - 4};
    Rectangle right = {bounds.x + bounds.width - btnW - 2, bounds.y + 2, btnW, bounds.height - 4};

    if (ArrowButton(left, "<", enabled))
    {
        if (gRoleIndex < 0)
            gRoleIndex = 0;
        else
            gRoleIndex = (gRoleIndex + 3 - 1) % 3;
        SyncPosisiFromRoleIndex();
    }
    if (ArrowButton(right, ">", enabled))
    {
        if (gRoleIndex < 0)
            gRoleIndex = 0;
        else
            gRoleIndex = (gRoleIndex + 1) % 3;
        SyncPosisiFromRoleIndex();
    }

    const char *label = (gRoleIndex >= 0 && gRoleIndex < 3) ? kRoles[gRoleIndex] : "-";
    DrawText(label, (int)(bounds.x + btnW + 12), (int)(bounds.y + 6), 18, BLACK);
}

static void ClearForm(void)
{
    gNama[0] = gPosisi[0] = gNoHP[0] = gEmail[0] = gUsername[0] = gPassword[0] = '\0';
    tbNama.len = tbNoHP.len = tbEmail.len = tbUsername.len = tbPassword.len = 0;
    gRoleIndex = -1;
}

static void CopyToForm(const Employee *e)
{
    strncpy(gNama, e->Nama, sizeof(gNama) - 1);
    gNama[sizeof(gNama) - 1] = '\0';
    strncpy(gPosisi, e->Posisi, sizeof(gPosisi) - 1);
    gPosisi[sizeof(gPosisi) - 1] = '\0';
    strncpy(gNoHP, e->NoHP, sizeof(gNoHP) - 1);
    gNoHP[sizeof(gNoHP) - 1] = '\0';
    strncpy(gEmail, e->Email, sizeof(gEmail) - 1);
    gEmail[sizeof(gEmail) - 1] = '\0';
    strncpy(gUsername, e->Username, sizeof(gUsername) - 1);
    gUsername[sizeof(gUsername) - 1] = '\0';
    strncpy(gPassword, e->Password, sizeof(gPassword) - 1);
    gPassword[sizeof(gPassword) - 1] = '\0';

    tbNama.len = (int)strlen(gNama);
    tbNoHP.len = (int)strlen(gNoHP);
    tbEmail.len = (int)strlen(gEmail);
    tbUsername.len = (int)strlen(gUsername);
    tbPassword.len = (int)strlen(gPassword);

    SyncRoleIndexFromPosisi();
}

static int ValidateEmployeeForm(void)
{
    if (!strlen(gNama))
        return 0;
    if (gRoleIndex < 0 || gRoleIndex >= 3)
        return 0;
    if (strlen(gNoHP) == 0)
        return false;
    if (!IsDigitsOnly(gNoHP))
        return false;
    int lenHp = (int)strlen(gNoHP);
    if (lenHp < 10 || lenHp > 15)
        return false;
    if (!strlen(gEmail) || !IsValidEmail(gEmail))
        return 0;
    if (!strlen(gUsername))
        return 0;
    if (!strlen(gPassword))
        return 0;

    return 1;
}

static void ReloadEmployees(void *dbcPtr)
{
    void *dbc = NULL;
    if (dbcPtr)
        dbc = *(void **)dbcPtr;

    int count = 0;
    if (DbEmployees_LoadAll(dbc, gEmp, MAX_EMP, &count))
    {
        gEmpCount = count;
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

static void BuildEmployeesView(void)
{
    gViewCount = 0;

    for (int i = 0; i < gEmpCount; i++)
    {
        int ok = 1;
        if (gTableSearch[0] != '\0')
        {
            ok = UIStringContainsI(gEmp[i].KaryawanID, gTableSearch) ||
                 UIStringContainsI(gEmp[i].Nama, gTableSearch) ||
                 UIStringContainsI(gEmp[i].Posisi, gTableSearch) ||
                 UIStringContainsI(gEmp[i].NoHP, gTableSearch) ||
                 UIStringContainsI(gEmp[i].Email, gTableSearch) ||
                 UIStringContainsI(gEmp[i].Username, gTableSearch);
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
            if (gViewIdx[v] == gSelected)
            {
                found = 1;
                break;
            }

        if (!found)
        {
            gSelected = -1;
            ClearForm();
        }
    }
}

static void DrawEmployeesTable(Rectangle area, bool allowInteraction)
{
    DrawRectangleRec(area, RAYWHITE);
    DrawRectangleLines((int)area.x, (int)area.y, (int)area.width, (int)area.height, BLACK);

    float pad = 10.0f;
    float headerY = area.y + pad;
    float lineY = area.y + 38.0f;

    float colNo = area.x + pad + 0;
    float colId = area.x + pad + 50;
    float colName = area.x + pad + 200;
    float colPos = area.x + pad + 380;
    float colPhone = area.x + pad + 490;
    float colEmail = area.x + pad + 620;
    float colUsn = area.x + pad + 800;

    DrawText("No", (int)colNo, (int)headerY, 18, BLACK);
    DrawText("Employee ID", (int)colId, (int)headerY, 18, BLACK);
    DrawText("Employee Name", (int)colName, (int)headerY, 18, BLACK);
    DrawText("Position", (int)colPos, (int)headerY, 18, BLACK);
    DrawText("Phone", (int)colPhone, (int)headerY, 18, BLACK);
    DrawText("Email", (int)colEmail, (int)headerY, 18, BLACK);
    DrawText("Username", (int)colUsn, (int)headerY, 18, BLACK);

    DrawLine((int)area.x, (int)lineY, (int)(area.x + area.width), (int)lineY, BLACK);

    int rowH = 28;
    int visibleRows = (int)((area.height - 50) / rowH);
    if (visibleRows < 1)
        visibleRows = 1;

    int maxScroll = gViewCount - visibleRows;
    if (maxScroll < 0)
        maxScroll = 0;

    gScroll = ClampInt(gScroll, 0, maxScroll);

    if (allowInteraction)
    {
        Vector2 mouse = GetMousePosition();
        if (CheckCollisionPointRec(mouse, area))
        {
            float mw = GetMouseWheelMove();
            if (mw != 0.0f && maxScroll > 0)
            {
                int step = (mw > 0.0f) ? (int)mw : (int)(-mw);
                if (step < 1)
                    step = 1;

                int dir = (mw > 0.0f) ? -1 : 1; // wheel up => scroll up
                gScroll = ClampInt(gScroll + dir * step, 0, maxScroll);
            }
        }
    }

    int start = gScroll;
    int end = start + visibleRows;
    if (end > gViewCount)
        end = gViewCount;

    Vector2 m = GetMousePosition();

    for (int v = start; v < end; v++)
    {
        int i = gViewIdx[v];
        float y = lineY + 10 + (v - start) * rowH;
        Rectangle row = {area.x + 2, y, area.width - 4, (float)rowH};

        bool hover = allowInteraction && CheckCollisionPointRec(m, row);
        bool selected = (i == gSelected);

        if (selected)
            DrawRectangleRec(row, (Color){200, 230, 255, 255});
        else if (hover)
            DrawRectangleRec(row, (Color){240, 240, 240, 255});

        if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
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
        snprintf(noStr, sizeof(noStr), "%d", v + 1);

        DrawText(noStr, (int)colNo, (int)row.y + 4, 16, BLACK);
        DrawText(gEmp[i].KaryawanID, (int)colId, (int)row.y + 4, 16, BLACK);
        DrawText(gEmp[i].Nama, (int)colName, (int)row.y + 4, 16, BLACK);
        DrawText(gEmp[i].Posisi, (int)colPos, (int)row.y + 4, 16, BLACK);
        DrawText(gEmp[i].NoHP, (int)colPhone, (int)row.y + 4, 16, BLACK);
        DrawText(gEmp[i].Email, (int)colEmail, (int)row.y + 4, 16, BLACK);
        DrawText(gEmp[i].Username, (int)colUsn, (int)row.y + 4, 16, BLACK);
    }

    if (gViewCount == 0)
    {
        DrawText("No employees data found.",
                 (int)area.x + 12,
                 (int)area.y + (int)area.height - 30,
                 16, GRAY);
    }
}

void AdminEmployeesPage(Rectangle contentArea, void *dbcPtr)
{
    void *dbc = NULL;
    if (dbcPtr)
        dbc = *(void **)dbcPtr;

    if (!gUiInited)
    {
        UITextBoxInit(&tbNama, (Rectangle){0, 0, 0, 0}, gNama, (int)sizeof(gNama), false);
        UITextBoxInit(&tbNoHP, (Rectangle){0, 0, 0, 0}, gNoHP, (int)sizeof(gNoHP), false);
        tbNoHP.numericOnly = true;   
        UITextBoxInit(&tbEmail, (Rectangle){0, 0, 0, 0}, gEmail, (int)sizeof(gEmail), false);
        UITextBoxInit(&tbUsername, (Rectangle){0, 0, 0, 0}, gUsername, (int)sizeof(gUsername), false);
        UITextBoxInit(&tbPassword, (Rectangle){0, 0, 0, 0}, gPassword, (int)sizeof(gPassword), true);
        UITextBoxInit(&tbTableSearch, (Rectangle){0, 0, 0, 0}, gTableSearch, (int)sizeof(gTableSearch), false);
        gUiInited = 1;
    }

    if (gNeedReload)
    {
        ReloadEmployees(dbcPtr);
        BuildEmployeesView();
        gNeedReload = 0;
    }

    if (gToastTimer > 0.0f)
    {
        gToastTimer -= GetFrameTime();
        if (gToastTimer <= 0.0f)
        {
            gToastTimer = 0.0f;
            gToast[0] = '\0';
        }
    }

    bool blocked = (gModal != MODAL_NONE);

    DrawText("Employee Data", (int)contentArea.x + 40, (int)contentArea.y + 35, 28, RAYWHITE);

    Rectangle filterArea = {contentArea.x + 40, contentArea.y + 90, contentArea.width - 80, 44};
    DrawRectangleRec(filterArea, (Color){200, 200, 200, 255});
    DrawText("Search", (int)filterArea.x + 12, (int)filterArea.y + 12, 18, BLACK);
    tbTableSearch.bounds = (Rectangle){filterArea.x + 90, filterArea.y + 6, 360, 32};
    if (!blocked)
        UITextBoxUpdate(&tbTableSearch);
    UITextBoxDraw(&tbTableSearch, 18);

    BuildEmployeesView();

    Rectangle tableArea = {contentArea.x + 40, filterArea.y + filterArea.height + 8, contentArea.width - 80, 280 - (44 + 8)};
    DrawEmployeesTable(tableArea, !blocked);

    // Form panel (abu-abu)
    Rectangle formArea = {
        contentArea.x + 40,
        tableArea.y + tableArea.height + 12,
        contentArea.width - 80,
        260};
    DrawRectangleRec(formArea, (Color){200, 200, 200, 255});

    // toast
    if (gToast[0] != '\0')
    {
        int tw = MeasureText(gToast, 18);
        DrawText(gToast, (int)(formArea.x + formArea.width - tw - 20), (int)(formArea.y + 6), 18, TOAST_GREEN);
    }

    // Grid layout
    float labelX = formArea.x + 20;
    float inputX = formArea.x + 160;
    float rowY = formArea.y + 18;

    float rowH = 40;
    float boxH = 32;
    float boxW1 = 360;
    float boxW2 = 220;

    // 1) Name
    DrawText("Name", (int)labelX, (int)rowY + 6, 18, BLACK);
    tbNama.bounds = (Rectangle){inputX, rowY, boxW1, boxH};

    // 2) Position (dropdown)
    rowY += rowH;
    DrawText("Position", (int)labelX, (int)rowY + 6, 18, BLACK);
    Rectangle roleBounds = (Rectangle){inputX, rowY, boxW2, boxH};

    // 3) Phone
    rowY += rowH;
    DrawText("Phone", (int)labelX, (int)rowY + 6, 18, BLACK);
    tbNoHP.bounds = (Rectangle){inputX, rowY, boxW2, boxH};

    // 4) Email
    rowY += rowH;
    DrawText("Email", (int)labelX, (int)rowY + 6, 18, BLACK);
    tbEmail.bounds = (Rectangle){inputX, rowY, boxW1, boxH};

    // 5) Username
    rowY += rowH;
    DrawText("Username", (int)labelX, (int)rowY + 6, 18, BLACK);
    tbUsername.bounds = (Rectangle){inputX, rowY, boxW1, boxH};

    // 6) Password
    rowY += rowH;
    DrawText("Password", (int)labelX, (int)rowY + 6, 18, BLACK);
    tbPassword.bounds = (Rectangle){inputX, rowY, boxW1, boxH};

    if (!blocked)
    {
        UITextBoxUpdate(&tbNama);
        UITextBoxUpdate(&tbNoHP);
        UITextBoxUpdate(&tbEmail);
        UITextBoxUpdate(&tbUsername);
        UITextBoxUpdate(&tbPassword);
    }

    UITextBoxDraw(&tbNama, 18);
    UITextBoxDraw(&tbNoHP, 18);
    UITextBoxDraw(&tbEmail, 18);
    UITextBoxDraw(&tbUsername, 18);
    UITextBoxDraw(&tbPassword, 18);

    // Position selector (left/right)
    DrawRoleSelector(roleBounds, !blocked);

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
            {
                ClearForm();
            }

            if (UIButton(btnRight, "+ Add", 18))
            {
                SyncPosisiFromRoleIndex();
                if (!ValidateEmployeeForm())
                {
                    gModal = MODAL_ERR_INPUT;
                }
                else if (dbc)
                {
                    if (IsDuplicateEmployee(gNoHP, gEmail, gUsername, NULL))
                    {
                        gModal = MODAL_DUPLICATE;
                    }
                    else if (DbEmployees_Insert(dbc, gNama, gPosisi, gNoHP, gEmail, gUsername, gPassword))
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
                    strncpy(gPendingDeleteId, gEmp[gSelected].KaryawanID, sizeof(gPendingDeleteId) - 1);
                    gPendingDeleteId[sizeof(gPendingDeleteId) - 1] = '\0';
                    gModal = MODAL_CONFIRM_DELETE;
                }
            }

            if (UIButton(btnLeft2, "Edit", 18))
            {
                SyncPosisiFromRoleIndex();
                if (!ValidateEmployeeForm())
                {
                    gModal = MODAL_ERR_INPUT;
                }
                else if (dbc && gSelected >= 0 && gSelected < gEmpCount)
                {
                    if (IsDuplicateEmployee(gNoHP, gEmail, gUsername, gEmp[gSelected].KaryawanID))
                    {
                        gModal = MODAL_DUPLICATE;
                    }
                    else if (DbEmployees_Update(dbc, gEmp[gSelected].KaryawanID, gNama, gPosisi, gNoHP, gEmail, gUsername, gPassword))
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
        DrawText("No employee data found.",
                 (int)tableArea.x + 12,
                 (int)tableArea.y + (int)tableArea.height - 30,
                 16, GRAY);
    }

    /* ===== MODAL DRAW (always last) ===== */
    if (gModal == MODAL_ERR_INPUT)
    {
        UIModalResult r = UIDrawModalOK("There is an incorrect input, please check again !", "OK", 18);
        if (r == UI_MODAL_OK)
            gModal = MODAL_NONE;
    }
    else if (gModal == MODAL_DUPLICATE)
    {
        UIModalResult r = UIDrawModalOK("Data must not be the same !", "OK", 18);
        if (r == UI_MODAL_OK)
            gModal = MODAL_NONE;
    }
    else if (gModal == MODAL_CONFIRM_DELETE)
    {
        UIModalResult r = UIDrawModalYesNo("Are you sure you want to delete?", "Ya", "Tidak", 18);
        if (r == UI_MODAL_NO)
        {
            gModal = MODAL_NONE;
        }
        else if (r == UI_MODAL_YES)
        {
            if (dbc && gPendingDeleteId[0] != '\0')
            {
                if (DbEmployees_Delete(dbc, gPendingDeleteId))
                {
                    gSelected = -1;
                    ClearForm();
                    gNeedReload = 1;
                    SetToast("Employee Deleted (Deactivated)");
                }
                else
                {
                    SetToast("Delete failed!");
                }
            }
            gPendingDeleteId[0] = '\0';
            gModal = MODAL_NONE;
        }
    }
}
