#include "textbox.h"
#include "ui.h"
#include <string.h>

void UITextBoxInit(UITextBox *tb, Rectangle bounds, char *buffer, int maxLen, bool passwordMode) {
    tb->bounds = bounds;
    tb->buffer = buffer;
    tb->maxLen = maxLen;
    tb->len = (int)strlen(buffer);
    tb->active = false;
    tb->passwordMode = passwordMode;
}

void UITextBoxUpdate(UITextBox *tb) {
    Vector2 m = GetMousePosition();

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        tb->active = CheckCollisionPointRec(m, tb->bounds);
    }

    if (!tb->active) return;

    int c = GetCharPressed();
    while (c > 0) {
        if (c >= 32 && c <= 126 && tb->len < tb->maxLen - 1) {
            tb->buffer[tb->len++] = (char)c;
            tb->buffer[tb->len] = '\0';
        }
        c = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE) && tb->len > 0) {
        tb->len--;
        tb->buffer[tb->len] = '\0';
    }
}

void UITextBoxDraw(UITextBox *tb, int fontSize) {
    Vector2 m = GetMousePosition();
    bool hover = CheckCollisionPointRec(m, tb->bounds);

    Color bg = tb->active ? (Color){255, 255, 255, 255}
              : hover ? (Color){245, 245, 245, 255}
                      : (Color){235, 235, 235, 255};

    // Always draw a visible box (fix: previously transparent + white border could be invisible on white panels)
    DrawRectangleRec(tb->bounds, bg);
    DrawRectangleLines((int)tb->bounds.x, (int)tb->bounds.y, (int)tb->bounds.width, (int)tb->bounds.height, BLACK);

    char display[256] = {0};
    if (tb->passwordMode) {
        int n = tb->len;
        if (n > 250) n = 250;
        for (int i = 0; i < n; i++) display[i] = '*';
        display[n] = '\0';
    } else {
        strncpy(display, tb->buffer, sizeof(display) - 1);
    }
 
    Color textColor = tb->active ? BLUE : BLACK;
    DrawText(display, (int)tb->bounds.x + 10, (int)tb->bounds.y + 10, fontSize, textColor);

    if (tb->active) {
        int textW = MeasureText(display, fontSize);
        int cx = (int)tb->bounds.x + 10 + textW + 2;
        int cy = (int)tb->bounds.y + 10;
        DrawLine(cx, cy, cx, cy + fontSize, textColor);
    }
}
