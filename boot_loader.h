#include <efi.h>
#include <efilib.h>
#include <elf.h>

// tables entries flags
#define P 0x0001
#define R_W 0x0002
#define U_S 0x0004
#define PWT 0x0008
#define PCD 0x0010
#define A 0x0020
#define D 0x0040
#define PS 0x0080
#define G 0x0100

//
#define PML4_SHIFT 38
#define PDPT_SHIFT 29
#define PD_SHIFT 20
#define PT_SHIFT 11

//
#define PAGE_TABLE_IDX_MASK 0x01FF

// Logger
#define _DEBUG 0
#define _INFO 1
#define _WARNNING 2
#define _ERROR 3

#ifndef LOG_LEVEL
#define LOG_LEVEL _ERROR
#endif

#define LOG(LEVEL, MSG, ...)                                                   \
  if (LEVEL >= LOG_LEVEL) {                                                    \
    Print(L#MSG, __VA_ARGS__)                                                  \
  }
