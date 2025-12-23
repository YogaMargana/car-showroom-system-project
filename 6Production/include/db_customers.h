#ifndef DB_CUSTOMERS_H
#define DB_CUSTOMERS_H

#include <stdbool.h>
#include "models.h"

bool DbCustomers_LoadAll(void *dbc, Customer *out, int outCap, int *outCount);
bool DbCustomers_Insert(void *dbc, const char *nama, const char *email, const char *noHp, const char *alamat);
bool DbCustomers_Update(void *dbc, const char *pelangganId, const char *nama, const char *email, const char *noHp, const char *alamat);
bool DbCustomers_Delete(void *dbc, const char *pelangganId);

#endif
