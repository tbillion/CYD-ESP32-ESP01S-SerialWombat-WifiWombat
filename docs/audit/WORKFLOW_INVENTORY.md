# GitHub Actions Workflow Inventory

**Generated:** 2026-01-25  
**Repository:** tbillion/CYD-ESP32-ESP01S-SerialWombat-WifiWombat

## Overview

This document provides a comprehensive inventory of all GitHub Actions workflows in this repository, including their configuration requirements, dependencies, and potential issues.

---

## Workflows

### 1. Build and Test (`.github/workflows/build.yml`)

**Purpose:** Build firmware for multiple ESP32 variants and perform code quality checks.

**Trigger Conditions:**
- Push to branches: `main`, `develop`, `copilot/**`
- Pull requests to: `main`, `develop`
- Manual dispatch (`workflow_dispatch`)

**Jobs:**

#### 1.1 Build Firmware
- **Runner:** `ubuntu-latest`
- **Strategy:** Matrix build for 4 environments:
  - `esp32-s3-devkit`
  - `esp32-devkit`
  - `esp32-2432S028`
  - `esp32-8048S070`

**Required Tools:**
- Python 3.11
- PlatformIO 6.1.16

**Required Config Files:**
- ✅ `platformio.ini` (exists)
- ❌ `requirements.txt` or `pyproject.toml` (referenced by pip cache but MISSING)

**Required Environment Variables:**
- `PLATFORMIO_BUILD_FLAGS` (set inline)

**Actions Used:**
- `actions/checkout@de0fac2e4500dabe0009e67214ff5f5447ce83dd` (v6.0.2) ✅
- `actions/cache@8b402f58fbc84540c8b491a91e594a4576fec3d7` (v5.0.2) ✅
- `actions/setup-python@a309ff8b426b58ec0e2a45f0f869d46889d02405` (v6.2.0) ✅
- `actions/upload-artifact@b7c566a772e6b6bfb58ed0dc250532a479d7789f` (v6.0.0) ✅

**Issues:**
- ❌ **CRITICAL:** `cache: pip` in setup-python step expects requirements.txt but file doesn't exist
- ⚠️ Using SHA pins instead of version tags (acceptable but less readable)

#### 1.2 Code Quality Checks (Lint)
- **Runner:** `ubuntu-latest`

**Required Tools:**
- clang-format 18

**Required Config Files:**
- ✅ `.clang-format` (exists)

**Actions Used:**
- `actions/checkout@de0fac2e4500dabe0009e67214ff5f5447ce83dd` (v6.0.2) ✅
- `jidicula/clang-format-action@6cd220de46c89139a0365edae93eee8eb30ca8fe` (v4.16.0) ✅

**Issues:**
- ❌ **CRITICAL:** Multiple source files fail clang-format validation
- Files with violations: `src/config/config_manager.cpp`, `src/main.cpp`, etc.

---

### 2. Copilot Setup Steps (`.github/workflows/copilot-setup-steps.yml`)

**Purpose:** Install and configure development tools for GitHub Copilot agent environment.

**Trigger Conditions:**
- Manual dispatch (`workflow_dispatch`)
- Push to workflow file
- Pull request to workflow file

**Jobs:**

#### 2.1 copilot-setup-steps
- **Runner:** `ubuntu-latest`

**Required Tools:**
- Python 3.11
- PlatformIO (latest)

**Required Config Files:**
- ✅ `platformio.ini` (conditionally checked)

**Actions Used:**
- `actions/checkout@v5` ⚠️ (should use SHA pin)
- `actions/setup-python@v5` ⚠️ (should use SHA pin)
- `actions/cache@v4` ⚠️ (should use SHA pin)

**Features:**
- ✅ Conditionally detects Python dependency files before enabling pip cache
- ✅ Safe fallback when no platformio.ini exists
- ✅ Best-effort package installation

**Issues:**
- ⚠️ Using version tags without SHA pins (less secure but still acceptable)
- ✅ Properly handles missing dependency files (GOOD)

---

### 3. Security Scanning (`.github/workflows/security.yml`)

**Purpose:** Run security scans (secrets, dependencies, and code analysis).

**Trigger Conditions:**
- Push to branches: `main`, `develop`, `copilot/**`
- Pull requests to: `main`, `develop`
- Scheduled: Weekly on Mondays at 9 AM UTC
- Manual dispatch (`workflow_dispatch`)

**Jobs:**

#### 3.1 Secret Detection
- **Runner:** `ubuntu-latest`

**Required Tools:**
- Gitleaks

**Actions Used:**
- `actions/checkout@de0fac2e4500dabe0009e67214ff5f5447ce83dd` (v6.0.2) ✅
- `gitleaks/gitleaks-action@4a20c8b69d025d0c91ec010e1dcd68f3a28bc18b` (v2.3.6) ✅

**Issues:**
- ✅ No issues detected

#### 3.2 Dependency Vulnerability Scan
- **Runner:** `ubuntu-latest`

**Required Tools:**
- OSV-Scanner

**Required Config Files:**
- ✅ `platformio.ini` (used as lockfile)

**Actions Used:**
- `actions/checkout@de0fac2e4500dabe0009e67214ff5f5447ce83dd` (v6.0.2) ✅
- `google/osv-scanner-action@5a0f5ffe4b3e8c4d96a17f5ec6a609b78e85f86d` (v1.9.1) ✅
- `actions/upload-artifact@b7c566a772e6b6bfb58ed0dc250532a479d7789f` (v6.0.0) ✅

**Issues:**
- ⚠️ OSV-Scanner may not fully support platformio.ini as a lockfile format
- ⚠️ Step marked as `continue-on-error: true` (acceptable for now)

#### 3.3 CodeQL Analysis
- **Runner:** `ubuntu-latest`
- **Strategy:** Matrix for language: `c-cpp`

**Required Tools:**
- Python 3.11
- PlatformIO 6.1.16

**Required Config Files:**
- ✅ `platformio.ini`

**Actions Used:**
- `actions/checkout@de0fac2e4500dabe0009e67214ff5f5447ce83dd` (v6.0.2) ✅
- `github/codeql-action/init@662472033e021d55d94146f66f6058822889cafe` (v3.27.9) ✅
- `actions/setup-python@a309ff8b426b58ec0e2a45f0f869d46889d02405` (v6.2.0) ✅
- `github/codeql-action/analyze@662472033e021d55d94146f66f6058822889cafe` (v3.27.9) ✅

**Issues:**
- ⚠️ Build step marked as `continue-on-error: true` (acceptable for CodeQL)
- ✅ No Python dependency file issues (no cache used)

---

## Configuration Files Audit

### Present
- ✅ `platformio.ini` - PlatformIO project configuration
- ✅ `.clang-format` - Code formatting rules
- ✅ `.github/dependabot.yml` - Dependabot configuration
- ✅ `.gitignore` - Git ignore rules

### Missing (but referenced)
- ❌ `requirements.txt` - Referenced by build.yml pip cache
- ❌ `pyproject.toml` - Alternative Python dependency file
- ❌ `poetry.lock` - Alternative Python dependency file

### Not Required (but could be useful)
- `platformio.lock` - Would provide reproducible builds
- `lv_conf.h` - LVGL configuration (library may use defaults)

---

## Action Versions Summary

### Latest Versions Used ✅
- `actions/checkout@v6.0.2` (SHA: de0fac2...)
- `actions/cache@v5.0.2` (SHA: 8b402f5...)
- `actions/setup-python@v6.2.0` (SHA: a309ff8...)
- `actions/upload-artifact@v6.0.0` (SHA: b7c566a...)

### Needs SHA Pinning ⚠️
- `actions/checkout@v5` in copilot-setup-steps.yml
- `actions/setup-python@v5` in copilot-setup-steps.yml
- `actions/cache@v4` in copilot-setup-steps.yml

### Third-Party Actions ✅
- `jidicula/clang-format-action@v4.16.0` (SHA pinned)
- `gitleaks/gitleaks-action@v2.3.6` (SHA pinned)
- `google/osv-scanner-action@v1.9.1` (SHA pinned)
- `github/codeql-action@v3.27.9` (SHA pinned)

---

## Critical Issues Requiring Immediate Fix

### Priority 1: Build Failures
1. **Remove pip cache from build.yml**
   - The project uses PlatformIO, which doesn't need a requirements.txt
   - Remove `cache: 'pip'` from setup-python step
   - OR create a minimal requirements.txt with just `platformio==6.1.16`

2. **Fix clang-format violations**
   - Run clang-format on all source files
   - Multiple files have formatting violations

### Priority 2: Action Version Updates
3. **Update copilot-setup-steps.yml to use SHA pins**
   - More secure and consistent with other workflows
   - Update to latest versions with SHA pins

### Priority 3: Documentation
4. **Create comprehensive audit documentation**
   - ✅ This inventory document
   - Next: WORKFLOW_FIX_SUMMARY.md

---

## Recommendations

### Immediate Actions
1. Remove pip cache from build.yml (no requirements.txt needed)
2. Run clang-format on all source files to fix violations
3. Update copilot-setup-steps.yml action versions to match build.yml
4. Test all workflows after fixes

### Future Enhancements
1. Consider adding platformio.lock for reproducible builds
2. Add workflow status badges to README.md
3. Consider adding a build matrix test job
4. Add pre-commit hooks for clang-format

---

## Workflow Execution Status

As of 2026-01-25:
- ❌ Build and Test: FAILING (pip cache + formatting)
- ⚠️ Copilot Setup Steps: Not recently tested
- ⚠️ Security Scanning: May have issues (action_required status)

**Goal:** All workflows should pass on next commit after fixes applied.
