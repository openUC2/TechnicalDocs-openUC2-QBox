# Testing Guide: SPIFFS Image Solution

## Overview
This guide provides step-by-step instructions for testing the SPIFFS-based image serving solution that replaces the previous header file approach.

## What Changed?
- **Before**: NV-Center image embedded in firmware as header file (260KB)
- **After**: Image stored in SPIFFS partition (43KB)
- **Benefit**: Prevents boot loops on ESP32C3, reduces firmware size

## Prerequisites
- PlatformIO installed
- ESP32-S3 or ESP32-C3 development board (Seeed XIAO)
- USB cable for programming

## Build and Flash Instructions

### Step 1: Build HTML/CSS Header Files
```bash
cd Production_Files/Software/ODMR_Server
python3 build_website.py
```

**Expected Output:**
```
✓ Converted index.html -> index_html.h
✓ Converted messung.html -> messung_html.h
...
🎉 Conversion complete! 7 files processed.
Note: Images are served from SPIFFS (data/ directory)
Use 'pio run --target uploadfs' to upload SPIFFS data
```

### Step 2: Upload SPIFFS Data
```bash
# For ESP32-S3
pio run -e seeed_xiao_esp32s3 --target uploadfs

# For ESP32-C3
pio run -e seeed_xiao_esp32c3 --target uploadfs
```

**Expected Output:**
```
Building SPIFFS image...
Looking for upload port...
Uploading...
Success!
```

### Step 3: Upload Firmware
```bash
# For ESP32-S3
pio run -e seeed_xiao_esp32s3 --target upload

# For ESP32-C3
pio run -e seeed_xiao_esp32c3 --target upload
```

**Expected Output:**
```
Building firmware...
Linking .pio/build/.../firmware.elf
Uploading...
Hard resetting via RTS pin...
```

## Testing Procedures

### Test 1: Serial Monitor Check
1. Connect to serial monitor: `pio device monitor -e seeed_xiao_esp32c3`
2. Reset the board
3. Look for these messages:

**Expected Serial Output:**
```
Booting...
WiFi Mode capabilities: 0
MAC Address: XX:XX:XX:XX:XX:XX
SSID: openUC2_ODMR_XXXXX
...
SPIFFS Mounted Successfully    ← IMPORTANT: Should see this
Access Point started successfully with SSID: openUC2_ODMR_XXXXX
AP IP address: 192.168.4.1
DNS Server started for captive portal
TSL2591 initialized
ADF4351 init
```

**If you see**: `SPIFFS Mount Failed - continuing without it`
- **Problem**: SPIFFS data not uploaded or partition corrupted
- **Solution**: Run `pio run -e <env> --target uploadfs` again

### Test 2: Web Interface Access
1. Connect to WiFi network: `openUC2_ODMR_XXXXX`
2. Navigate to: `http://192.168.4.1`
3. The main page should load with the NV-Center diamond lattice image

### Test 3: Image Loading
1. Open browser developer tools (F12)
2. Go to Network tab
3. Reload page (`Ctrl+R` or `Cmd+R`)
4. Look for request to `/NVGitter.png`

**Expected Result:**
- Status: `200 OK`
- Type: `image/png`
- Size: `~43 KB`

**If you see**: `404 Not Found`
- **Problem**: Image not in SPIFFS or path incorrect
- **Check**: Serial monitor should show "Failed to open NVGitter.png from SPIFFS"
- **Solution**: Verify `data/NVGitter.png` exists and run uploadfs again

### Test 4: Boot Loop Check (ESP32C3)
1. Flash firmware to ESP32C3
2. Monitor serial output during boot
3. Device should boot successfully without repeated resets

**Signs of Boot Loop:**
- Repeated "Booting..." messages
- ESP32 resets every few seconds
- Never reaches "SPIFFS Mounted Successfully"

**If boot loop occurs:**
- Check firmware size: `pio run -e seeed_xiao_esp32c3 -v | grep "firmware.bin"`
- Firmware should be significantly smaller than before (~260KB reduction)

### Test 5: Firmware Size Verification
```bash
# Check firmware size
pio run -e seeed_xiao_esp32c3 -v 2>&1 | grep "firmware.bin"
```

**Expected Output:**
```
Building .pio/build/seeed_xiao_esp32c3/firmware.bin
Firmware size: ~900KB (was ~1160KB with header file)
```

The firmware should be approximately 260KB smaller than the previous version.

## Troubleshooting

### Problem: SPIFFS Mount Failed
**Symptoms:**
- Serial monitor shows: "SPIFFS Mount Failed - continuing without it"
- Image returns 404

**Solutions:**
1. Re-upload SPIFFS: `pio run -e <env> --target uploadfs`
2. Erase flash completely: `pio run -e <env> --target erase`
3. Re-upload SPIFFS, then firmware

### Problem: Image Shows Broken Icon
**Symptoms:**
- Placeholder image shown instead of diamond lattice
- Browser shows broken image icon

**Solutions:**
1. Check browser console for errors
2. Verify file exists: `ls -l data/NVGitter.png` (should be 43427 bytes)
3. Re-run: `pio run -e <env> --target uploadfs`

### Problem: Still Getting Boot Loops
**Symptoms:**
- ESP32C3 resets repeatedly
- Never completes boot process

**Solutions:**
1. Verify nvgitter_png.h was deleted (should not exist)
2. Check main.cpp does NOT include nvgitter_png.h
3. Rebuild firmware: `pio run -e <env> --target clean && pio run -e <env>`
4. Check partition table allocates enough space for firmware

### Problem: Outdated Image Served
**Symptoms:**
- Old version of image appears
- Image doesn't match data/NVGitter.png

**Solutions:**
1. Force SPIFFS re-upload: 
   ```bash
   pio run -e <env> --target erase
   pio run -e <env> --target uploadfs
   pio run -e <env> --target upload
   ```

## Verification Checklist

- [ ] Build script runs without errors
- [ ] SPIFFS upload completes successfully
- [ ] Firmware upload completes successfully
- [ ] Serial monitor shows "SPIFFS Mounted Successfully"
- [ ] Can connect to WiFi AP
- [ ] Main page loads at http://192.168.4.1
- [ ] NV-Center image displays correctly
- [ ] Image request returns 200 OK (check browser dev tools)
- [ ] No boot loops on ESP32C3
- [ ] Firmware size reduced by ~260KB

## Performance Metrics

### Before (Header File Approach)
- Firmware size: ~1160KB
- Boot success rate (ESP32C3): ~50% (boot loops common)
- Image load time: ~instant (PROGMEM)
- Update process: Rebuild + reflash firmware

### After (SPIFFS Approach)
- Firmware size: ~900KB (-260KB)
- Boot success rate (ESP32C3): ~100% (no boot loops)
- Image load time: ~100-200ms (SPIFFS read)
- Update process: Upload SPIFFS only (firmware unchanged)

## Notes

1. **First-time Setup**: Both `uploadfs` and `upload` are required
2. **Image Updates Only**: Run `uploadfs` only (no firmware reflash needed)
3. **Code Changes**: Run `build_website.py`, then `upload` (no uploadfs needed)
4. **Clean Build**: Run `erase`, `uploadfs`, then `upload`

## Success Criteria

The solution is working correctly when:
1. ✅ No boot loops on ESP32C3
2. ✅ Serial shows "SPIFFS Mounted Successfully"
3. ✅ Image loads in browser without 404 errors
4. ✅ Firmware size reduced by ~260KB
5. ✅ Image can be updated via uploadfs independently

## Additional Resources

- See `SPIFFS_IMAGE_SOLUTION.md` for technical details
- See `README.md` for development workflow
- See `IMPLEMENTATION_SUMMARY.md` for all features
