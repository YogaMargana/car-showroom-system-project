#include "loginPage.h"
#include "ui.h"
#include "auth.h"
#include <stdio.h>
#include <string.h>

// TIDAK ADA lagi static bool DBCheckLogin di sini
void InitLoginState(LoginState *ls) {
    memset(ls->username, 0, sizeof(ls->username));
    memset(ls->password, 0, sizeof(ls->password));
    ls->showError = false;

    // Layout dasar
    float x     = 245;
    float boxW  = 280;
    float boxH  = 50;

    // Mulai agak turun agar ruang label lebih lega
    float userBoxY = 590;

    // Jarak antar field (biar label Password tidak dempet Username)
    float fieldGap  = 70;
    float passBoxY  = userBoxY + boxH + fieldGap;

    UITextBoxInit(&ls->tbUser, (Rectangle){ x, userBoxY, boxW, boxH },
                  ls->username, 50, false);
    UITextBoxInit(&ls->tbPass, (Rectangle){ x, passBoxY, boxW, boxH },
                  ls->password, 50, true);
}

void UpdateLoginPage(LoginState *ls) {
    UITextBoxUpdate(&ls->tbUser);
    UITextBoxUpdate(&ls->tbPass);
}

bool DrawLoginPage(AppState *app, LoginState *ls,
                   SQLHDBC *conn, const char *title) {

    DrawText(title, 220, 500, 36, WHITE);

    float x     = ls->tbUser.bounds.x;
    float userY = ls->tbUser.bounds.y;
    float passY = ls->tbPass.bounds.y;
    float boxW  = ls->tbUser.bounds.width;
    float boxH  = ls->tbUser.bounds.height;

    int   labelFont   = 18;
    float labelOffset = 28;

    // Username
    DrawText("Username", (int)x, (int)(userY - labelOffset), labelFont, WHITE);
    UITextBoxDraw(&ls->tbUser, 20);

    // Password
    DrawText("Password", (int)x, (int)(passY - labelOffset), labelFont, WHITE);
    UITextBoxDraw(&ls->tbPass, 20);

    // Tombol
    float btnH          = 50;
    float btnGap        = 20;
    float afterFieldGap = 50;
    float btnMasukY     = passY + boxH + afterFieldGap;

    Rectangle btnMasuk = { x, btnMasukY, boxW, btnH };
    Rectangle btnBack  = { x, btnMasukY + btnH + btnGap, boxW, btnH };

    bool klikMasuk = UIButton(btnMasuk, "LOGIN", 20);
    bool klikBack  = UIButton(btnBack,  "BACK",  20);

    // BACK -> balik ke landing
    if (klikBack) {
        ls->showError        = false;
        app->roleAktif       = ROLE_NONE;
        app->halamanSekarang = HAL_LANDING;
        return false;
    }

    if (klikMasuk) {
        Role role = ROLE_NONE;

        // Cek ke database melalui auth.c
        char kid[16] = {0};
        char knama[64] = {0};

        if (DBCheckLogin(conn, ls->username, ls->password, &role, kid, (int)sizeof(kid), knama, (int)sizeof(knama))) {
            ls->showError  = false;
            app->roleAktif = role;

            // simpan session login
            snprintf(app->currentKaryawanID, (int)sizeof(app->currentKaryawanID), "%s", kid);
            snprintf(app->currentNama, (int)sizeof(app->currentNama), "%s", knama);
            snprintf(app->currentUsername, (int)sizeof(app->currentUsername), "%s", ls->username);

            if (role == ROLE_ADMIN) {
                app->halamanSekarang = HAL_DASHBOARD_ADMIN;
            } else if (role == ROLE_CASHIER) {
                app->halamanSekarang = HAL_DASHBOARD_CASHIER;
            } else if (role == ROLE_SALES) {
                app->halamanSekarang = HAL_DASHBOARD_SALES;
            } else {
                app->halamanSekarang = HAL_LANDING;
            }

            // Bersihkan textbox setelah login sukses
            strcpy(ls->tbUser.buffer, "");
            strcpy(ls->tbPass.buffer, "");
            ls->tbUser.len = 0;
            ls->tbPass.len = 0;

            return true;
        } else {
            // username / password salah
            ls->showError = true;
        }
    }

    if (ls->showError) {
        DrawText("Login failed. Check username/password.",
                 (int)x,
                 (int)(btnBack.y + btnBack.height + 20),
                 18,
                 RED);
    }

    return false;
}
