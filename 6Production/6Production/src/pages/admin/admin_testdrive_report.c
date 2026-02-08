#include "admin_testdrive_report.h"

#include "ui.h"
#include "textbox.h"
#include "db_testdrive_report.h"
#include "datepicker.h"
#include "xlsx_export.h"

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

// pager bawah -> 1 = transaksi, 2 = insight
static int gReportPage = 1;
static const int gReportPages = 2;

// input
static char gSearch[96] = "";
static char gFrom[16] = ""; // YYYY-MM-DD
static char gTo[16]   = "";

// previous filter snapshot (for auto-filter on typing)
static char gPrevSearch[96] = "";
static char gPrevFrom[16] = "";
static char gPrevTo[16] = "";

static UITextBox tbSearch;
static UITextBox tbFrom;
static UITextBox tbTo;
static UIDatePicker dpFrom;
static UIDatePicker dpTo;
static int gUiInited = 0;

// sort
static SortKey gSortKey = SORT_TANGGAL;
static bool gSortAsc = false; // default terbaru dulu

// toast
static char gToast[160] = "";
static float gToastTimer = 0.0f;

/* =========================
   helpers
========================= */
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

static bool StrEqI(const char *a, const char *b){
    // Case-insensitive compare, ignore leading/trailing whitespace
    if(!a) a="";
    if(!b) b="";

    // skip leading spaces
    while(*a && isspace((unsigned char)*a)) a++;
    while(*b && isspace((unsigned char)*b)) b++;

    // rtrim (compute effective lengths)
    size_t la = strlen(a);
    size_t lb = strlen(b);
    while(la>0 && isspace((unsigned char)a[la-1])) la--;
    while(lb>0 && isspace((unsigned char)b[lb-1])) lb--;
    if(la != lb) return false;

    for(size_t i=0;i<la;i++){
        if(tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
    }
    return true;
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

/* ===== status mapping (penting!) ===== */
static bool IsStatusScheduled(const char *s){
    return StrEqI(s,"Scheduled") || StrEqI(s,"Dijadwalkan");
}
static bool IsStatusCanceled(const char *s){
    return StrEqI(s,"Canceled") || StrEqI(s,"Cancelled") ||
           StrEqI(s,"Dibatalkan") || StrEqI(s,"Batal") || StrEqI(s,"Cancel");
}
static bool IsStatusFinished(const char *s){
    return StrEqI(s,"Finished") || StrEqI(s,"Done") || StrEqI(s,"Selesai");
}

// format uang: double -> long long (pembulatan) -> "Rp x.xxx"
static void FormatRupiahFromDouble(double v, char *out, int outSize)
{
    long long val = (long long)((v >= 0.0) ? (v + 0.5) : (v - 0.5));
    UIFormatRupiahLL(val, out, outSize);
}

/* =========================
   load / build view
========================= */
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

/* =========================
   UI widgets
========================= */
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

static void ExportXLSX(const char *prefix){
    time_t t=time(NULL);
    struct tm *ti=localtime(&t);

    char fname[128];
    snprintf(fname,sizeof(fname),"%s_%04d%02d%02d_%02d%02d%02d.xlsx",
        prefix, ti->tm_year+1900, ti->tm_mon+1, ti->tm_mday,
        ti->tm_hour, ti->tm_min, ti->tm_sec);

    XlsxBook *b = Xlsx_Create(fname, "Test Drive");
    if(!b){ SetToast("Failed to create XLSX file (check permission folder)."); return; }

    // header
    Xlsx_WriteString(b, 0, 0, "Date");
    Xlsx_WriteString(b, 0, 1, "TestDriveID");
    Xlsx_WriteString(b, 0, 2, "Car");
    Xlsx_WriteString(b, 0, 3, "Customer");
    Xlsx_WriteString(b, 0, 4, "Sales");
    Xlsx_WriteString(b, 0, 5, "Status");

    for(int i=0;i<gViewCount;i++){
        const TestDriveRow *r=&gRows[gViewIdx[i]];
        int row = i + 1;
        Xlsx_WriteString(b, row, 0, r->Tanggal);
        Xlsx_WriteString(b, row, 1, r->TestDriveID);
        Xlsx_WriteString(b, row, 2, r->Mobil);
        Xlsx_WriteString(b, row, 3, r->Customer);
        Xlsx_WriteString(b, row, 4, r->Sales);
        Xlsx_WriteString(b, row, 5, r->Status);
    }

    if(!Xlsx_Close(b)){
        SetToast("Failed to finalize XLSX file.");
        return;
    }

    char msg[200];
    snprintf(msg,sizeof(msg),"Eksport xlsx Berhasil !: %s",fname);
    SetToast(msg);
}

/* =========================
   PAGE 1
========================= */
static void DrawTestDriveSummary(Rectangle area){
    int totalFiltered = gViewCount;

    int scheduled=0, ongoing=0, finished=0;
    for(int i=0;i<gViewCount;i++){
        const TestDriveRow *r=&gRows[gViewIdx[i]];
        if(IsStatusScheduled(r->Status)) scheduled++;
        else if(IsStatusCanceled(r->Status)) ongoing++;
        else if(IsStatusFinished(r->Status)) finished++;
    }

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

    DrawText("Test Drive (Filtered)", (int)c1.x + 14, (int)c1.y + 12, 18, BLACK);
    snprintf(buf, sizeof(buf), "%d", totalFiltered);
    DrawText(buf, (int)c1.x + 14, (int)c1.y + 42, 34, BLACK);

    DrawText("Scheduled", (int)c2.x + 14, (int)c2.y + 12, 18, BLACK);
    snprintf(buf, sizeof(buf), "%d", scheduled);
    DrawText(buf, (int)c2.x + 14, (int)c2.y + 42, 34, BLACK);

    DrawText("Selesai", (int)c3.x + 14, (int)c3.y + 12, 18, BLACK);
    snprintf(buf, sizeof(buf), "%d", finished);
    DrawText(buf, (int)c3.x + 14, (int)c3.y + 42, 34, BLACK);

    char sub[64];
    snprintf(sub, sizeof(sub), "Dibatalkan: %d", ongoing);
    DrawText(sub, (int)c3.x + 14, (int)c3.y + 82, 16, ((Color){60,60,60,255}));
}

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
    changed |= HeaderSortCell(cT,"Tanggal",SORT_TANGGAL);
    changed |= HeaderSortCell(cI,"ID",SORT_ID);
    changed |= HeaderSortCell(cC,"Mobil",SORT_MOBIL);
    changed |= HeaderSortCell(cCu,"Pelanggan",SORT_CUSTOMER);
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

static void DrawTestDriveReport_Page1(Rectangle contentArea){
    float yOff=-70.0f;

    Rectangle sum = { contentArea.x + 40, 225 + yOff, contentArea.width - 80, 110 };
    DrawTestDriveSummary(sum);

    float gap=15.0f;
    Rectangle table = {
        contentArea.x + 40,
        sum.y + sum.height + gap,
        contentArea.width - 80,
        contentArea.height - (sum.y + sum.height + gap) - 220
    };
    DrawTD_Table(table);
}

/* =========================
   PAGE 2 (INSIGHTS) - dihitung dari data yg sudah loaded
   (biar pasti MUNCUL dulu)
========================= */
typedef struct {
    char name[96];
    int total;
    int finished;
} AggCar;

typedef struct {
    char name[96];
    int finished;
    int converted;
} AggSales;

static void DrawCardBox(Rectangle r, const char *title, const char *value, const char *sub){
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

static int FindOrAddCar(AggCar *arr, int *n, int cap, const char *name){
    for(int i=0;i<*n;i++) if(StrEqI(arr[i].name,name)) return i;
    if(*n>=cap) return -1;
    strncpy(arr[*n].name,name?name:"",sizeof(arr[*n].name)-1);
    arr[*n].name[sizeof(arr[*n].name)-1]=0;
    arr[*n].total=0; arr[*n].finished=0;
    (*n)++;
    return (*n)-1;
}

static int FindOrAddSales(AggSales *arr, int *n, int cap, const char *name){
    for(int i=0;i<*n;i++) if(StrEqI(arr[i].name,name)) return i;
    if(*n>=cap) return -1;
    strncpy(arr[*n].name,name?name:"",sizeof(arr[*n].name)-1);
    arr[*n].name[sizeof(arr[*n].name)-1]=0;
    arr[*n].finished=0; arr[*n].converted=0;
    (*n)++;
    return (*n)-1;
}

static void SortCarByTotalDesc(AggCar *a, int n){
    for(int i=1;i<n;i++){
        AggCar key=a[i]; int j=i-1;
        while(j>=0){
            if(a[j].total>key.total) break;
            if(a[j].total==key.total && a[j].finished>=key.finished) break;
            a[j+1]=a[j]; j--;
        }
        a[j+1]=key;
    }
}

static void SortSalesByConvertedDesc(AggSales *a, int n){
    for(int i=1;i<n;i++){
        AggSales key=a[i]; int j=i-1;
        while(j>=0){
            if(a[j].converted>key.converted) break;
            if(a[j].converted==key.converted && a[j].finished>=key.finished) break;
            a[j+1]=a[j]; j--;
        }
        a[j+1]=key;
    }
}

static void DrawTestDriveReport_Page2(Rectangle contentArea, void *dbcPtr)
{
    float yOff = -120.0f;
    void *dbc = dbcPtr ? *(void**)dbcPtr : NULL;

    Rectangle area = { contentArea.x + 40, 225 + yOff, contentArea.width - 80, contentArea.height - 350 };
    DrawRectangleRec(area, RAYWHITE);
    DrawRectangleLines((int)area.x, (int)area.y, (int)area.width, (int)area.height, BLACK);

    TestDriveInsightSummary sum;
    #define TOP_N_INSIGHT 5

    TestDriveMostCarRow mostCars[TOP_N_INSIGHT]; int nCars = 0;
    TestDriveSalesPerfRow perf[TOP_N_INSIGHT];   int nPerf = 0;

    const char *from = (gFrom[0] ? gFrom : NULL);
    const char *to   = (gTo[0]   ? gTo   : NULL);
    const char *srch = (gSearch[0] ? gSearch : NULL);

    bool okSum = DbTestDriveInsight_LoadSummary(dbc, &sum, from, to);
    nCars = DbTestDriveInsight_LoadMostTestDrivenCars(
    dbc, mostCars, TOP_N_INSIGHT, from, to, srch, TOP_N_INSIGHT
    );
    nPerf = DbTestDriveInsight_LoadSalesPerformance(
        dbc, perf, TOP_N_INSIGHT, from, to, TOP_N_INSIGHT
    );

    if (!okSum) memset(&sum, 0, sizeof(sum));
    if (nCars < 0) nCars = 0;
    if (nPerf < 0) nPerf = 0;

    float pad = 16, x0 = area.x + pad, y0 = area.y + pad;
    DrawText("Insight", (int)x0, (int)y0, 24, BLACK);
    y0 += 36;

    float gap = 14;
    float cardW = (area.width - pad * 2 - gap * 2) / 3.0f;
    float cardH = 110;

    Rectangle c1 = { x0, y0, cardW, cardH };
    Rectangle c2 = { x0 + cardW + gap, y0, cardW, cardH };
    Rectangle c3 = { x0 + (cardW + gap) * 2, y0, cardW, cardH };

    char v1[64], v2[64], v3[64], sub3[96];
    snprintf(v1, sizeof(v1), "%d", sum.totalFinished);
    snprintf(v2, sizeof(v2), "%d", sum.totalConverted);
    snprintf(v3, sizeof(v3), "%.2f%%", sum.conversionRatePct);

    DrawCardBox(c1, "Test Drive Selesai", v1, "");
    DrawCardBox(c2, "Konversi (ke Penjualan)", v2, "");
    DrawCardBox(c3, "Rasio Konversi", v3, sub3);

    y0 += cardH + 18;

    float boxH = 260;
    float boxW = (area.width - pad * 2 - gap) / 2.0f;

    Rectangle bCar  = { x0, y0, boxW, boxH };
    Rectangle bPerf = { x0 + boxW + gap, y0, boxW, boxH };

    DrawMiniHeader(bCar,  "Mobil Paling Sering Test Drive");
    DrawMiniHeader(bPerf, "Performa Test Drive -> Penjualan");

    {
        float tx = bCar.x + 12, ty = bCar.y + 50;
        DrawText("Mobil",    (int)tx,         (int)ty, 18, BLACK);
        DrawText("Jumlah",   (int)(tx + 260), (int)ty, 18, BLACK);
        DrawText("Selesai",  (int)(tx + 330), (int)ty, 18, BLACK);
        ty += 26;

        int show = (nCars > 5) ? 5 : nCars;
        for (int i = 0; i < show; i++) {
            char cA[16], cB[16];
            snprintf(cA, sizeof(cA), "%d", mostCars[i].totalTestDrive);
            snprintf(cB, sizeof(cB), "%d", mostCars[i].finishedCount);

            DrawText(mostCars[i].Mobil, (int)tx, (int)ty, 18, ((Color){40,40,40,255}));
            DrawText(cA, (int)(tx + 260), (int)ty, 18, ((Color){40,40,40,255}));
            DrawText(cB, (int)(tx + 330), (int)ty, 18, ((Color){40,40,40,255}));
            ty += 24;
        }
        if (nCars == 0) DrawText("Tidak ada data.", (int)tx, (int)ty, 18, ((Color){80,80,80,255}));
    }

    {
        float tx = bPerf.x + 12, ty = bPerf.y + 50;
        DrawText("Sales",      (int)tx,         (int)ty, 18, BLACK);
        DrawText("Konversi",   (int)(tx + 210), (int)ty, 18, BLACK);
        DrawText("Rasio",      (int)(tx + 300), (int)ty, 18, BLACK);
        DrawText("Pendapatan", (int)(tx + 380), (int)ty, 18, BLACK);
        ty += 26;

        int show = (nPerf > 8) ? 8 : nPerf;
        for (int i = 0; i < show; i++) {
            char cA[16], cB[16], cC[64];
            snprintf(cA, sizeof(cA), "%d", perf[i].convertedCount);
            snprintf(cB, sizeof(cB), "%.2f%%", perf[i].conversionRatePct);
            FormatRupiahFromDouble(perf[i].revenueFromConverted, cC, (int)sizeof(cC));

            const char *nm = perf[i].SalesName[0] ? perf[i].SalesName : perf[i].SalesID;

            DrawText(nm, (int)tx, (int)ty, 18, ((Color){40,40,40,255}));
            DrawText(cA, (int)(tx + 210), (int)ty, 18, ((Color){40,40,40,255}));
            DrawText(cB, (int)(tx + 300), (int)ty, 18, ((Color){40,40,40,255}));
            DrawText(cC, (int)(tx + 380), (int)ty, 18, ((Color){40,40,40,255}));
            ty += 24;
        }
        if (nPerf == 0) DrawText("Tidak ada data.", (int)tx, (int)ty, 18, ((Color){80,80,80,255}));
    }
}

/* =========================
   pager bawah (sama kaya sales report)
========================= */
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

/* =========================
   entry page
========================= */
void AdminTestDriveReportPage(Rectangle contentArea, void *dbcPtr)
{
    float yOff = -70.0f;

    DrawText("Test Drive Reports", (int)contentArea.x + 40, (int)(100 + yOff), 28, WHITE);

    if (!gUiInited) {
        UITextBoxInit(&tbSearch, (Rectangle){ contentArea.x + 40,  (int)(170 + yOff), 380, 40 }, gSearch, (int)sizeof(gSearch), false);
        // shrink a bit to make room for calendar button
        UITextBoxInit(&tbFrom,   (Rectangle){ contentArea.x + 440, (int)(170 + yOff), 120, 40 }, gFrom,   (int)sizeof(gFrom),   false);
        UITextBoxInit(&tbTo,     (Rectangle){ contentArea.x + 610, (int)(170 + yOff), 120, 40 }, gTo,     (int)sizeof(gTo),     false);
        UIDatePickerInit(&dpFrom, 0, 0);
        UIDatePickerInit(&dpTo, 0, 0);
        gUiInited = 1;
    }

    static bool firstLoad = true;
    if (firstLoad) {
        ReloadRows(dbcPtr);
        BuildView();
        // keep snapshot in sync
        strncpy(gPrevSearch, gSearch, sizeof(gPrevSearch) - 1);
        gPrevSearch[sizeof(gPrevSearch) - 1] = '\0';
        strncpy(gPrevFrom, gFrom, sizeof(gPrevFrom) - 1);
        gPrevFrom[sizeof(gPrevFrom) - 1] = '\0';
        strncpy(gPrevTo, gTo, sizeof(gPrevTo) - 1);
        gPrevTo[sizeof(gPrevTo) - 1] = '\0';
        firstLoad = false;
    }

    if (gNeedReload) {
        ReloadRows(dbcPtr);
        gNeedReload = 0;
        BuildView();
        // keep snapshot in sync
        strncpy(gPrevSearch, gSearch, sizeof(gPrevSearch) - 1);
        gPrevSearch[sizeof(gPrevSearch) - 1] = '\0';
        strncpy(gPrevFrom, gFrom, sizeof(gPrevFrom) - 1);
        gPrevFrom[sizeof(gPrevFrom) - 1] = '\0';
        strncpy(gPrevTo, gTo, sizeof(gPrevTo) - 1);
        gPrevTo[sizeof(gPrevTo) - 1] = '\0';
    }

    // FILTER BAR: HANYA PAGE 1 (sama kayak sales report)
    if (gReportPage == 1) {
        DrawText("Cari", (int)contentArea.x + 40, (int)(145 + yOff), 18, WHITE);
        UITextBoxUpdate(&tbSearch);
        UITextBoxDraw(&tbSearch, 18);

        DrawText("Dari", (int)contentArea.x + 440, (int)(145 + yOff), 18, WHITE);
        UITextBoxUpdate(&tbFrom);
        UITextBoxDraw(&tbFrom, 18);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), tbFrom.bounds)) dpFrom.open = true;

        DrawText("Sampai", (int)contentArea.x + 580, (int)(145 + yOff), 18, WHITE);
        UITextBoxUpdate(&tbTo);
        UITextBoxDraw(&tbTo, 18);

        if (strcmp(gPrevSearch, gSearch) != 0 || strcmp(gPrevFrom, gFrom) != 0 || strcmp(gPrevTo, gTo) != 0) {
            strncpy(gPrevSearch, gSearch, sizeof(gPrevSearch) - 1);
            gPrevSearch[sizeof(gPrevSearch) - 1] = '\0';
            strncpy(gPrevFrom, gFrom, sizeof(gPrevFrom) - 1);
            gPrevFrom[sizeof(gPrevFrom) - 1] = '\0';
            strncpy(gPrevTo, gTo, sizeof(gPrevTo) - 1);
            gPrevTo[sizeof(gPrevTo) - 1] = '\0';
            BuildView();
            gScroll = 0;
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), tbTo.bounds)) dpTo.open = true;

        if (UIButton((Rectangle){ contentArea.x + 790, (int)(170 + yOff), 90, 40 }, "Reset", 18)) {
            gSearch[0] = '\0';
            gFrom[0] = '\0';
            gTo[0] = '\0';

            dpFrom.open = 0;
            dpTo.open = 0;

            BuildView();
            gScroll = 0;
        }

        if (UIButton((Rectangle){ contentArea.x + 890, (int)(170 + yOff), 140, 40 }, "Ekspor XLSX", 18)) {
            ExportXLSX("testdrive_report");
        }
    }

    // page content
    if (gReportPage == 1) DrawTestDriveReport_Page1(contentArea);
    else DrawTestDriveReport_Page2(contentArea, dbcPtr);

    DrawReportPager(contentArea);

    // ===== calendar overlay (HARUS setelah semua konten biar tidak ketutupan) =====
    // From
    if (UIDatePickerUpdateDraw(&dpFrom, tbFrom.bounds, gFrom, (int)sizeof(gFrom))) {
        BuildView();
        gScroll = 0;
    }
    // To
    if (UIDatePickerUpdateDraw(&dpTo, tbTo.bounds, gTo, (int)sizeof(gTo))) {
        BuildView();
        gScroll = 0;
    }

    DrawToast();
}
