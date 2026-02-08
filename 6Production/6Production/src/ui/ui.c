#include "ui.h"
#include "i18n.h"

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>


/* ===== UI FONT STATE ===== */
static Font gUIFont = (Font){0};
static float gUISpacing = 1.0f;
static bool gFontSet = false;

void UISetFont(Font font, float spacing)
{
    gUIFont = font;
    gUISpacing = spacing;
    gFontSet = true;
}

Font UIGetFont(void)
{
    return gFontSet ? gUIFont : GetFontDefault();
}

float UIGetFontSpacing(void)
{
    return gUISpacing;
}

int UIMeasureText(const char *text, int fontSize)
{
    const char *t = I18N_Tr(text);
    Vector2 sz = MeasureTextEx(UIGetFont(), t, (float)fontSize, UIGetFontSpacing());
    return (int)sz.x;
}

void UIDrawText(const char *text, int x, int y, int fontSize, Color color)
{
    const char *t = I18N_Tr(text);
    DrawTextEx(UIGetFont(), t, (Vector2){(float)x, (float)y}, (float)fontSize, UIGetFontSpacing(), color);
}

/* ===== BUTTON ===== */
bool UIButton(Rectangle Bounds, const char *text, int fontSize)
{
    Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, Bounds);

    Color bg = hover ? (Color){240, 240, 240, 255} : (Color){0, 0, 0, 0};
    Color border = WHITE;
    Color textColor = hover ? BLUE : WHITE;

    if (hover)
        DrawRectangleRec(Bounds, bg);
    else
        DrawRectangleLines((int)Bounds.x, (int)Bounds.y, (int)Bounds.width, (int)Bounds.height, border);

    int textW = MeasureText(text, fontSize);
    int textX = (int)(Bounds.x + (Bounds.width - textW) / 2);
    int textY = (int)(Bounds.y + (Bounds.height - fontSize) / 2);

    DrawText(text, textX, textY, fontSize, textColor);

    return hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

/* ===== MODALS ===== */
static Rectangle CenterRect(float w, float h)
{
    float sx = (float)GetScreenWidth();
    float sy = (float)GetScreenHeight();
    return (Rectangle){ (sx - w) * 0.5f, (sy - h) * 0.5f, w, h };
}

static void DrawModalFrame(Rectangle box)
{
    DrawRectangleRec((Rectangle){0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()}, Fade(BLACK, 0.55f));
    DrawRectangleRec(box, RAYWHITE);
    DrawRectangleLines((int)box.x, (int)box.y, (int)box.width, (int)box.height, BLACK);
}

UIModalResult UIDrawModalOK(const char *message, const char *okText, int fontSize)
{
    Rectangle box = CenterRect(560, 220);
    DrawModalFrame(box);

    int msgSize = fontSize;
    int msgW = MeasureText(message, msgSize);
    int msgX = (int)(box.x + (box.width - msgW) / 2);
    int msgY = (int)(box.y + 55);
    DrawText(message, msgX, msgY, msgSize, BLACK);

    Rectangle btn = { box.x + (box.width - 120) / 2, box.y + box.height - 70, 120, 40 };

    // Button style: use same outline look as other controls
    Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, btn);
    Color bg = hover ? (Color){240, 240, 240, 255} : (Color){0, 0, 0, 0};
    if (hover) DrawRectangleRec(btn, bg);
    DrawRectangleLines((int)btn.x, (int)btn.y, (int)btn.width, (int)btn.height, BLACK);

    const char *label = okText ? okText : "OK";
    int tw = MeasureText(label, fontSize);
    DrawText(label, (int)(btn.x + (btn.width - tw) / 2), (int)(btn.y + (btn.height - fontSize) / 2), fontSize, BLACK);

    if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        return UI_MODAL_OK;

    return UI_MODAL_NONE;
}

UIModalResult UIDrawModalYesNo(const char *message, const char *yesText, const char *noText, int fontSize)
{
    Rectangle box = CenterRect(600, 240);
    DrawModalFrame(box);

    int msgSize = fontSize;
    int msgW = MeasureText(message, msgSize);
    int msgX = (int)(box.x + (box.width - msgW) / 2);
    int msgY = (int)(box.y + 60);
    DrawText(message, msgX, msgY, msgSize, BLACK);

    Rectangle btnYes = { box.x + box.width * 0.25f - 60, box.y + box.height - 75, 120, 40 };
    Rectangle btnNo  = { box.x + box.width * 0.75f - 60, box.y + box.height - 75, 120, 40 };

    const char *yLabel = yesText ? yesText : "Yes";
    const char *nLabel = noText ? noText : "No";

    Vector2 mouse = GetMousePosition();

    bool hoverYes = CheckCollisionPointRec(mouse, btnYes);
    bool hoverNo  = CheckCollisionPointRec(mouse, btnNo);

    if (hoverYes) DrawRectangleRec(btnYes, (Color){240, 240, 240, 255});
    if (hoverNo)  DrawRectangleRec(btnNo,  (Color){240, 240, 240, 255});

    DrawRectangleLines((int)btnYes.x, (int)btnYes.y, (int)btnYes.width, (int)btnYes.height, BLACK);
    DrawRectangleLines((int)btnNo.x,  (int)btnNo.y,  (int)btnNo.width,  (int)btnNo.height,  BLACK);

    int twY = MeasureText(yLabel, fontSize);
    int twN = MeasureText(nLabel, fontSize);

    DrawText(yLabel, (int)(btnYes.x + (btnYes.width - twY) / 2), (int)(btnYes.y + (btnYes.height - fontSize) / 2), fontSize, BLACK);
    DrawText(nLabel, (int)(btnNo.x  + (btnNo.width  - twN) / 2), (int)(btnNo.y  + (btnNo.height  - fontSize) / 2), fontSize, BLACK);

    if (hoverYes && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        return UI_MODAL_YES;
    if (hoverNo && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        return UI_MODAL_NO;

    return UI_MODAL_NONE;
}

/* ===== COMBO BOX ===== */
bool UIComboBox(Rectangle bounds, const char **items, int itemCount,
                int *selectedIndex, bool *open, int fontSize)
{
    if (!items || itemCount <= 0 || !selectedIndex || !open)
        return false;

    Vector2 mouse = GetMousePosition();
    bool hoverBase = CheckCollisionPointRec(mouse, bounds);

    DrawRectangleRec(bounds, RAYWHITE);
    DrawRectangleLines((int)bounds.x, (int)bounds.y, (int)bounds.width, (int)bounds.height, BLACK);

    const char *label = "Select";
    if (*selectedIndex >= 0 && *selectedIndex < itemCount)
        label = items[*selectedIndex];

    DrawText(label, (int)bounds.x + 10, (int)bounds.y + (int)((bounds.height - fontSize) / 2), fontSize, BLACK);

    // small arrow
    DrawText(*open ? "^" : "v", (int)(bounds.x + bounds.width - 18), (int)bounds.y + 6, fontSize, BLACK);

    if (hoverBase && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        *open = !*open;

    bool changed = false;

    Rectangle listArea = (Rectangle){ bounds.x, bounds.y + bounds.height + 4, bounds.width, (float)itemCount * (float)(fontSize + 10) };

    if (*open)
    {
        DrawRectangleRec(listArea, RAYWHITE);
        DrawRectangleLines((int)listArea.x, (int)listArea.y, (int)listArea.width, (int)listArea.height, BLACK);

        for (int i = 0; i < itemCount; i++)
        {
            Rectangle itemRect = { listArea.x + 1, listArea.y + 1 + i * (fontSize + 10), listArea.width - 2, (float)(fontSize + 10) };
            bool hoverItem = CheckCollisionPointRec(mouse, itemRect);

            if (hoverItem)
                DrawRectangleRec(itemRect, (Color){220, 230, 255, 255});

            DrawText(items[i], (int)itemRect.x + 10, (int)itemRect.y + 5, fontSize, BLACK);

            if (hoverItem && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                if (*selectedIndex != i)
                {
                    *selectedIndex = i;
                    changed = true;
                }
                *open = false;
            }
        }

        // click outside closes
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            bool inside = hoverBase || CheckCollisionPointRec(mouse, listArea);
            if (!inside)
                *open = false;
        }
    }

    return changed;
}


/* =========================
   UI HELPERS
========================= */

int UIStringContainsI(const char *haystack, const char *needle)
{
    if (!needle || needle[0] == '\0') return 1;
    if (!haystack) return 0;

    size_t nl = strlen(needle);
    if (nl == 0) return 1;

    for (const char *h = haystack; *h; h++)
    {
        size_t i = 0;
        while (i < nl)
        {
            unsigned char hc = (unsigned char)h[i];
            unsigned char nc = (unsigned char)needle[i];
            if (hc == 0) break;
            if ((char)tolower(hc) != (char)tolower(nc)) break;
            i++;
        }
        if (i == nl) return 1;
    }
    return 0;
}

long long UIParseDigitsLL(const char *s)
{
    if (!s) return 0;
    long long v = 0;
    for (const char *p = s; *p; p++)
    {
        if (*p >= '0' && *p <= '9')
        {
            int d = *p - '0';
            if (v > 922337203685477580LL) { /* prevent overflow */
                v = 9223372036854775807LL;
                break;
            }
            v = v * 10 + d;
        }
    }
    return v;
}

void UIFormatRupiahLL(long long value, char *out, int outSize)
{
    if (!out || outSize <= 0)
        return;

    if (value < 0) value = -value;

    char digits[32];
    snprintf(digits, sizeof(digits), "%lld", value);

    int len = (int)strlen(digits);
    int first = len % 3;
    if (first == 0) first = 3;

    int w = 0;
    w += snprintf(out + w, (size_t)outSize - (size_t)w, "Rp ");

    for (int i = 0; i < len && w < outSize - 1; i++)
    {
        if (i != 0 && (i - first) >= 0 && ((i - first) % 3) == 0)
        {
            if (w < outSize - 1) out[w++] = '.';
        }
        if (w < outSize - 1) out[w++] = digits[i];
    }
    out[w] = '\0';
}

void UIFormatRupiahStr(const char *moneyStr, char *out, int outSize)
{
    UIFormatRupiahLL(UIParseDigitsLL(moneyStr), out, outSize);
}
