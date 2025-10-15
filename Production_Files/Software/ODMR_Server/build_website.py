#!/usr/bin/env python3
"""
HTML to Header Converter for ODMR Server
Converts HTML files from src/website_html/ to header files in src/website/

This script provides automated conversion of web interface files for embedded systems,
enabling development in standard HTML/CSS/JS files with automatic build integration.
"""

import os
import re
import sys
from pathlib import Path

def sanitize_variable_name(filename):
    """Convert filename to valid C variable name"""
    name = filename.upper()
    name = re.sub(r'[^A-Z0-9_]', '_', name)
    # Remove file extension
    name = re.sub(r'_HTML$|_CSS$|_JS$', '', name)
    return name

def escape_c_string(content):
    """Escape content for C string literal"""
    # Replace backslashes first
    content = content.replace('\\', '\\\\')
    # Replace quotes
    content = content.replace('"', '\\"')
    # Replace newlines with \n
    content = content.replace('\n', '\\n')
    # Replace carriage returns
    content = content.replace('\r', '')
    return content

def convert_file_to_header(input_path, output_path, variable_name):
    """Convert a single file to header format"""
    try:
        with open(input_path, 'r', encoding='utf-8') as f:
            content = f.read()
    except UnicodeDecodeError:
        # Try with different encoding
        with open(input_path, 'r', encoding='latin-1') as f:
            content = f.read()
    
    # Escape content for C string
    escaped_content = escape_c_string(content)
    
    # Create header content
    header_content = f'''// Auto-generated header file from {input_path.name}
// Do not edit manually - use build_website.py to regenerate

#ifndef __{variable_name}_H__
#define __{variable_name}_H__

const char {variable_name}[] PROGMEM = "{escaped_content}";

#endif // __{variable_name}_H__
'''
    
    # Write header file
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(header_content)
    
    print(f"✓ Converted {input_path.name} -> {output_path.name}")

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
    
    # File mappings: input_file -> (output_file, variable_name)
    file_mappings = {
        "index.html": ("index_html.h", "INDEX_HTML"),
        "messung.html": ("messung_html.h", "MESSUNG_HTML"),
        "messung_webserial.html": ("messung_webserial_html.h", "MESSUNG_WEBSERIAL_HTML"),
        "justage.html": ("justage_html.h", "JUSTAGE_HTML"),
        "infos.html": ("infos_html.h", "INFOS_HTML"),
        "style.css": ("style_css.h", "STYLE_CSS")
    }
    
    converted_count = 0
    
    for input_filename, (output_filename, variable_name) in file_mappings.items():
        input_path = html_dir / input_filename
        output_path = header_dir / output_filename
        
        if input_path.exists():
            try:
                convert_file_to_header(input_path, output_path, variable_name)
                converted_count += 1
            except Exception as e:
                print(f"✗ Error converting {input_filename}: {e}")
        else:
            print(f"⚠ Skipping {input_filename} (file not found)")
    
    print(f"\n🎉 Conversion complete! {converted_count} files processed.")
    
    # Also run image conversion
    print("\nConverting images...")
    import subprocess
    result = subprocess.run([sys.executable, str(script_dir / "convert_image.py")], 
                          capture_output=True, text=True)
    print(result.stdout)
    if result.stderr:
        print(result.stderr)
    
    return True

if __name__ == "__main__":
    main()