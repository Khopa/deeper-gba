// Host-side stand-ins for the GBA bits the engine sources touch, so that the
// platform-independent modules compile and run on a PC. Included by
// include/common.h when HOST_TEST is defined.
//
// Hardware registers become slots of the host_io[] array (indexed by the
// register's I/O offset / 2, like the real memory map) and cartridge SRAM
// becomes host_sram[]. Tests inspect those arrays to observe what the code
// would have done to the hardware.
#ifndef HOST_SHIM_H
#define HOST_SHIM_H

#include <stdbool.h>
#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef volatile u16 vu16;
typedef volatile u8  vu8;

#define IWRAM_CODE
#define EWRAM_DATA
#define EWRAM_BSS

// --- I/O registers ----------------------------------------------------------
extern u16 host_io[0x200];              // 0x04000000 .. 0x040003FF
#define HOST_REG(offset) (host_io[(offset) / 2])

#define REG_SND1SWEEP  HOST_REG(0x060)
#define REG_SND1CNT    HOST_REG(0x062)
#define REG_SND1FREQ   HOST_REG(0x064)
#define REG_SND2CNT    HOST_REG(0x068)
#define REG_SND2FREQ   HOST_REG(0x06C)
#define REG_SND4CNT    HOST_REG(0x078)
#define REG_SND4FREQ   HOST_REG(0x07C)
#define REG_SNDDMGCNT  HOST_REG(0x080)
#define REG_SNDDSCNT   HOST_REG(0x082)
#define REG_SNDSTAT    HOST_REG(0x084)
#define REG_WAITCNT    HOST_REG(0x204)

// --- constants copied from libtonc (tonc_memdef.h) ----------------------------
#define SSW_OFF        0x0008
#define SSQR_ENV_BUILD(ivol, dir, time) ((((ivol)) << 12) | ((dir) << 11) | (((time) & 7) << 8))
#define SFREQ_RESET    0x8000
#define SDMG_SQR1      0x01
#define SDMG_SQR2      0x02
#define SDMG_NOISE     0x08
#define SDMG_BUILD(_lmode, _rmode, _lvol, _rvol) \
    (((_rmode) << 12) | ((_lmode) << 8) | (((_rvol) & 7) << 4) | ((_lvol) & 7))
#define SDMG_BUILD_LR(_mode, _vol) SDMG_BUILD(_mode, _mode, _vol, _vol)
#define SDS_DMG100     0x0002
#define SSTAT_ENABLE   0x0080
#define WS_SRAM_8      0x0003
#define WS_STANDARD    0x4317

// --- cartridge SRAM -----------------------------------------------------------
#define HOST_SRAM_SIZE 0x8000
extern u8 host_sram[HOST_SRAM_SIZE];
#define MEM_SRAM ((uintptr_t)host_sram)

// Reset every fake register and the SRAM to a known state
void host_reset(void);

#endif
