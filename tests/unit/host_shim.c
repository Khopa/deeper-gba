#include <string.h>
#include "host_shim.h"

u16 host_io[0x200];
u8  host_sram[HOST_SRAM_SIZE];

void host_reset(void)
{
    memset(host_io, 0, sizeof host_io);
    memset(host_sram, 0xFF, sizeof host_sram);     // blank cartridge SRAM
}
