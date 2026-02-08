#include "i18n.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    char *key;
    char *val;
} I18NPair;

static I18NPair *gPairs = NULL;
static int gCount = 0;
static int gCap = 0;

static char gLang[16] = "en";
static int gInited = 0;

static char *DupStr(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static void FreePairs(void)
{
    for (int i = 0; i < gCount; i++)
    {
        free(gPairs[i].key);
        free(gPairs[i].val);
    }
    free(gPairs);
    gPairs = NULL;
    gCount = 0;
    gCap = 0;
}

static void TrimInPlace(char *s)
{
    if (!s) return;

    // leading
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);

    // trailing
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1]))
    {
        s[len - 1] = '\0';
        len--;
    }
}

static void EnsureCap(int need)
{
    if (need <= gCap) return;
    int newCap = (gCap <= 0) ? 64 : gCap;
    while (newCap < need) newCap *= 2;

    I18NPair *p = (I18NPair *)realloc(gPairs, (size_t)newCap * sizeof(I18NPair));
    if (!p) return; // fail silently; app will fallback to original strings
    gPairs = p;
    gCap = newCap;
}

static int FindKey(const char *key)
{
    if (!key) return -1;
    for (int i = 0; i < gCount; i++)
    {
        if (gPairs[i].key && strcmp(gPairs[i].key, key) == 0)
            return i;
    }
    return -1;
}

static void AddOrReplace(const char *key, const char *val)
{
    if (!key || !val) return;

    int idx = FindKey(key);
    if (idx >= 0)
    {
        free(gPairs[idx].val);
        gPairs[idx].val = DupStr(val);
        return;
    }

    EnsureCap(gCount + 1);
    if (gCap <= gCount) return;

    gPairs[gCount].key = DupStr(key);
    gPairs[gCount].val = DupStr(val);
    gCount++;
}

static int LoadFile(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;

    char line[2048];
    while (fgets(line, (int)sizeof(line), fp))
    {
        // strip \r\n
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
        {
            line[n - 1] = '\0';
            n--;
        }

        char *s = line;
        while (*s && isspace((unsigned char)*s)) s++;
        if (*s == '\0') continue;
        if (*s == '#' || *s == ';') continue;

        // split at first '='
        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = '\0';
        char *k = s;
        char *v = eq + 1;
        TrimInPlace(k);
        TrimInPlace(v);
        if (k[0] == '\0') continue;

        AddOrReplace(k, v);
    }

    fclose(fp);
    return 1;
}

static void LoadLangFile(const char *langCode)
{
    if (!langCode || langCode[0] == '\0') return;

    char p1[256];
    char p2[256];

    // If the exe runs from project root
    snprintf(p1, sizeof(p1), "assets/lang/%s.txt", langCode);
    // If the exe runs from build/
    snprintf(p2, sizeof(p2), "../assets/lang/%s.txt", langCode);

    if (!LoadFile(p1))
        LoadFile(p2);
}

void I18N_Init(const char *langCode)
{
    // Default: Indonesian (requested). If user passes NULL, keep previous.
    if (!langCode || langCode[0] == '\0')
        langCode = "id";

    if (gInited && strcmp(gLang, langCode) == 0)
        return;

    // reset
    FreePairs();

    // set
    strncpy(gLang, langCode, sizeof(gLang) - 1);
    gLang[sizeof(gLang) - 1] = '\0';
    gInited = 1;

    // "en" means: no translation
    if (strcmp(gLang, "en") != 0)
        LoadLangFile(gLang);
}

void I18N_SetLanguage(const char *langCode)
{
    I18N_Init(langCode);
}

const char *I18N_GetLanguage(void)
{
    if (!gInited) I18N_Init("id");
    return gLang;
}

const char *I18N_Tr(const char *key)
{
    if (!key) return "";

    if (!gInited)
        I18N_Init("id");

    if (strcmp(gLang, "en") == 0 || gCount <= 0)
        return key;

    int idx = FindKey(key);
    if (idx >= 0 && gPairs[idx].val && gPairs[idx].val[0] != '\0')
        return gPairs[idx].val;

    return key;
}

void I18N_Shutdown(void)
{
    FreePairs();
    gInited = 0;
    strncpy(gLang, "en", sizeof(gLang) - 1);
    gLang[sizeof(gLang) - 1] = '\0';
}
