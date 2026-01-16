#include "cart_manager.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// CONFIGURATION
// ============================================================
#define MAX_CART_ITEMS 50  // Max 50 item dalam 1 keranjang

// ============================================================
// STATIC VARIABLES (Private, hanya bisa diakses di file ini)
// ============================================================
static CartItemData gCart[MAX_CART_ITEMS];   // Array untuk menyimpan items
static int gCartCount = 0;               // Jumlah items saat ini

// ============================================================
// IMPLEMENTATION
// ============================================================

bool CartAdd(const char *mobilID, 
             const char *namaMobil, 
             int qty, 
             long harga)
{
    // ✓ CEK #1: Keranjang sudah penuh?
    if (gCartCount >= MAX_CART_ITEMS)
    {
        printf("ERROR: Keranjang penuh! Max %d item\n", MAX_CART_ITEMS);
        return false;
    }

    // ✓ CEK #2: Parameter valid?
    if (!mobilID || !namaMobil || qty <= 0 || harga <= 0)
    {
        printf("ERROR: Parameter tidak valid!\n");
        return false;
    }

    // ✓ CEK #3: Mobil sudah ada di keranjang? (update qty instead of add)
    for (int i = 0; i < gCartCount; i++)
    {
        if (strcmp(gCart[i].MobilID, mobilID) == 0)
        {
            gCart[i].Qty += qty;  // ← Tambah qty, jangan duplikat item
            gCart[i].Subtotal = gCart[i].Qty * gCart[i].Harga;
            printf("✓ Update: %s qty sekarang %d\n", namaMobil, gCart[i].Qty);
            return true;
        }
    }

    // ✓ JIKA item baru, masukkan ke keranjang
    CartItemData newItem;
    memset(&newItem, 0, sizeof(newItem));
    
    strncpy(newItem.MobilID, mobilID, sizeof(newItem.MobilID) - 1);
    strncpy(newItem.NamaMobil, namaMobil, sizeof(newItem.NamaMobil) - 1);
    newItem.Qty = qty;
    newItem.Harga = harga;
    newItem.Subtotal = qty * harga;

    gCart[gCartCount] = newItem;
    gCartCount++;

    printf("✓ Ditambahkan: %s (Rp%ld) x%d\n", namaMobil, harga, qty);
    return true;
}

void CartRemove(int index)
{
    // CEK: Index valid?
    if (index < 0 || index >= gCartCount)
    {
        printf("ERROR: Index tidak valid!\n");
        return;
    }

    // Geser item setelahnya ke depan
    // Contoh: remove index 1 dari 3 items
    // [0][1][2] → [0][2][-]
    for (int i = index; i < gCartCount - 1; i++)
    {
        gCart[i] = gCart[i + 1];
    }

    gCartCount--;
    printf("✓ Item dihapus. Sisa: %d item\n", gCartCount);
}

void CartClear(void)
{
    // Kosongkan semua data
    memset(gCart, 0, sizeof(gCart));
    gCartCount = 0;
    printf("✓ Keranjang dikosongkan\n");
}

int CartGetCount(void)
{
    return gCartCount;
}

CartItemData* CartGetItem(int index)
{
    if (index < 0 || index >= gCartCount)
        return NULL;
    return &gCart[index];
}

long CartGetTotal(void)
{
    long total = 0;
    for (int i = 0; i < gCartCount; i++)
    {
        total += gCart[i].Subtotal;  // Subtotal = Qty * Harga
    }
    return total;
}

bool CartIsEmpty(void)
{
    return gCartCount == 0;
}

void CartDebugPrint(void)
{
    printf("\n=== CART DEBUG INFO ===\n");
    printf("Total items: %d\n", gCartCount);
    
    for (int i = 0; i < gCartCount; i++)
    {
        printf("[%d] %s x%d = Rp%ld (unit: Rp%ld)\n",
            i,
            gCart[i].NamaMobil,
            gCart[i].Qty,
            gCart[i].Subtotal,
            gCart[i].Harga);
    }
    
    printf("TOTAL: Rp%ld\n", CartGetTotal());
    printf("======================\n\n");
}
