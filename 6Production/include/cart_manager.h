#ifndef CART_MANAGER_H
#define CART_MANAGER_H

#include <stdbool.h>

// ============================================================
// DEFINISI: Cart Item (apa yang ada di keranjang)
// ============================================================
typedef struct {
    char MobilID[10];
    char NamaMobil[50];
    int Qty;
    long Harga;
    long Subtotal;
} CartItemData; 

// ============================================================
// FUNCTION DECLARATIONS
// ============================================================

// Tambah item ke keranjang
// Return: true jika berhasil, false jika keranjang penuh
bool CartAdd(const char *mobilID, 
             const char *namaMobil, 
             int qty, 
             long harga);

// Hapus item dari keranjang berdasarkan index
void CartRemove(int index);

// Kosongkan seluruh keranjang
void CartClear(void);

// Ambil jumlah item di keranjang
int CartGetCount(void);

// Ambil item tertentu berdasarkan index
CartItemData* CartGetItem(int index);

// Hitung total harga semua item
long CartGetTotal(void);

// Cek apakah keranjang kosong
bool CartIsEmpty(void);

// Debug: Print isi keranjang ke console
void CartDebugPrint(void);

#endif
