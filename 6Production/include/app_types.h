#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <raylib.h>
#include <stdbool.h>
#include "page_type.h"


typedef struct {
    int screenW;
    int screenH;

    char textJam[10];

    Halaman halamanSekarang;

    Role roleAktif;

    char currentKaryawanID[16];
    char currentNama[64];
    char currentUsername[64];

    Texture2D logo;
    Texture2D Landing;
    Texture2D login;
} AppState;

#endif
