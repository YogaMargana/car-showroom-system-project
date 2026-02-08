#include "xlsx_export.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

typedef struct {
    int row;
    int col;
    int isNumber;
    double num;
    char *str;
} Cell;

struct XlsxBook {
    char path[260];
    char sheetName[64];
    Cell *cells;
    int n;
    int cap;
    int maxRow;
    int maxCol;
};

// --- helpers ---
static char *StrDupSafe(const char *s){
    if(!s) s="";
    size_t n=strlen(s);
    char *p=(char*)malloc(n+1);
    if(!p) return NULL;
    memcpy(p,s,n+1);
    return p;
}

static void XmlEscapeAppend(char **dst, size_t *cap, size_t *len, const char *s){
    if(!s) s="";
    for(const unsigned char *p=(const unsigned char*)s; *p; p++){
        const char *rep=NULL;
        switch(*p){
            case '&': rep="&amp;"; break;
            case '<': rep="&lt;"; break;
            case '>': rep="&gt;"; break;
            case '"': rep="&quot;"; break;
            case '\'': rep="&apos;"; break;
            default: rep=NULL; break;
        }
        const char *add = rep ? rep : (const char[]){(char)*p,0};
        size_t addLen = strlen(add);
        if(*len + addLen + 1 > *cap){
            size_t newCap = (*cap==0 ? 1024 : (*cap*2));
            while(newCap < *len + addLen + 1) newCap*=2;
            char *nd = (char*)realloc(*dst, newCap);
            if(!nd) return;
            *dst = nd; *cap = newCap;
        }
        memcpy(*dst + *len, add, addLen);
        *len += addLen;
        (*dst)[*len] = 0;
    }
}

static void AppendFmt(char **dst, size_t *cap, size_t *len, const char *fmt, ...){
    va_list ap;
    va_start(ap, fmt);
    char tmp[2048];
    int n = vsnprintf(tmp, (int)sizeof(tmp), fmt, ap);
    va_end(ap);
    if(n < 0) return;
    if((size_t)n >= sizeof(tmp)) n = (int)sizeof(tmp)-1;
    if(*len + (size_t)n + 1 > *cap){
        size_t newCap = (*cap==0 ? 2048 : (*cap*2));
        while(newCap < *len + (size_t)n + 1) newCap*=2;
        char *nd = (char*)realloc(*dst, newCap);
        if(!nd) return;
        *dst = nd; *cap = newCap;
    }
    memcpy(*dst + *len, tmp, (size_t)n);
    *len += (size_t)n;
    (*dst)[*len] = 0;
}

static void ColToName(int col, char *out, size_t outSize){
    // 0->A, 25->Z, 26->AA
    char buf[16];
    int i=0;
    int c = col;
    do {
        int rem = c % 26;
        buf[i++] = (char)('A' + rem);
        c = c / 26 - 1;
    } while(c >= 0 && i < (int)sizeof(buf)-1);
    buf[i]=0;
    // reverse
    for(int j=0;j<i/2;j++){ char t=buf[j]; buf[j]=buf[i-1-j]; buf[i-1-j]=t; }
    strncpy(out, buf, outSize-1);
    out[outSize-1]=0;
}

static int CellCmp(const void *pa, const void *pb){
    const Cell *a=(const Cell*)pa;
    const Cell *b=(const Cell*)pb;
    if(a->row != b->row) return (a->row < b->row) ? -1 : 1;
    if(a->col != b->col) return (a->col < b->col) ? -1 : 1;
    return 0;
}

XlsxBook *Xlsx_Create(const char *xlsxPath, const char *sheetName){
    if(!xlsxPath || !xlsxPath[0]) return NULL;
    XlsxBook *b=(XlsxBook*)calloc(1, sizeof(XlsxBook));
    if(!b) return NULL;
    strncpy(b->path, xlsxPath, sizeof(b->path)-1);
    strncpy(b->sheetName, (sheetName && sheetName[0]) ? sheetName : "Sheet1", sizeof(b->sheetName)-1);
    b->maxRow = -1;
    b->maxCol = -1;
    return b;
}

static bool EnsureCellCap(XlsxBook *b){
    if(b->n + 1 <= b->cap) return true;
    int nc = (b->cap == 0) ? 128 : (b->cap * 2);
    Cell *p = (Cell*)realloc(b->cells, (size_t)nc * sizeof(Cell));
    if(!p) return false;
    b->cells = p;
    b->cap = nc;
    return true;
}

bool Xlsx_WriteString(XlsxBook *b, int row, int col, const char *utf8){
    if(!b || row < 0 || col < 0) return false;
    if(!EnsureCellCap(b)) return false;
    Cell *c=&b->cells[b->n++];
    memset(c,0,sizeof(*c));
    c->row=row; c->col=col; c->isNumber=0;
    c->str = StrDupSafe(utf8);
    if(!c->str) return false;
    if(row > b->maxRow) b->maxRow = row;
    if(col > b->maxCol) b->maxCol = col;
    return true;
}

bool Xlsx_WriteNumber(XlsxBook *b, int row, int col, double v){
    if(!b || row < 0 || col < 0) return false;
    if(!EnsureCellCap(b)) return false;
    Cell *c=&b->cells[b->n++];
    memset(c,0,sizeof(*c));
    c->row=row; c->col=col; c->isNumber=1;
    c->num=v;
    if(row > b->maxRow) b->maxRow = row;
    if(col > b->maxCol) b->maxCol = col;
    return true;
}

static char *BuildSheetXml(const XlsxBook *b, size_t *outSize){
    char *xml=NULL; size_t cap=0,len=0;
    AppendFmt(&xml,&cap,&len,"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>");
    AppendFmt(&xml,&cap,&len,"<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">");
    AppendFmt(&xml,&cap,&len,"<sheetData>");

    // copy & sort cells
    Cell *tmp = NULL;
    if(b->n > 0){
        tmp = (Cell*)malloc((size_t)b->n * sizeof(Cell));
        if(!tmp){ if(outSize) *outSize=0; return xml; }
        memcpy(tmp, b->cells, (size_t)b->n * sizeof(Cell));
        qsort(tmp, (size_t)b->n, sizeof(Cell), CellCmp);
    }

    int curRow = -1;
    for(int i=0;i<b->n;i++){
        const Cell *c = &tmp[i];
        if(c->row != curRow){
            if(curRow != -1) AppendFmt(&xml,&cap,&len,"</row>");
            curRow = c->row;
            AppendFmt(&xml,&cap,&len,"<row r=\"%d\">", curRow+1);
        }
        char colName[16];
        ColToName(c->col, colName, sizeof(colName));
        char ref[32];
        snprintf(ref, sizeof(ref), "%s%d", colName, c->row+1);

        if(c->isNumber){
            AppendFmt(&xml,&cap,&len,"<c r=\"%s\"><v>", ref);
            AppendFmt(&xml,&cap,&len,"%.15g", c->num);
            AppendFmt(&xml,&cap,&len,"</v></c>");
        } else {
            AppendFmt(&xml,&cap,&len,"<c r=\"%s\" t=\"inlineStr\"><is><t>", ref);
            XmlEscapeAppend(&xml,&cap,&len, c->str ? c->str : "");
            AppendFmt(&xml,&cap,&len,"</t></is></c>");
        }
    }
    if(curRow != -1) AppendFmt(&xml,&cap,&len,"</row>");

    if(tmp) free(tmp);

    AppendFmt(&xml,&cap,&len,"</sheetData></worksheet>");
    if(outSize) *outSize = len;
    return xml;
}

static void FreeCells(XlsxBook *b){
    if(!b) return;
    for(int i=0;i<b->n;i++) free(b->cells[i].str);
    free(b->cells);
    b->cells=NULL; b->n=0; b->cap=0;
}

bool Xlsx_Close(XlsxBook *b){
    if(!b) return false;

    // Required OpenXML parts (minimal, single sheet)
    const char *contentTypes =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
        "</Types>";

    const char *rels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
        "</Relationships>";

    char workbook[512];
    snprintf(workbook, sizeof(workbook),
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
        "<sheets><sheet name=\"%s\" sheetId=\"1\" r:id=\"rId1\"/></sheets>"
        "</workbook>",
        b->sheetName);

    const char *wbRels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
        "</Relationships>";

    size_t sheetSz = 0;
    char *sheetXml = BuildSheetXml(b, &sheetSz);
    if(!sheetXml) sheetSz = 0;

    bool ok = true;

    free(sheetXml);
    FreeCells(b);
    free(b);
    return ok;
}
