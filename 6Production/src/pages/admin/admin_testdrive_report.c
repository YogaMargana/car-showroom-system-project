#include "admin_testdrive_report.h"

#include "ui.h"
#include "textbox.h"
#include "db_testdrive_report.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#define MAX_ROWS 2000
#define MAX_VIEW 2000

typedef enum {
    SORT_TANGGAL = 0,
    SORT_ID,
    SORT_MOBIL,
    SORT_CUSTOMER,
    SORT_SALES,
    SORT_STATUS
} SortKey;

static TestDriveRow gRows[MAX_ROWS];
static int gRowCount = 0;

static int gViewIdx[MAX_VIEW];
static int gViewCount = 0;

static int gNeedReload = 1;
static int gScroll = 0;

// pager bawah
static int gReportPage = 1;
static const int gReportPages = 2;

// input
static char gSearch[96] = "";
static char gFrom[16] = ""; // YYYY-MM-DD
static char gTo[16]   = "";

static UITextBox tbSearch;
static UITextBox tbFrom;
static UITextBox tbTo;
static int gUiInited = 0;

// sort
static SortKey gSortKey = SORT_TANGGAL;
static bool gSortAsc = false; // terbaru dulu

// toast
static char gToast[160] = "";
static float gToastTimer = 0.0f;

// ---------- helpers ----------
static int ClampInt(int v, int lo, int hi){ if(v<lo) return lo; if(v>hi) return hi; return v; }

static void SetToast(const char *msg){
    strncpy(gToast, msg ? msg : "", sizeof(gToast)-1);
    gToast[sizeof(gToast)-1] = '\0';
    gToastTimer = 2.5f;
}

static void ToLowerCopy(const char *src, char *dst, int cap){
    if(!dst||cap<=0) return;
    int j=0;
    for(int i=0; src && src[i] && j+1<cap; i++) dst[j++] = (char)tolower((unsigned char)src[i]);
    dst[j]='\0';
}

static bool ContainsInsensitive(const char *hay, const char *needle){
    if(!needle || needle[0]=='\0') return true;
    if(!hay) return false;
    char h[256], n[128];
    ToLowerCopy(hay,h,(int)sizeof(h));
    ToLowerCopy(needle,n,(int)sizeof(n));
    return strstr(h,n)!=NULL;
}

static int ParseDateKey(const char *ymd){
    if(!ymd || ymd[0]=='\0') return -1;
    int y=0,m=0,d=0;
    if(sscanf(ymd,"%d-%d-%d",&y,&m,&d)!=3) return -1;
    if(y<1900||y>2100||m<1||m>12||d<1||d>31) return -1;
    return y*10000+m*100+d;
}

static int CmpStr(const char *a, const char *b){
    if(!a) a=""; if(!b) b="";
    return strcmp(a,b);
}

// ---------- load / build view ----------
static void ReloadRows(void *dbcPtr){
    void *dbc = dbcPtr ? *(void**)dbcPtr : NULL;
    int count=0;
    if (DbTestDriveReport_LoadAll(dbc, gRows, MAX_ROWS, &count)) gRowCount = count;
    else gRowCount = 0;
    gScroll = 0;
}

static int CompareRow(const TestDriveRow *a, const TestDriveRow *b){
    int r=0;
    switch(gSortKey){
        case SORT_ID:       r = CmpStr(a->TestDriveID, b->TestDriveID); break;
        case SORT_MOBIL:    r = CmpStr(a->Mobil, b->Mobil); break;
        case SORT_CUSTOMER: r = CmpStr(a->Customer, b->Customer); break;
        case SORT_SALES:    r = CmpStr(a->Sales, b->Sales); break;
        case SORT_STATUS:   r = CmpStr(a->Status, b->Status); break;
        case SORT_TANGGAL:
        default:            r = CmpStr(a->Tanggal, b->Tanggal); break;
    }
    if(!gSortAsc) r = -r;
    if(r==0){
        r = CmpStr(a->TestDriveID, b->TestDriveID);
        if(!gSortAsc) r = -r;
    }
    return r;
}

static void SortView(void){
    for(int i=1;i<gViewCount;i++){
        int key = gViewIdx[i];
        int j=i-1;
        while(j>=0){
            const TestDriveRow *A=&gRows[gViewIdx[j]];
            const TestDriveRow *B=&gRows[key];
            if(CompareRow(A,B)<=0) break;
            gViewIdx[j+1]=gViewIdx[j];
            j--;
        }
        gViewIdx[j+1]=key;
    }
}

static void BuildView(void){
    int fromK = ParseDateKey(gFrom);
    int toK   = ParseDateKey(gTo);
    if(fromK!=-1 && toK!=-1 && fromK>toK){ int t=fromK; fromK=toK; toK=t; }

    gViewCount=0;
    for(int i=0;i<gRowCount && gViewCount<MAX_VIEW;i++){
        const TestDriveRow *r=&gRows[i];

        int dk=ParseDateKey(r->Tanggal);
        if(fromK!=-1 && dk!=-1 && dk<fromK) continue;
        if(toK!=-1   && dk!=-1 && dk>toK) continue;

        bool ok=true;
        if(gSearch[0]){
            ok = ContainsInsensitive(r->Tanggal,gSearch) ||
                 ContainsInsensitive(r->TestDriveID,gSearch) ||
                 ContainsInsensitive(r->Mobil,gSearch) ||
                 ContainsInsensitive(r->Customer,gSearch) ||
                 ContainsInsensitive(r->Sales,gSearch) ||
                 ContainsInsensitive(r->Status,gSearch);
        }
        if(!ok) continue;

        gViewIdx[gViewCount++] = i;
    }

    SortView();
    if(gScroll > gViewCount) gScroll=0;
}

// ---------- UI ----------
static bool HeaderSortCell(Rectangle r, const char *txt, SortKey key){
    bool hot = CheckCollisionPointRec(GetMousePosition(), r);
    Color bg = hot ? (Color){220,220,220,255} : (Color){235,235,235,255};
    DrawRectangleRec(r,bg);
    DrawRectangleLines((int)r.x,(int)r.y,(int)r.width,(int)r.height,BLACK);

    char label[64];
    if(gSortKey==key) snprintf(label,sizeof(label),"%s %s",txt, gSortAsc ? "^":"v");
    else snprintf(label,sizeof(label),"%s",txt);
    DrawText(label,(int)r.x+6,(int)r.y+6,18,BLACK);

    if(hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
        if(gSortKey==key) gSortAsc=!gSortAsc;
        else { gSortKey=key; gSortAsc=true; }
        return true;
    }
    return false;
}

static bool SolidButton(Rectangle r, const char *label, bool enabled){
    bool hot = CheckCollisionPointRec(GetMousePosition(), r);
    Color bg = enabled ? (hot ? (Color){225,225,225,255} : (Color){235,235,235,255})
                       : (Color){215,215,215,255};
    DrawRectangleRounded(r,0.2f,8,bg);
    DrawRectangleRoundedLines(r,0.2f,8,(Color){40,40,40,255});

    Color tc = enabled ? BLACK : (Color){120,120,120,255};
    int fontSize=18;
    int tw = MeasureText(label,fontSize);
    int tx=(int)(r.x+(r.width-tw)/2);
    int ty=(int)(r.y+(r.height-fontSize)/2);
    DrawText(label,tx,ty,fontSize,tc);

    if(!enabled) return false;
    return hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

static void DrawToast(void){
    if(gToastTimer<=0) return;
    float w=520,h=46;
    float x=(GetScreenWidth()-w)/2.0f, y=20;
    DrawRectangleRounded((Rectangle){x,y,w,h},0.2f,8,(Color){20,20,20,220});
    DrawText(gToast,(int)x+14,(int)y+14,18,RAYWHITE);
    gToastTimer -= GetFrameTime();
    if(gToastTimer<0) gToastTimer=0;
}

static void ExportCSV(const char *prefix){
    time_t t=time(NULL);
    struct tm *ti=localtime(&t);

    char fname[128];
    snprintf(fname,sizeof(fname),"%s_%04d%02d%02d_%02d%02d%02d.csv",
        prefix, ti->tm_year+1900, ti->tm_mon+1, ti->tm_mday,
        ti->tm_hour, ti->tm_min, ti->tm_sec);

    FILE *f=fopen(fname,"w");
    if(!f){ SetToast("Failed to create CSV file."); return; }

    fprintf(f,"Date,TestDriveID,Car,Customer,Sales,Status\n");
    for(int i=0;i<gViewCount;i++){
        const TestDriveRow *r=&gRows[gViewIdx[i]];
        // replace commas
        char car[128], cust[96], sales[96];
        strncpy(car,r->Mobil,sizeof(car)-1); car[sizeof(car)-1]=0;
        strncpy(cust,r->Customer,sizeof(cust)-1); cust[sizeof(cust)-1]=0;
        strncpy(sales,r->Sales,sizeof(sales)-1); sales[sizeof(sales)-1]=0;
        for(int j=0;car[j];j++) if(car[j]==',') car[j]=' ';
        for(int j=0;cust[j];j++) if(cust[j]==',') cust[j]=' ';
        for(int j=0;sales[j];j++) if(sales[j]==',') sales[j]=' ';

        fprintf(f,"%s,%s,%s,%s,%s,%s\n",
            r->Tanggal,r->TestDriveID,car,cust,sales,r->Status);
    }
    fclose(f);

    char msg[200];
    snprintf(msg,sizeof(msg),"CSV export successful: %s",fname);
    SetToast(msg);
}

// ---------- PAGE 1 ----------
static void DrawTD_Table(Rectangle area){
    DrawRectangleRec(area,RAYWHITE);
    DrawRectangleLines((int)area.x,(int)area.y,(int)area.width,(int)area.height,BLACK);

    float pad=8, x=area.x+pad, y=area.y+pad;
    float wT=120, wID=120, wCar=220, wCus=180, wSales=160, wSt=120;
    float hHead=32;

    Rectangle cT={x,y,wT,hHead}; x+=wT;
    Rectangle cI={x,y,wID,hHead}; x+=wID;
    Rectangle cC={x,y,wCar,hHead}; x+=wCar;
    Rectangle cCu={x,y,wCus,hHead}; x+=wCus;
    Rectangle cS={x,y,wSales,hHead}; x+=wSales;
    Rectangle cSt={x,y,wSt,hHead};

    bool changed=false;
    changed |= HeaderSortCell(cT,"Date",SORT_TANGGAL);
    changed |= HeaderSortCell(cI,"ID",SORT_ID);
    changed |= HeaderSortCell(cC,"Car",SORT_MOBIL);
    changed |= HeaderSortCell(cCu,"Customer",SORT_CUSTOMER);
    changed |= HeaderSortCell(cS,"Sales",SORT_SALES);
    changed |= HeaderSortCell(cSt,"Status",SORT_STATUS);
    if(changed){ BuildView(); gScroll=0; }

    int rowH=26;
    float bodyY=y+hHead+8;

    int visibleRows=(int)((area.y+area.height-bodyY-10)/rowH);
    if(visibleRows<1) visibleRows=1;

    int maxScroll=gViewCount-visibleRows;
    if(maxScroll<0) maxScroll=0;
    gScroll=ClampInt(gScroll,0,maxScroll);

    if(CheckCollisionPointRec(GetMousePosition(), area)){
        float wheel=GetMouseWheelMove();
        if(wheel!=0){
            gScroll -= (int)wheel;
            gScroll = ClampInt(gScroll,0,maxScroll);
        }
    }

    for(int i=0;i<visibleRows;i++){
        int idx=gScroll+i;
        if(idx>=gViewCount) break;
        const TestDriveRow *r=&gRows[gViewIdx[idx]];
        float rowY=bodyY+i*rowH;

        if(i%2==0) DrawRectangle((int)area.x+1,(int)rowY,(int)area.width-2,rowH,(Color){245,245,245,255});

        float cx=area.x+pad;
        DrawText(r->Tanggal,(int)cx+4,(int)rowY+4,18,BLACK); cx+=wT;
        DrawText(r->TestDriveID,(int)cx+4,(int)rowY+4,18,BLACK); cx+=wID;
        DrawText(r->Mobil,(int)cx+4,(int)rowY+4,18,BLACK); cx+=wCar;
        DrawText(r->Customer,(int)cx+4,(int)rowY+4,18,BLACK); cx+=wCus;
        DrawText(r->Sales,(int)cx+4,(int)rowY+4,18,BLACK); cx+=wSales;
        DrawText(r->Status,(int)cx+4,(int)rowY+4,18,BLACK);
    }
}

static void DrawTestDriveSummary(Rectangle area);

static void DrawTestDriveReport_Page1(Rectangle contentArea){
    float yOff=-70.0f;

    Rectangle sum = { contentArea.x + 40, 225 + yOff, contentArea.width - 80, 110 };
    DrawTestDriveSummary(sum);

    float gap =15.0f;

    Rectangle table = {
        contentArea.x + 40,
        sum.y + sum.height + gap,                           // <-- ini yang geser tabel ke bawah
        contentArea.width - 80,
        contentArea.height - (sum.y + sum.height + gap) - 220 // <-- biar tinggi tabel tetap pas
    };

    DrawTD_Table(table);
}

// =========================
// TEST DRIVE SUMMARY CARDS
// =========================

static bool TdStatusEqI(const char *a, const char *b)
{
    if (!a) a = "";
    if (!b) b = "";
    while (*a && *b) {
        char ca = (char)tolower((unsigned char)*a);
        char cb = (char)tolower((unsigned char)*b);
        if (ca != cb) return false;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

static int TdCountByStatus(const char *status)
{
    int c = 0;
    for (int i = 0; i < gViewCount; i++) {
        const TestDriveRow *r = &gRows[gViewIdx[i]];
        if (TdStatusEqI(r->Status, status)) c++;
    }
    return c;
}

static void DrawTestDriveSummary(Rectangle area)
{
    int totalFiltered = gViewCount;
    int scheduled = TdCountByStatus("Scheduled");
    int ongoing   = TdCountByStatus("Ongoing");
    int finished  = TdCountByStatus("Finished");

    float gap = 14.0f;
    float cardW = (area.width - 2 * gap) / 3.0f;
    float cardH = area.height;

    Rectangle c1 = { area.x, area.y, cardW, cardH };
    Rectangle c2 = { area.x + cardW + gap, area.y, cardW, cardH };
    Rectangle c3 = { area.x + (cardW + gap) * 2, area.y, cardW, cardH };

    Color cardBg = (Color){245,245,245,255};
    Color stroke = (Color){40,40,40,255};

    DrawRectangleRounded(c1, 0.18f, 10, cardBg);
    DrawRectangleRounded(c2, 0.18f, 10, cardBg);
    DrawRectangleRounded(c3, 0.18f, 10, cardBg);
    DrawRectangleRoundedLines(c1, 0.18f, 10, stroke);
    DrawRectangleRoundedLines(c2, 0.18f, 10, stroke);
    DrawRectangleRoundedLines(c3, 0.18f, 10, stroke);

    char buf[64];

    // Card 1: total filtered
    DrawText("Test Drive (Filtered)", (int)c1.x + 14, (int)c1.y + 12, 18, BLACK);
    snprintf(buf, sizeof(buf), "%d", totalFiltered);
    DrawText(buf, (int)c1.x + 14, (int)c1.y + 42, 34, BLACK);

    // Card 2: scheduled
    DrawText("Scheduled", (int)c2.x + 14, (int)c2.y + 12, 18, BLACK);
    snprintf(buf, sizeof(buf), "%d", scheduled);
    DrawText(buf, (int)c2.x + 14, (int)c2.y + 42, 34, BLACK);

    // Card 3: done + ongoing (subtext)
    DrawText("Done", (int)c3.x + 14, (int)c3.y + 12, 18, BLACK);
    snprintf(buf, sizeof(buf), "%d", finished);
    DrawText(buf, (int)c3.x + 14, (int)c3.y + 42, 34, BLACK);

    char sub[64];
    snprintf(sub, sizeof(sub), "Ongoing: %d", ongoing);
    DrawText(sub, (int)c3.x + 14, (int)c3.y + 82, 16, ((Color){60,60,60,255}));
}


// ---------- PAGE 2 (INSIGHTS) ----------
static void DrawCard(Rectangle r, const char *title, const char *value, const char *sub){
    DrawRectangleRounded(r,0.18f,10,(Color){245,245,245,255});
    DrawRectangleRoundedLines(r,0.18f,10,(Color){40,40,40,255});
    DrawText(title,(int)r.x+14,(int)r.y+12,18,BLACK);
    DrawText(value,(int)r.x+14,(int)r.y+42,34,BLACK);
    if(sub && sub[0]) DrawText(sub,(int)r.x+14,(int)r.y+82,16,((Color){60,60,60,255}));
}

static void DrawMiniHeader(Rectangle r, const char *title){
    DrawRectangleRounded(r,0.15f,8,(Color){245,245,245,255});
    DrawRectangleRoundedLines(r,0.15f,8,(Color){40,40,40,255});
    DrawText(title,(int)r.x+12,(int)r.y+10,20,BLACK);
}

static void DrawTestDriveReport_Page2(Rectangle contentArea, void *dbcPtr){
    float yOff=-120.0f;
    void *dbc = dbcPtr ? *(void**)dbcPtr : NULL;

    Rectangle area = { contentArea.x + 40, 225 + yOff, contentArea.width - 80, contentArea.height - 650 };
    DrawRectangleRec(area,RAYWHITE);
    DrawRectangleLines((int)area.x,(int)area.y,(int)area.width,(int)area.height,BLACK);

    // use filter yg sama (gFrom/gTo) buat insights juga
    TestDriveInsightSummary sum;
    TestDriveMostCarRow mostCars[8]; int nCars=0;
    TestDriveSalesPerfRow perf[10]; int nPerf=0;

    bool okSum = DbTestDriveInsight_LoadSummary(dbc, &sum, gFrom[0]?gFrom:NULL, gTo[0]?gTo:NULL);
    nCars = DbTestDriveInsight_LoadMostTestDrivenCars(dbc, mostCars, 8, gFrom[0]?gFrom:NULL, gTo[0]?gTo:NULL, 5);
    nPerf = DbTestDriveInsight_LoadSalesPerformance(dbc, perf, 10, gFrom[0]?gFrom:NULL, gTo[0]?gTo:NULL, 10);

    if(!okSum){
        memset(&sum,0,sizeof(sum));
        if(nCars<0) nCars=0;
        if(nPerf<0) nPerf=0;
    }

    float pad=16, x0=area.x+pad, y0=area.y+pad;
    DrawText("Insights",(int)x0,(int)y0,24,BLACK);
    y0 += 36;

    float gap=14;
    float cardW=(area.width - pad*2 - gap*2)/3.0f;
    float cardH=110;

    Rectangle c1={x0,y0,cardW,cardH};
    Rectangle c2={x0+cardW+gap,y0,cardW,cardH};
    Rectangle c3={x0+(cardW+gap)*2,y0,cardW,cardH};

    char v1[64], v2[64], v3[64], sub3[64];
    snprintf(v1,sizeof(v1),"%d",sum.totalFinished);
    snprintf(v2,sizeof(v2),"%d",sum.totalConverted);
    snprintf(v3,sizeof(v3),"%.2f%%",sum.conversionRatePct);
    snprintf(sub3,sizeof(sub3),"Scheduled:%d  Ongoing:%d",sum.totalScheduled,sum.totalOngoing);

    DrawCard(c1,"Finished Test Drives",v1,"");
    DrawCard(c2,"Converted (to Sales)",v2,"");
    DrawCard(c3,"Conversion Rate",v3,sub3);

    y0 += cardH + 18;

    // row 2: Most test driven car (left) + Sales performance (right)
    float boxH=260;
    float boxW=(area.width - pad*2 - gap)/2.0f;

    Rectangle bCar={x0,y0,boxW,boxH};
    Rectangle bPerf={x0+boxW+gap,y0,boxW,boxH};

    DrawMiniHeader(bCar,"Most Test Driven Car");
    DrawMiniHeader(bPerf,"Test Drive -> Sales Performance");

    // left content
    {
        float tx=bCar.x+12, ty=bCar.y+50;
        DrawText("Car",(int)tx,(int)ty,18,BLACK);
        DrawText("Count",(int)(tx+260),(int)ty,18,BLACK);
        DrawText("Finished",(int)(tx+330),(int)ty,18,BLACK);
        ty += 26;

        int show = (nCars>8)?8:nCars;
        for(int i=0;i<show;i++){
            char cA[16], cB[16];
            snprintf(cA,sizeof(cA),"%d",mostCars[i].totalTestDrive);
            snprintf(cB,sizeof(cB),"%d",mostCars[i].finishedCount);
            DrawText(mostCars[i].Mobil,(int)tx,(int)ty,18,((Color){40,40,40,255}));
            DrawText(cA,(int)(tx+260),(int)ty,18,((Color){40,40,40,255}));
            DrawText(cB,(int)(tx+330),(int)ty,18,((Color){40,40,40,255}));
            ty += 24;
        }
        if(nCars==0) DrawText("No data.",(int)tx,(int)ty,18,((Color){80,80,80,255}));
    }

    // right content
    {
        float tx=bPerf.x+12, ty=bPerf.y+50;
        DrawText("Sales",(int)tx,(int)ty,18,BLACK);
        DrawText("Converted",(int)(tx+210),(int)ty,18,BLACK);
        DrawText("Rate",(int)(tx+300),(int)ty,18,BLACK);
        DrawText("Revenue",(int)(tx+380),(int)ty,18,BLACK);
        ty += 26;

        int show = (nPerf>8)?8:nPerf;
        for(int i=0;i<show;i++){
            char cA[16], cB[16], cC[32];
            snprintf(cA,sizeof(cA),"%d",perf[i].convertedCount);
            snprintf(cB,sizeof(cB),"%.2f%%",perf[i].conversionRatePct);
            snprintf(cC,sizeof(cC),"%.0f",perf[i].revenueFromConverted);

            DrawText(perf[i].SalesName,(int)tx,(int)ty,18,((Color){40,40,40,255}));
            DrawText(cA,(int)(tx+210),(int)ty,18,((Color){40,40,40,255}));
            DrawText(cB,(int)(tx+300),(int)ty,18,((Color){40,40,40,255}));
            DrawText(cC,(int)(tx+380),(int)ty,18,((Color){40,40,40,255}));
            ty += 24;
        }
        if(nPerf==0) DrawText("No data.",(int)tx,(int)ty,18,((Color){80,80,80,255}));
    }
}

// pager bawah (sama persis kayak sales report)
static void DrawReportPager(Rectangle contentArea){
    float btnW=90, btnH=34;
    float py = contentArea.y + contentArea.height - btnH - 160;
    float px = contentArea.x + contentArea.width - (btnW*2 + 10 + 140) - 50;

    Rectangle bar={px-10,py-8,(btnW*2+10+140)+20,btnH+16};
    DrawRectangleRounded(bar,0.15f,8,(Color){245,245,245,230});
    DrawRectangleRoundedLines(bar,0.15f,8,(Color){40,40,40,255});

    bool canPrev=(gReportPage>1);
    bool canNext=(gReportPage<gReportPages);

    Rectangle rPrev={px,py,btnW,btnH};
    if(SolidButton(rPrev,"Prev",canPrev)){ gReportPage--; gScroll=0; }

    px += btnW+10;
    char pageText[64];
    snprintf(pageText,sizeof(pageText),"Page %d / %d",gReportPage,gReportPages);
    DrawText(pageText,(int)px,(int)py+8,18,BLACK);

    px += 140;
    Rectangle rNext={px,py,btnW,btnH};
    if(SolidButton(rNext,"Next",canNext)){ gReportPage++; gScroll=0; }
}

void AdminTestDriveReportPage(Rectangle contentArea, void *dbcPtr)
{
    float yOff = -70.0f;

    DrawText("Test Drive Reports", (int)contentArea.x + 40, (int)(100 + yOff), 28, WHITE);

    if (!gUiInited) {
        UITextBoxInit(&tbSearch, (Rectangle){ contentArea.x + 40,  (int)(170 + yOff), 380, 40 }, gSearch, (int)sizeof(gSearch), false);
        UITextBoxInit(&tbFrom,   (Rectangle){ contentArea.x + 440, (int)(170 + yOff), 140, 40 }, gFrom,   (int)sizeof(gFrom),   false);
        UITextBoxInit(&tbTo,     (Rectangle){ contentArea.x + 600, (int)(170 + yOff), 140, 40 }, gTo,     (int)sizeof(gTo),     false);
        gUiInited = 1;
    }

    static bool firstLoad = true;
    if (firstLoad) {
        ReloadRows(dbcPtr);
        BuildView();
        firstLoad = false;
    }

    if (gNeedReload) {
        ReloadRows(dbcPtr);
        gNeedReload = 0;
        BuildView();
    }

    // =========================
    // FILTER BAR: HANYA PAGE 1
    // =========================
    if (gReportPage == 1) {
        DrawText("Search", (int)contentArea.x + 40, (int)(145 + yOff), 18, WHITE);
        UITextBoxUpdate(&tbSearch);
        UITextBoxDraw(&tbSearch, 18);

        DrawText("From", (int)contentArea.x + 440, (int)(145 + yOff), 18, WHITE);
        UITextBoxUpdate(&tbFrom);
        UITextBoxDraw(&tbFrom, 18);

        DrawText("To", (int)contentArea.x + 600, (int)(145 + yOff), 18, WHITE);
        UITextBoxUpdate(&tbTo);
        UITextBoxDraw(&tbTo, 18);

        if (IsKeyPressed(KEY_ENTER)) { BuildView(); gScroll = 0; }

        if (UIButton((Rectangle){ contentArea.x + 760, (int)(170 + yOff), 110, 40 }, "Apply", 18)) {
            if (gFrom[0] && ParseDateKey(gFrom) == -1) SetToast("Wrong from format. Use YYYY-MM-DD.");
            else if (gTo[0] && ParseDateKey(gTo) == -1) SetToast("Wrong to format. Use YYYY-MM-DD.");
            else { BuildView(); gScroll = 0; }
        }

        if (UIButton((Rectangle){ contentArea.x + 880, (int)(170 + yOff), 110, 40 }, "Refresh", 18)) {
            gNeedReload = 1;
        }

        if (UIButton((Rectangle){ contentArea.x + 1000, (int)(170 + yOff), 140, 40 }, "Export CSV", 18)) {
            ExportCSV("testdrive_report");
        }
    }

    // --- page content ---
    if (gReportPage == 1) DrawTestDriveReport_Page1(contentArea);
    else DrawTestDriveReport_Page2(contentArea, dbcPtr);

    DrawReportPager(contentArea);
    DrawToast();
}