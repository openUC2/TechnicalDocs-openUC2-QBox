# SPIFFS Partition Solution for NV-Center Image

## Problem Statement

The NV-Center image (NVGitter.png) was originally stored as a C header file (`nvgitter_png.h`) containing raw binary data. This approach caused several issues:

1. **Boot Loops on ESP32C3**: Large header files (260KB+) embedded in firmware can cause the ESP32C3 to enter a boot loop
2. **Increased Firmware Size**: Embedding images in PROGMEM increases the firmware size significantly
3. **Memory Constraints**: ESP32C3 has limited program memory compared to ESP32S3

## Solution: SPIFFS Partition

Instead of embedding the image in the firmware, we now store it in the SPIFFS (SPI Flash File System) partition:

### Architecture Changes

```
Before:
┌─────────────────────────────────┐
│ Firmware (with 260KB image)     │ ← Boot loop risk
│ - HTML/CSS in header files      │
│ - Image in nvgitter_png.h       │
└─────────────────────────────────┘
│ SPIFFS Partition (unused)       │
└─────────────────────────────────┘

After:
┌─────────────────────────────────┐
│ Firmware (without image)        │ ← 260KB smaller
│ - HTML/CSS in header files      │
│ - SPIFFS code to serve image    │
└─────────────────────────────────┘
│ SPIFFS Partition                │
│ - NVGitter.png (43KB optimized) │ ← Image stored here
└─────────────────────────────────┘
```

### Implementation Details

#### 1. SPIFFS Initialization

The firmware now initializes SPIFFS during startup:

```cpp
// Initialize SPIFFS for serving images
if (!SPIFFS.begin(true))
{
  Serial.println("SPIFFS Mount Failed - continuing without it");
}
else
{
  Serial.println("SPIFFS Mounted Successfully");
}
```

The `true` parameter enables auto-formatting if SPIFFS is not formatted.

#### 2. Image Serving

A dedicated route handler serves the image from SPIFFS:

```cpp
server.on("/NVGitter.png", HTTP_GET, []()
{
  File file = SPIFFS.open("/NVGitter.png", "r");
  if (!file)
  {
    Serial.println("Failed to open NVGitter.png from SPIFFS");
    server.send(404, "text/plain", "Image not found");
    return;
  }
  server.streamFile(file, "image/png");
  file.close();
});
```

#### 3. SPIFFS Partition Allocation

The partition tables already allocate space for SPIFFS:

**ESP32-C3** (`custom_partition_esp32c3.csv`):
```
spiffs,   data,   spiffs,  0x2B0000, 0x150000,  # 1.3 MB
```

**ESP32-S3** (`custom_partition_esp32s3.csv`):
```
spiffs,   data, spiffs,  0x291000,0x16F000  # 1.4 MB
```

## Deployment Process

### 1. Build HTML/CSS Header Files

```bash
cd Production_Files/Software/ODMR_Server
python3 build_website.py
```

This converts HTML/CSS files to header files but **skips** image conversion.

### 2. Upload SPIFFS Data

Upload the `data/` directory contents to the SPIFFS partition:

```bash
# For ESP32-S3
pio run -e seeed_xiao_esp32s3 --target uploadfs

# For ESP32-C3
pio run -e seeed_xiao_esp32c3 --target uploadfs
```

### 3. Upload Firmware

```bash
# For ESP32-S3
pio run -e seeed_xiao_esp32s3 --target upload

# For ESP32-C3
pio run -e seeed_xiao_esp32c3 --target upload
```

## Benefits

1. **No Boot Loops**: Firmware size is reduced by 260KB, preventing ESP32C3 boot loops
2. **Flexibility**: Images can be updated independently from firmware
3. **Best of Both Worlds**: HTML/CSS remain in header files (fast access), images in SPIFFS (smaller firmware)
4. **Existing Infrastructure**: Uses the already-allocated SPIFFS partition

## File Structure

```
Production_Files/Software/ODMR_Server/
├── data/                          # SPIFFS data directory
│   └── NVGitter.png              # 43KB optimized image
├── src/
│   ├── main.cpp                  # Includes SPIFFS serving code
│   ├── website/                  # Header files
│   │   ├── index_html.h
│   │   ├── style_css.h
│   │   └── ...                   # Other HTML/CSS headers
│   └── website_html/             # Source HTML/CSS files
│       ├── NVGitter.png          # Original 503KB image
│       ├── NVGitter_optimized.png # 43KB optimized version
│       └── *.html, *.css
├── build_website.py              # Builds header files (not images)
├── optimize_image.py             # Optimizes images to 43KB
└── platformio.ini                # Project configuration
```

## Testing

### Verify SPIFFS Upload

After uploading SPIFFS data, connect to the serial monitor:

```
Booting...
WiFi Mode capabilities: 0
MAC Address: XX:XX:XX:XX:XX:XX
SSID: openUC2_ODMR_XXXXX
...
SPIFFS Mounted Successfully    ← Should see this
```

### Verify Image Serving

1. Connect to the WiFi AP: `openUC2_ODMR_XXXXX`
2. Navigate to `http://192.168.4.1`
3. The NV-Center diamond lattice image should be visible
4. Check browser console for any 404 errors on `/NVGitter.png`

### Troubleshooting

**Issue**: "SPIFFS Mount Failed"
- **Solution**: Run `pio run -e <env> --target uploadfs` to format and upload SPIFFS

**Issue**: Image shows 404 error
- **Solution**: Ensure `data/NVGitter.png` exists and run uploadfs command

**Issue**: Still experiencing boot loops
- **Solution**: Check total firmware size with `pio run -e <env> -v` and ensure it fits in partition

## Technical Notes

### Why Not All Files in SPIFFS?

HTML/CSS files remain in header files because:
1. They compress well and are relatively small
2. Header files provide faster access (no filesystem overhead)
3. No risk of SPIFFS corruption affecting website functionality
4. Simpler deployment (one firmware upload vs. two steps)

### Image Optimization

The original 503KB image is optimized to 43KB:
- Resized from 1385x1385 to 300x300 pixels
- Converted from RGBA to RGB (removes alpha channel)
- PNG optimization applied

Use `optimize_image.py` to re-optimize if needed.

## References

- [SPIFFS Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/spiffs.html)
- [PlatformIO SPIFFS Upload](https://docs.platformio.org/en/latest/platforms/espressif32.html#uploading-files-to-file-system-spiffs)
- Original Issue: "Use of Spiff-Partition for NV-Center Image"
