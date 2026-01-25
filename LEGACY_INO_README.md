# Legacy Firmware Archive

## File Renamed

**Original:** `CYD_Framework_LGFX_LVGL_Final_COMPILE_OK_FIXED6.ino`  
**Renamed to:** `CYD_Framework_LGFX_LVGL_Final_COMPILE_OK_FIXED6.ino.legacy`

## Reason

This file has been renamed to prevent compilation conflicts with the new modular architecture.

### The Problem

The repository contains TWO entry points:
1. **Legacy monolithic:** `CYD_Framework_LGFX_LVGL_Final_COMPILE_OK_FIXED6.ino` (~4000 lines)
   - Contains its own `setup()` and `loop()` functions
   - Defines global variables: `WebServer server`, `SerialWombat sw`, etc.

2. **Modern modular:** `src/main.cpp` (19 lines)
   - Also contains `setup()` and `loop()` functions
   - Delegates to `App::getInstance().begin()` and `.update()`
   - Uses refactored code in `src/app/`, `src/services/`, `src/hal/`, etc.

When PlatformIO compiles with `src_dir = .` (root directory), it compiles **both** files, causing:
- ❌ Duplicate definition of `setup()`
- ❌ Duplicate definition of `loop()`
- ❌ Duplicate global variables (`SerialWombat sw`, etc.)
- ❌ Build failure with linker errors

### The Solution

**Renamed the .ino file to `.ino.legacy`** so it won't be compiled, while preserving it for reference.

## Current Architecture

The project now uses the **modular architecture**:

```
src/main.cpp (entry point)
  └─> App::getInstance()
      ├─> services/ (web server, security, i2c, etc.)
      ├─> hal/ (display, storage, gpio, adc)
      ├─> ui/ (LVGL, screens, components)
      ├─> core/ (messages, types, globals)
      └─> config/ (configuration management)
```

## For Developers

### If you need to reference legacy code:
```bash
# The legacy file is preserved with .legacy extension
cat CYD_Framework_LGFX_LVGL_Final_COMPILE_OK_FIXED6.ino.legacy
```

### If you want to use ONLY the legacy code:
```bash
# Rename it back (will break modular build):
git mv CYD_Framework_LGFX_LVGL_Final_COMPILE_OK_FIXED6.ino.legacy \\
       CYD_Framework_LGFX_LVGL_Final_COMPILE_OK_FIXED6.ino

# OR change platformio.ini to exclude src/:
# src_dir = .
# build_src_filter = -<src/>
```

But this is **not recommended** - use the modular architecture instead.

## Migration Status

✅ **Refactoring Complete**
- All code extracted to modular components
- New entry point: `src/main.cpp`
- 16 modules created with 32 files
- Security code preserved unchanged
- Full documentation in `REFACTORING_COMPLETION_SUMMARY.md`

## Build Instructions

```bash
# Install dependencies
pip install platformio==6.1.16

# Build (uses src/main.cpp entry point)
pio run --environment esp32-s3-devkit

# Upload
pio run --target upload
```

The modular architecture is now the primary codebase. The legacy .ino file is preserved for historical reference only.
