#!/usr/bin/env python3
"""
Image to Base64 Header Converter for ODMR Server
Converts PNG images to base64-encoded C header files
"""

import base64
import os
from pathlib import Path

def convert_image_to_header(input_path, output_path, variable_name):
    """Convert an image file to raw binary header format"""
    try:
        # Read the image file as binary
        with open(input_path, 'rb') as f:
            image_data = f.read()
        
        # Determine content type based on file extension
        ext = input_path.suffix.lower()
        content_types = {
            '.png': 'image/png',
            '.jpg': 'image/jpeg',
            '.jpeg': 'image/jpeg',
            '.gif': 'image/gif',
            '.svg': 'image/svg+xml'
        }
        content_type = content_types.get(ext, 'application/octet-stream')
        
        # Convert to C array format (hex bytes)
        hex_bytes = ', '.join(f'0x{b:02x}' for b in image_data)
        
        # Split into multiple lines for readability (16 bytes per line)
        hex_lines = []
        bytes_per_line = 16
        for i in range(0, len(image_data), bytes_per_line):
            chunk = image_data[i:i+bytes_per_line]
            hex_line = ', '.join(f'0x{b:02x}' for b in chunk)
            hex_lines.append(f'  {hex_line}')
        
        hex_array = ',\n'.join(hex_lines)
        
        # Create header content
        header_content = f'''// Auto-generated header file from {input_path.name}
// Do not edit manually - use convert_image.py to regenerate

#ifndef __{variable_name}_H__
#define __{variable_name}_H__

#include <pgmspace.h>

// Content type for HTTP response
const char {variable_name}_CONTENT_TYPE[] PROGMEM = "{content_type}";

// Image size in bytes
const unsigned int {variable_name}_SIZE = {len(image_data)};

// Raw image data
const unsigned char {variable_name}_DATA[] PROGMEM = {{
{hex_array}
}};

#endif // __{variable_name}_H__
'''
        
        # Write header file
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(header_content)
        
        print(f"✓ Converted {input_path.name} -> {output_path.name}")
        print(f"  File size: {len(image_data):,} bytes")
        
    except Exception as e:
        print(f"✗ Error converting {input_path.name}: {e}")
        raise

def main():
    """Main conversion function"""
    script_dir = Path(__file__).parent
    html_dir = script_dir / "src" / "website_html"
    header_dir = script_dir / "src" / "website"
    
    if not html_dir.exists():
        print(f"Error: Source directory {html_dir} does not exist")
        return False
    
    # Create output directory if it doesn't exist
    header_dir.mkdir(parents=True, exist_ok=True)
    
    # Image to convert
    image_file = html_dir / "NVGitter_optimized.png"
    output_file = header_dir / "nvgitter_png.h"
    variable_name = "NVGITTER_PNG"
    
    if image_file.exists():
        try:
            convert_image_to_header(image_file, output_file, variable_name)
            print(f"\n🎉 Image conversion complete!")
            return True
        except Exception as e:
            print(f"✗ Error: {e}")
            return False
    else:
        print(f"⚠ Image file not found: {image_file}")
        return False

if __name__ == "__main__":
    main()
