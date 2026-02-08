#pragma once

#include "raylib.h"
#include <stdbool.h>

typedef struct {
    bool open;
    int year;
    int month; // 1-12
} UIDatePicker;

// Initialize to a default month/year (fallback to today if year/month are 0).
void UIDatePickerInit(UIDatePicker *dp, int year, int month);

// Draw & handle a small date picker popup.
// - anchor: popup will appear below this rectangle.
// - outYmd: output buffer "YYYY-MM-DD" updated when user selects a day.
// Returns true if date changed.
bool UIDatePickerUpdateDraw(UIDatePicker *dp, Rectangle anchor, char *outYmd, int outYmdSize);
