// cyd_predecls.h
// Purpose: Provide type declarations that must exist BEFORE the Arduino builder
// auto-generates function prototypes (which it inserts after the include list).
// This prevents "does not name a type" errors for types declared later in the .ino.

#pragma once

#include <FS.h>
#include <stdint.h>

// ------------------------------
// SD flag shim (works for SD.h and SdFat-style code)
// ------------------------------
typedef uint32_t oflag_t;
#ifndef O_RDONLY
#  define O_RDONLY 0x0001
#endif
#ifndef O_WRITE
#  define O_WRITE 0x0002
#endif
#ifndef O_RDWR
#  define O_RDWR (O_RDONLY | O_WRITE)
#endif
#ifndef O_CREAT
#  define O_CREAT 0x0004
#endif
#ifndef O_TRUNC
#  define O_TRUNC 0x0008
#endif
#ifndef O_APPEND
#  define O_APPEND 0x0010
#endif

// SD file handle type used throughout the sketch.
// (Even when using SdFat, this alias is only used to satisfy Arduino prototypes;
// the real backend typedef is redefined later under the SD abstraction.)
using SDFile = fs::File;

// ------------------------------
// Forward declarations for config types (Arduino prototype generation needs these)
// ------------------------------
enum CydModel : uint8_t;
enum PanelKind : uint8_t;
enum TouchKind : uint8_t;
struct SystemConfig;

// ------------------------------
// LVGL forward include so callback parameter types exist during prototype generation
// ------------------------------
#include <lvgl.h>
