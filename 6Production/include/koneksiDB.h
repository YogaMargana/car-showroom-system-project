#ifndef KONEKSI_DB_H
#define KONEKSI_DB_H

typedef void *SQLHANDLE;
typedef SQLHANDLE SQLHDBC;

void connectKeDB(SQLHDBC *koneksiDb);
void disconnectDB(SQLHDBC *koneksiDb);

#endif
