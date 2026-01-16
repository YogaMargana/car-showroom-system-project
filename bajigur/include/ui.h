#ifndef UI_H
#define UI_H

#include <raylib.h>
#include <stdbool.h>

/* =========================
   UI FONT (GLOBAL)
   - Semua tabel + input akan ikut berubah asalkan file .c meng-include "ui.h"
   ========================= */

void UISetFont(Font font, float spacing);
Font UIGetFont(void);
float UIGetFontSpacing(void);

int UIMeasureText(const char *text, int fontSize);
void UIDrawText(const char *text, int x, int y, int fontSize, Color color);

/* Redirect agar semua pemanggilan DrawText/MeasureText otomatis pakai font UI */
#define DrawText(text, x, y, fontSize, color) UIDrawText((text), (x), (y), (fontSize), (color))
#define MeasureText(text, fontSize)           UIMeasureText((text), (fontSize))

/* =========================
   UI COMPONENTS
   ========================= */

bool UIButton(Rectangle bounds, const char *text, int fontSize);

/* Modal / popup result */
typedef enum {
    UI_MODAL_NONE = 0,
    UI_MODAL_OK,
    UI_MODAL_YES,
    UI_MODAL_NO
} UIModalResult;

/*
  Draw modal popups.
  - Call these functions ONLY when your page decides a modal is open.
  - Return value indicates which button was clicked in this frame.
*/
UIModalResult UIDrawModalOK(const char *message, const char *okText, int fontSize);
UIModalResult UIDrawModalYesNo(const char *message, const char *yesText, const char *noText, int fontSize);

/* Simple combo box (dropdown)
   - selectedIndex: -1 means none selected.
   - open: persistent open/close state.
   - returns true when selection changed.
*/
bool UIComboBox(Rectangle bounds, const char **items, int itemCount,
               int *selectedIndex, bool *open, int fontSize);

// Helpers
int UIStringContainsI(const char *haystack, const char *needle);
long long UIParseDigitsLL(const char *s);
void UIFormatRupiahLL(long long value, char *out, int outSize);
void UIFormatRupiahStr(const char *moneyStr, char *out, int outSize);

#endif
