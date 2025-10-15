# Testing Guide for Website Fixes

## Prerequisites
- ESP32-S3 or ESP32-C3 with ODMR firmware
- Computer with WiFi capability
- Web browser (Chrome, Edge, or Firefox)

## Test 1: Root Path Routing

### Objective
Verify that accessing http://192.168.4.1 serves index.html directly without redirecting to /static/

### Steps
1. Flash the updated firmware to the ESP32
2. Connect to the WiFi access point (ODMR_XXXXXX)
3. Open a web browser
4. Navigate to `http://192.168.4.1`
5. Observe the browser's address bar

### Expected Results
- ✓ The page loads successfully
- ✓ The URL in the address bar remains `http://192.168.4.1` (or shows `http://192.168.4.1/`)
- ✓ No redirect to `/static/index.html` occurs
- ✓ The index page content is displayed

### Failure Indicators
- ✗ Browser shows `http://192.168.4.1/static/index.html` in address bar
- ✗ 404 error page
- ✗ Infinite redirect loop

## Test 2: Image Display

### Objective
Verify that the NVGitter.png (diamond lattice) image displays correctly on the index page

### Steps
1. While on the index page (http://192.168.4.1)
2. Scroll down to the section "Über das Experiment"
3. Look for "Abbildung 1: Struktur des Diamantgitters"
4. Open browser developer tools (F12)
5. Go to the Network tab
6. Reload the page
7. Filter by "png" or look for "NVGitter.png"

### Expected Results
- ✓ The diamond lattice image is visible on the page
- ✓ The image is properly sized and centered
- ✓ Network tab shows `NVGitter.png` loaded with status 200
- ✓ Image URL is `http://192.168.4.1/NVGitter.png`
- ✓ Content-Type header is `image/png`

### Failure Indicators
- ✗ Broken image icon or placeholder
- ✗ 404 error for NVGitter.png in network tab
- ✗ Image not loading at all

## Test 3: Memory and Performance

### Objective
Verify that the firmware fits within ESP32 memory constraints and performs well

### Steps
1. During firmware compilation, check the build output
2. After flashing, monitor the serial console
3. Test page load times

### Expected Results
- ✓ Firmware compiles successfully
- ✓ Flash usage is below 90% of available flash
- ✓ ESP32 boots without errors
- ✓ Page loads within 2-3 seconds
- ✓ Image loads within 1-2 seconds

### Failure Indicators
- ✗ Compilation errors about program size
- ✗ ESP32 crashes or reboots
- ✗ Very slow page/image loading (>10 seconds)

## Test 4: Multi-Page Navigation

### Objective
Verify that all pages load correctly without /static/ issues

### Steps
1. From index page, navigate to each menu item:
   - Messung (on Device)
   - Messung (WebSerial)
   - Justage
   - Weitere Infos
2. Observe URLs and functionality

### Expected Results
- ✓ All pages load correctly
- ✓ URLs are clean (e.g., `/messung.html`, not `/static/messung.html`)
- ✓ Navigation works smoothly
- ✓ Language switcher persists across pages

## Test 5: Build Process

### Objective
Verify the build scripts work correctly

### Steps
1. On development machine, navigate to ODMR_Server directory
2. Run: `python3 build_website.py`
3. Check output and generated files

### Expected Results
- ✓ Script completes without errors
- ✓ Shows "Conversion complete! 6 files processed"
- ✓ Shows "Image conversion complete!"
- ✓ All header files updated in `src/website/`
- ✓ `nvgitter_png.h` is present and ~707KB

## Debugging Tips

### If root path redirects to /static/:
1. Check that the firmware was actually flashed (verify version/timestamp)
2. Clear browser cache
3. Try a different browser
4. Check serial console for any routing errors

### If image doesn't display:
1. Check Network tab in browser dev tools for the actual error
2. Verify `nvgitter_png.h` exists in `src/website/`
3. Check that the include is present in `main.cpp` (line 21)
4. Verify the image handler code is present (around line 210-216)

### If compilation fails:
1. Verify all header files were generated
2. Check that `nvgitter_png.h` is not corrupted
3. Try removing `nvgitter_png.h` temporarily to isolate the issue
4. Check available flash memory in partition table

## Success Criteria

All tests should pass with the following overall results:
- ✓ No redirects to /static/ paths
- ✓ All images display correctly from header files
- ✓ Fast page load times
- ✓ Stable operation without crashes
- ✓ Clean URLs throughout the application

## Reporting Issues

If any tests fail, please report:
1. Which test failed
2. ESP32 model (S3 or C3)
3. Serial console output
4. Browser console errors (F12 → Console tab)
5. Network tab screenshot showing the failed request
