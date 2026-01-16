#ifndef ROLE_MENU_H
#define ROLE_MENU_H

#include "app_types.h"
#include <raylib.h>
#include <stdbool.h>

void DrawCashierLeftMenu(AppState *app, Rectangle leftArea);
void DrawSalesLeftMenu(AppState *app, Rectangle leftArea);
void DrawAdminLeftMenu(AppState *app, Rectangle leftArea);

// Mengecek apakah halaman sekarang termasuk grup role tertentu
bool IsCashierPage(Halaman h);
bool IsSalesPage(Halaman h);
bool IsAdminPage(Halaman h);

#endif
