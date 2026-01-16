#include "landingPage.h"
#include "ui.h"

void DrawLandingPage(AppState *app) {
    float tombolX = 245;
    float mulaiY = 575;
    float lebar = 220;
    float tinggi = 50;
    float jarakT = 70;

    DrawText("SELECT MENU", 210, 500, 40, WHITE);

    // Petunjuk baru yang sesuai cursor
    DrawText("*click the button to select", 215, 900, 20, WHITE);

    Rectangle btnLogin  = { tombolX, mulaiY, lebar, tinggi };
    Rectangle btnKeluar = { tombolX, (mulaiY + jarakT), lebar, tinggi };
    Rectangle kotak = { tombolX, (mulaiY + jarakT), lebar, tinggi };

    if (UIButton(btnLogin, "LOGIN", 20)) {
        app->halamanSekarang = HAL_LOGIN;
    }

    if (UIButton(btnKeluar, "CLOSE", 20)) {
        app->halamanSekarang = HAL_EXIT;
    }
}
