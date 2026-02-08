#ifndef TEXTBOX_H
#define TEXTBOX_H

#include <raylib.h>
#include <stdbool.h>

typedef struct {
    Rectangle bounds;
    char *buffer;
    int maxLen;
    int len;
    bool active;
    bool passwordMode;
    bool numericOnly;
} UITextBox;

void UITextBoxInit(UITextBox *tb, Rectangle bounds, char *buffer, int maxLen, bool passwordMode);
void UITextBoxUpdate(UITextBox *tb);
void UITextBoxDraw(UITextBox *tb, int fontSize);

#endif
