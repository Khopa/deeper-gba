#include "bank.h"
#include "dig.h"
#ifndef HOST_TEST
#include "bank_dig.h"
#endif

typedef struct {
    const uint8_t *img;
    int            len;
    int            count;
} BankRef;

static BankRef banks[FAM_COUNT];

static u16 rd16(const uint8_t *p) { return (u16)(p[0] | (p[1] << 8)); }
static u32 rd32(const uint8_t *p) { return (u32)(p[0] | (p[1] << 8) | (p[2] << 16) | ((u32)p[3] << 24)); }

bool bank_register(const uint8_t *image, int len)
{
    if (len < 32 || memcmp(image, "DPZ1", 4) != 0) return false;
    int family = image[4];
    if (family >= FAM_COUNT) return false;
    int count = rd16(image + 6);
    if (32 + 4 * count > len) return false;
    banks[family].img = image;
    banks[family].len = len;
    banks[family].count = count;
    return true;
}

void bank_init(void)
{
    memset(banks, 0, sizeof banks);
#ifndef HOST_TEST
    bank_register(bank_dig, bank_dig_len);
#endif
}

int bank_count(int family)
{
    return (family >= 0 && family < FAM_COUNT) ? banks[family].count : 0;
}

static int payload_len_for(const PuzzleHeader *h)
{
    switch (h->family) {
    case FAM_DIG: return dig_payload_len(h->size);
    default:      return 0;
    }
}

bool bank_get(int family, int index, BankEntry *out)
{
    if (index < 0 || index >= bank_count(family)) return false;
    const BankRef *b = &banks[family];
    u32 off = rd32(b->img + 32 + 4 * index);
    if (off + PUZZLE_HEADER_LEN > (u32)b->len) return false;
    out->hdr = (const PuzzleHeader *)(b->img + off);
    out->payload = b->img + off + PUZZLE_HEADER_LEN;
    out->payload_len = payload_len_for(out->hdr);
    return off + PUZZLE_HEADER_LEN + out->payload_len <= (u32)b->len;
}

void bank_range(int family, int lo, int hi, int *first, int *last)
{
    *first = *last = 0;
    if (!bank_count(family)) return;
    lo = clampi(lo, 0, 11);
    hi = clampi(hi, lo, 10);
    const uint8_t *ds = banks[family].img + 8;
    *first = rd16(ds + 2 * lo);
    *last = rd16(ds + 2 * (hi + 1));
}
