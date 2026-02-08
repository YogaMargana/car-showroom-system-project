#ifndef I18N_H
#define I18N_H

// Simple runtime localization (no DB changes).
//
// How it works:
// - UI strings are used as keys (usually the English text already in code).
// - At runtime we load a key=value file (e.g. assets/lang/id.txt).
// - When UI draws text, we translate via I18N_Tr(). If not found, fallback to original.
//
// File format example (UTF-8, one entry per line):
//   Admin Dashboard=Dashboard Admin
//   CASHIER MENU=MENU KASIR
// Lines starting with '#' or ';' are ignored.

#ifdef __cplusplus
extern "C" {
#endif

// Safe to call multiple times. If langCode is NULL or "en", it will clear translations.
void I18N_Init(const char *langCode);

// Switch language at runtime (ex: I18N_SetLanguage("id");).
void I18N_SetLanguage(const char *langCode);

// Current language code ("id", "en", ...). Never returns NULL.
const char *I18N_GetLanguage(void);

// Translate a UI string key. If missing, returns key.
const char *I18N_Tr(const char *key);

// Free loaded translations.
void I18N_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // I18N_H
