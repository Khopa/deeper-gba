// Access to the puzzle banks embedded in ROM (format: docs/puzzle_bank.md).
#ifndef BANK_H
#define BANK_H

#include "common.h"

typedef struct {
    const PuzzleHeader *hdr;
    const uint8_t      *payload;
    int                 payload_len;
} BankEntry;

// Banks are registered once at boot (bank_init) from the generated arrays.
void bank_init(void);
// Optional: register a bank image explicitly (tests use this)
bool bank_register(const uint8_t *image, int len);

int  bank_count(int family);                          // 0 if the family has no bank
bool bank_get(int family, int index, BankEntry *out);
// Index range [first, last) of the puzzles whose difficulty is in [lo, hi]
void bank_range(int family, int lo, int hi, int *first, int *last);

#endif
