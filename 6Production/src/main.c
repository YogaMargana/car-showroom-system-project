#include "preheader.h"

int main(void) {
    // Layout
    InitWindow(800, 600, "Sistem Showroom Mobil");
    SetTargetFPS(60);

        int displayW = GetMonitorWidth(0);
        int displayH = GetMonitorHeight(0);
        SetWindowSize(displayW, displayH);
        ToggleFullscreen();
    
    AppState app = (AppState){0};
    app.halamanSekarang = HAL_LANDING;

    app.logo = LoadTexture("assets/logo.png");
    app.Landing = LoadTexture("assets/Landing.png");
    app.login = LoadTexture("assets/login.png");

    Font uiFont = (Font){0};

    const char *fontPathA = "assets/fonts/OpenSans-Regular.ttf";
    const char *fontPathB = "../assets/fonts/OpenSans-Regular.ttf"; // jaga-jaga kalau exe dijalankan dari build/
    const char *fontPath = NULL;

    if (FileExists(fontPathA)) fontPath = fontPathA;
    else if (FileExists(fontPathB)) fontPath = fontPathB;

    if (fontPath) {
    uiFont = LoadFontEx(fontPath, 64, 0, 0);
    if (uiFont.texture.id != 0) {
        SetTextureFilter(uiFont.texture, TEXTURE_FILTER_BILINEAR);
        UISetFont(uiFont, 1.0f);
    }
}

    LoginState lsLogin;
    InitLoginState(&lsLogin);

    SQLHDBC koneksiDB;
    connectKeDB(&koneksiDB);

    bool jalan = true;

    // Deklarasi variabel yang dipakai di setiap loop
    time_t raw;
    struct tm *ti;
    float dividerX;
    Rectangle leftArea;
    Rectangle rightArea;

    while (!WindowShouldClose() && jalan) {
        // UPDATE LEBAR/TINGGI LAYAR
        app.screenW = GetScreenWidth();
        app.screenH = GetScreenHeight();

        // UPDATE JAM
        raw = time(NULL);
        ti  = localtime(&raw);
        strftime(app.textJam, 10, "%H:%M", ti);

        BeginDrawing();
        ClearBackground(BLUE);

        // POSISI PEMBATAS & AREA
        dividerX = 710.0f;

        leftArea  = (Rectangle){ 0, 0, dividerX, (float)app.screenH };
        rightArea = (Rectangle){ dividerX, 0, (float)app.screenW - dividerX, (float)app.screenH };

        // 5) Gambar garis pembatas
        DrawLineEx(
            (Vector2){ dividerX, 0.0f },
            (Vector2){ dividerX, 1085 },
            5.0f,
            WHITE
        );

        // 6) Gambar jam di pojok kanan atas
        DrawText(app.textJam, app.screenW - 130, 30, 40, WHITE);

        // 7) MENU KIRI HANYA UNTUK HALAMAN DASHBOARD
        if (IsCashierPage(app.halamanSekarang)) {
            DrawCashierLeftMenu(&app, leftArea);
        }
        else if (IsSalesPage(app.halamanSekarang)) {
            DrawSalesLeftMenu(&app, leftArea);
        }
        else if (IsAdminPage(app.halamanSekarang)) {
            DrawAdminLeftMenu(&app, leftArea);
        }

        // 8) BAGIAN KANAN
        switch (app.halamanSekarang) {
            case HAL_LANDING:
                DrawTexture(app.Landing, 0, 0, WHITE);
                DrawTexture(app.logo, 108, 0, WHITE);
                DrawLandingPage(&app);
                DrawText(app.textJam, app.screenW - 130, 30, 40, WHITE);
                break;

            case HAL_LOGIN:
                UpdateLoginPage(&lsLogin);
                DrawLoginPage(&app, &lsLogin, &koneksiDB, "LOGIN");
                DrawTexture(app.logo, 108, 0, WHITE);
                DrawTexture(app.login, 711, 0, WHITE);
                DrawText(app.textJam, app.screenW - 130, 30, 40, WHITE);
                break;

            case HAL_DASHBOARD_ADMIN:
                DrawText("Dashboard Admin", (int)rightArea.x + 40, 120, 28, WHITE);
                DrawText("Pilih menu di kiri untuk mengelola master data / laporan.",
                (int)rightArea.x + 40, 160, 18, WHITE);
                break;

            case HAL_ADMIN_M_KARYAWAN:
                AdminEmployeesPage(rightArea, &koneksiDB);
                break;

            case HAL_ADMIN_M_MOBIL:
                AdminCarsDataPage(rightArea, &koneksiDB);
                break;

            case HAL_ADMIN_M_AKSESORIS:
                AdminAccessorissPage(rightArea, &koneksiDB);
                break;

            case HAL_CASHIER_INPUT_PENJUALAN_MOBIL:
                AdminInputPenjualanMobilPage(&app, leftArea, rightArea, &koneksiDB);
                break;

            case HAL_CASHIER_PENJUALAN_MOBIL:
                AdminPenjualannobilPage(&app, rightArea, &koneksiDB);
                break;

            case HAL_ADMIN_L_TEST_DRIVE:
                AdminTestDriveReportPage(rightArea, &koneksiDB);
                break;

            case HAL_ADMIN_L_PENJUALAN:
                AdminSalesReportPage(rightArea, &koneksiDB);
                break;

            case HAL_DASHBOARD_CASHIER:
                DrawText("Dashboard Kasir", (int)rightArea.x + 40, 120, 28, WHITE);
                break;

            case HAL_SALES_TEST_DRIVE:
                SalesTestDrivePage(rightArea, &koneksiDB);
                break;

            case HAL_CASHIER_DATA_PELANGGAN:
                 CashierCustomersPage(rightArea, &koneksiDB);
                break;

            case HAL_DASHBOARD_SALES:
                DrawText("Dashboard Sales", (int)rightArea.x + 40, 120, 28, WHITE);
                DrawText("Nanti isi fitur sales di panel kanan.",
                         (int)rightArea.x + 40, 160, 18, WHITE);
                break;

            case HAL_SALES_DATA_PELANGGAN:
                SalesCustomersPage(rightArea, &koneksiDB);
                break;

            case HAL_EXIT:
                jalan = false;
                break;

            default:
                DrawText("Halaman belum diimplementasi",
                         (int)rightArea.x + 40, 120, 24, WHITE);
                break;
        }

        EndDrawing();
    }
    if (uiFont.texture.id != 0) UnloadFont(uiFont);
    CloseWindow();
    disconnectDB(&koneksiDB);
    
    return 0;
}
