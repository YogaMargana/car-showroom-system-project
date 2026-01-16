#ifndef LOGIN_PAGE_H
#define LOGIN_PAGE_H

#include <stdbool.h>

#include "app_types.h"
#include "textbox.h"
#include "koneksiDB.h"

typedef struct {
    char username[50];
    char password[50];
    bool showError;
    UITextBox tbUser;
    UITextBox tbPass;
} LoginState;

void InitLoginState(LoginState *ls);
void UpdateLoginPage(LoginState *ls);

bool DrawLoginPage(AppState *app,
                   LoginState *ls,
                   SQLHDBC *conn,
                   const char *title);

#endif