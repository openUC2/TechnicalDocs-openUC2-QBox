# Version Information System

## Overview

The ODMR Server firmware now includes automatic version tracking and display.

## Features

### 1. Automatic Version Generation

The `generate_version.py` script automatically creates a `version_info.h` header file with:
- Firmware version number (currently 1.0.0)
- Build date and time
- Git commit hash (short form)
- Git branch name

### 2. Version Endpoint

The firmware exposes a `/version` HTTP endpoint that returns JSON with version information:

```bash
curl http://192.168.4.1/version
```

Returns:
```json
{
  "version": "1.0.0",
  "build_date": "2025-10-19",
  "build_time": "09:34:34",
  "git_hash": "191daff",
  "git_branch": "main"
}
```

### 3. Website Display

All HTML pages automatically fetch and display version information in the footer:
- Format: `v1.0.0 | Build: 2025-10-19 | 191daff`
- Updates automatically when the page loads
- Unobtrusive styling (reduced opacity)

## Build Process

The version information is automatically generated when running:

```bash
python3 build_website.py
```

This is integrated into the standard build workflow, so no manual steps are needed.

## Updating Firmware Version

To update the firmware version number, edit `generate_version.py`:

```python
FIRMWARE_VERSION = "1.0.0"  # Change this to update version
```

Then rebuild:

```bash
python3 build_website.py
```

## Usage

### For Users
- Check the bottom of any page on the web interface to see the firmware version
- Visit http://192.168.4.1/version to get version details programmatically

### For Developers
- The version is automatically updated with each build
- Git hash allows tracking exactly which commit is running
- Build date helps identify when firmware was compiled

## Implementation Details

**Files:**
- `generate_version.py` - Generates version header
- `src/version_info.h` - Generated header file (auto-created, do not edit)
- `src/main.cpp` - Includes version header and implements /version endpoint
- All HTML files - Display version in footer

**Constants Available in Code:**
```cpp
FIRMWARE_VERSION  // "1.0.0"
BUILD_DATE        // "2025-10-19"
BUILD_TIME        // "09:34:34"
BUILD_TIMESTAMP   // "20251019093434"
GIT_HASH          // "191daff"
GIT_BRANCH        // "main"
VERSION_STRING    // "v1.0.0 (2025-10-19 09:34:34 - 191daff)"
```
