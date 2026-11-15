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
#define PML4_SHIFT 39
#define PDPT_SHIFT 30
#define PD_SHIFT 21
#define PT_SHIFT 12

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

#define LOG_DEBUG(MSG, ...)                                                    \
  if (LOG_LEVEL <= _DEBUG) {                                                   \
    Print(L"[ DEBUG ]: ");                                                     \
    Print(MSG __VA_OPT__(, ) __VA_ARGS__);                                     \
    Print(L"\n");                                                              \
  }
#define LOG_INFO(MSG, ...)                                                     \
  if (LOG_LEVEL <= _INFO) {                                                    \
    Print(L"[ INFO ]: ");                                                      \
    Print(MSG __VA_OPT__(, ) __VA_ARGS__);                                     \
    Print(L"\n");                                                              \
  }
#define LOG_WARNNING(MSG, ...)                                                 \
  if (LOG_LEVEL <= _WARNNING) {                                                \
    Print(L"[ WARNNING ]: ");                                                  \
    Print(MSG __VA_OPT__(, ) __VA_ARGS__);                                     \
    Print(L"\n");                                                              \
  }
#define LOG_ERROR(MSG, ...)                                                    \
  if (LOG_LEVEL <= _ERROR) {                                                   \
    Print(L"[ ERROR ]: ");                                                     \
    Print(MSG __VA_OPT__(, ) __VA_ARGS__);                                     \
    Print(L"\n");                                                              \
  }
