#!/usr/bin/env python3
"""
Convert Bootstrap CSS and JS files to C++ header files
"""
import re

def escape_c_string(content):
    """Escape content for use in C++ string literal"""
    # Replace backslashes first (they need to be escaped)
    content = content.replace('\\', '\\\\')
    # Replace quotes
    content = content.replace('"', '\\"')
    # Replace newlines with \n
    content = content.replace('\n', '\\n')
    return content

def create_header_file(input_file, output_file, var_name):
    """Create a C++ header file from input file"""
    print(f"Converting {input_file} to {output_file}...")
    
    try:
        with open(input_file, 'r', encoding='utf-8') as f:
            content = f.read()
    except UnicodeDecodeError:
        # Try with latin-1 if utf-8 fails
        with open(input_file, 'r', encoding='latin-1') as f:
            content = f.read()
    
    # Escape for C++
    escaped_content = escape_c_string(content)
    
    # Create header content
    header_content = f'''#ifndef {var_name}_H
#define {var_name}_H

const char {var_name}[] PROGMEM = R"rawliteral({content})rawliteral";

#endif // {var_name}_H
'''
    
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(header_content)
    
    print(f"Successfully created {output_file}")

# Convert Bootstrap CSS
create_header_file('bootstrap.min.css', 'src/website/bootstrap_css.h', 'BOOTSTRAP_CSS')

# Convert Bootstrap JS
create_header_file('bootstrap.bundle.min.js', 'src/website/bootstrap_js.h', 'BOOTSTRAP_JS')

print("Bootstrap conversion completed!")
