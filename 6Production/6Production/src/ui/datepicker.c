#include "datepicker.h"

// Use the shared UI font + runtime translations
#include "ui.h"

#include <stdio.h>
#include <time.h>

static int DaysInMonth(int y, int m){
    static const int d[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int dim = d[m-1];
    // leap year
    if(m==2){
        bool leap = ( (y%4==0 && y%100!=0) || (y%400==0) );
        if(leap) dim = 29;
    }
    return dim;
}

// 0=Sun..6=Sat
static int WeekdayOf(int y, int m, int d){
    struct tm t = {0};
    t.tm_year = y - 1900;
    t.tm_mon  = m - 1;
    t.tm_mday = d;
    mktime(&t);
    return t.tm_wday;
}

void UIDatePickerInit(UIDatePicker *dp, int year, int month){
    if(!dp) return;
    if(year <= 0 || month <= 0){
        time_t now = time(NULL);
        struct tm *ti = localtime(&now);
        dp->year = (ti ? ti->tm_year + 1900 : 2026);
        dp->month = (ti ? ti->tm_mon + 1 : 1);
    } else {
        dp->year = year;
        dp->month = month;
    }
    if(dp->month < 1) dp->month = 1;
    if(dp->month > 12) dp->month = 12;
    dp->open = false;
}

static bool Button(Rectangle r, const char *txt){
    bool hot = CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRounded(r, 0.15f, 8, hot ? (Color){230,230,230,255} : (Color){245,245,245,255});
    DrawRectangleRoundedLines(r, 0.15f, 8, (Color){40,40,40,255});
    int fs = 18;
    int tw = MeasureText(txt, fs);
    DrawText(txt, (int)(r.x + (r.width-tw)/2), (int)(r.y + (r.height-fs)/2), fs, BLACK);
    return hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

bool UIDatePickerUpdateDraw(UIDatePicker *dp, Rectangle anchor, char *outYmd, int outYmdSize){
    if(!dp || !outYmd || outYmdSize < 11) return false;
    bool changed = false;

    // Close if clicking outside (but allow click on anchor itself)
    if(dp->open && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
        Vector2 m = GetMousePosition();
        Rectangle popup = { anchor.x, anchor.y + anchor.height + 6, 280, 260 };
        bool inside = CheckCollisionPointRec(m, popup) || CheckCollisionPointRec(m, anchor);
        if(!inside) dp->open = false;
    }

    if(!dp->open) return false;

    Rectangle pop = { anchor.x, anchor.y + anchor.height + 6, 280, 260 };
    // keep in screen
    if(pop.x + pop.width > GetScreenWidth() - 10) pop.x = GetScreenWidth() - pop.width - 10;
    if(pop.y + pop.height > GetScreenHeight() - 10) pop.y = GetScreenHeight() - pop.height - 10;

    DrawRectangleRounded(pop, 0.12f, 10, (Color){255,255,255,255});
    DrawRectangleRoundedLines(pop, 0.12f, 10, (Color){40,40,40,255});

    // Header
    Rectangle left = { pop.x + 10, pop.y + 10, 34, 34 };
    Rectangle right = { pop.x + pop.width - 44, pop.y + 10, 34, 34 };
    if(Button(left, "<")){
        dp->month--; if(dp->month < 1){ dp->month = 12; dp->year--; }
    }
    if(Button(right, ">")){
        dp->month++; if(dp->month > 12){ dp->month = 1; dp->year++; }
    }
    char title[64];
    static const char *mNames[] = {"Januari","Februari","Maret","April","Mei","Juni","Juli","Agustus","September","Oktober","November","Desember"};
    snprintf(title, sizeof(title), "%s %d", mNames[dp->month-1], dp->year);
    int fs=18;
    int tw=MeasureText(title, fs);
    DrawText(title, (int)(pop.x + (pop.width-tw)/2), (int)(pop.y + 18), fs, BLACK);

    // Day names
    static const char *dn[] = {"Min","Sen","Sel","Rab","Kam","Jum","Sab"};
    float gx = pop.x + 10;
    float gy = pop.y + 56;
    float cellW = (pop.width - 20) / 7.0f;
    float cellH = 26;
    for(int i=0;i<7;i++){
        DrawText(dn[i], (int)(gx + i*cellW + 6), (int)gy, 16, (Color){60,60,60,255});
    }

    // Grid
    int firstWd = WeekdayOf(dp->year, dp->month, 1);
    int dim = DaysInMonth(dp->year, dp->month);
    int day=1;
    gy += 22;
    for(int r=0;r<6;r++){
        for(int c=0;c<7;c++){
            Rectangle cell = { gx + c*cellW, gy + r*cellH, cellW, cellH };
            int idx = r*7 + c;
            if(idx >= firstWd && day <= dim){
                bool hot = CheckCollisionPointRec(GetMousePosition(), cell);
                if(hot) DrawRectangleRec(cell, (Color){245,245,245,255});
                char ds[8]; snprintf(ds, sizeof(ds), "%d", day);
                DrawText(ds, (int)(cell.x + 6), (int)(cell.y + 4), 16, BLACK);
                if(hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
                    snprintf(outYmd, (size_t)outYmdSize, "%04d-%02d-%02d", dp->year, dp->month, day);
                    changed = true;
                    dp->open = false;
                }
                day++;
            }
        }
    }

    return changed;
}
