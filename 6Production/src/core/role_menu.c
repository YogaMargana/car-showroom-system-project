 
 #include "role_menu.h"
#include "ui.h"

bool IsCashierPage(Halaman h) {
    switch (h) {
        case HAL_DASHBOARD_CASHIER:
        case HAL_CASHIER_PENJUALAN_MOBIL:
        case HAL_CASHIER_PENJUALAN_AKSESORIS:
        case HAL_CASHIER_DATA_PELANGGAN:
            return true;
        default:
            return false;
    }
}

bool IsSalesPage(Halaman h) {
    switch (h) {
        case HAL_DASHBOARD_SALES:
        case HAL_SALES_TEST_DRIVE:
        case HAL_SALES_DATA_PELANGGAN:
            return true;
        default:
            return false;
    }
}

bool IsAdminPage(Halaman h) {
    switch (h) {
        case HAL_DASHBOARD_ADMIN:
        case HAL_ADMIN_M_MOBIL:
        case HAL_ADMIN_M_AKSESORIS:
        case HAL_ADMIN_M_KARYAWAN:
        case HAL_ADMIN_EMPLOYEE_DATA:
        case HAL_ADMIN_L_PENJUALAN:
        case HAL_ADMIN_L_TEST_DRIVE:
            return true;
        default:
            return false;
    }
}

// KASIR MENU
void DrawCashierLeftMenu(AppState *app, Rectangle leftArea) {
    float x = 40;
    float w = leftArea.width - 80;

    float y = 70;
    DrawText("CASHIER MENU", (int)x, (int)y, 22, WHITE);
    y += 40;

    float h = 50;
    float gap = 12;

    Rectangle b1 = { x, y, w, h }; y += h + gap;
    Rectangle b2 = { x, y, w, h }; y += h + gap;
    Rectangle b3 = { x, y, w, h }; y += h + gap;
    Rectangle b4 = { x, y, w, h }; y += h + gap;
    Rectangle b5 = { x, y, w, h }; y += h + gap;
    Rectangle backBtn = { x, y + 560, w, 45 };

    if (UIButton(b1, "Customers Data", 18))
        app->halamanSekarang = HAL_CASHIER_DATA_PELANGGAN;

    if (UIButton(b2, "Car Sales", 18))
        app->halamanSekarang = HAL_CASHIER_PENJUALAN_MOBIL;

    if (UIButton(b3, "Accessory Sales", 18))
        app->halamanSekarang = HAL_CASHIER_PENJUALAN_AKSESORIS;

    // Tombol Kembali
    if (UIButton(backBtn, "Log Out", 18)) {
        app ->roleAktif = ROLE_NONE;
        app->halamanSekarang = HAL_LOGIN; 
        return;
    }
    y += 65;

}

// SALES MENU
void DrawSalesLeftMenu(AppState *app, Rectangle leftArea) {
    float x = 40;
    float w = leftArea.width - 80;

    float y = 70;
    DrawText("SALES MENU", (int)x, (int)y, 22, WHITE);
    y += 40;

    float h = 50;
    float gap = 12;

    Rectangle b1 = { x, y, w, h }; y += h + gap;
    Rectangle b2 = { x, y, w, h }; y += h + gap;
    Rectangle backBtn = { x, y + 630, w, 45 };

    if (UIButton(b1, "Customers Data", 18))
        app->halamanSekarang = HAL_SALES_DATA_PELANGGAN;

    if (UIButton(b2, "Test Drive", 18))
        app->halamanSekarang = HAL_SALES_TEST_DRIVE;

    /* Revisi V2: Schedule & History dihapus. Gunakan filter Status di halaman Test Drive. */

    if (UIButton(backBtn, "Log Out", 18)) {
        app ->roleAktif = ROLE_NONE;
        app->halamanSekarang = HAL_LOGIN;
        return;
    }
    y += 65;
}

// ADMIN MENU
void DrawAdminLeftMenu(AppState *app, Rectangle leftArea) {
    float x = 40;
    float w = leftArea.width - 80;

    float y = 70;
    DrawText("ADMIN MENU", (int)x, (int)y, 22, WHITE);
    y += 40;

    float h = 48;
    float gap = 10;

    // MASTER
    DrawText("MASTER", (int)x, (int)y, 18, WHITE);
    y += 28;

    Rectangle m1 = { x, y, w, h }; y += h + gap;
    Rectangle m2 = { x, y, w, h }; y += h + gap;
    Rectangle m3 = { x, y, w, h }; y += h + gap;
    Rectangle m4 = { x, y, w, h }; y += h + gap;

    if (UIButton(m1, "Car Data", 18))      app->halamanSekarang = HAL_ADMIN_M_MOBIL;
    if (UIButton(m2, "Accessory Data", 18))  app->halamanSekarang = HAL_ADMIN_M_AKSESORIS;
    if (UIButton(m3, "Employee Data", 18))   app->halamanSekarang = HAL_ADMIN_M_KARYAWAN;

    // LAPORAN
    DrawText("Report", (int)x, (int)y, 18, WHITE);
    y += 28;

    Rectangle l1 = { x, y, w, h }; y += h + gap;
    Rectangle l2 = { x, y, w, h };

    if (UIButton(l1, "Sales Reports", 18))  app->halamanSekarang = HAL_ADMIN_L_PENJUALAN;
    if (UIButton(l2, "Test Drive Reports", 18)) app->halamanSekarang = HAL_ADMIN_L_TEST_DRIVE;

    // Tombol Back
    Rectangle backBtn = { x, y + 530, w, 45 };
    if (UIButton(backBtn, "Log Out", 18)) {
        app ->roleAktif = ROLE_NONE;
        app->halamanSekarang = HAL_LOGIN;
        return;
    }
    y += 70;
}