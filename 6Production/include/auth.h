#ifndef AUTH_H
#define AUTH_H

#include <stdbool.h>
#include "koneksiDB.h"

#include "page_type.h"

bool DBCheckLogin(SQLHDBC *conn,
                  const char *username,
                  const char *password,
                  Role *outRole,
                  char *outKaryawanID,
                  int outKaryawanIDSize,
                  char *outNama,
                  int outNamaSize);


#endif
