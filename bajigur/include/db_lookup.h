#ifndef DB_LOOKUP_H
#define DB_LOOKUP_H

#include <stdbool.h>

/* Simple lookup item for searchable pickers */
typedef struct {
    char id[16];
    char label[96];
} LookupItem;

/* Load master lists (small/light queries)
   - label is a human-friendly string (e.g. "M00001 - Toyota Avanza")
*/
bool DbLookup_LoadMobilList(void *dbcVoid, LookupItem *out, int outCap, int *outCount);
bool DbLookup_LoadCustomerList(void *dbcVoid, LookupItem *out, int outCap, int *outCount);
bool DbLookup_LoadSalesEmployeeList(void *dbcVoid, LookupItem *out, int outCap, int *outCount);

#endif
