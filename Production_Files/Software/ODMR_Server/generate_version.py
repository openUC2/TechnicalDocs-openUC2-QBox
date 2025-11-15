#!/usr/bin/env python3
"""
Generate version information header file for ODMR Server
Creates a header file with build date, time, and git commit hash
"""

import subprocess
from datetime import datetime
from pathlib import Path

def get_git_hash():
    """Get short git commit hash"""
    try:
        result = subprocess.run(
            ['git', 'rev-parse', '--short', 'HEAD'],
            capture_output=True,
            text=True,
            check=True
        )
        return result.stdout.strip()
    except:
        return "unknown"

def get_git_branch():
    """Get current git branch name"""
    try:
        result = subprocess.run(
            ['git', 'rev-parse', '--abbrev-ref', 'HEAD'],
            capture_output=True,
            text=True,
            check=True
        )
        return result.stdout.strip()
    except:
        return "unknown"

def generate_version_header(output_path):
    """Generate version header file"""
    now = datetime.now()
    build_date = now.strftime("%Y-%m-%d")
    build_time = now.strftime("%H:%M:%S")
    build_timestamp = now.strftime("%Y%m%d%H%M%S")
    git_hash = get_git_hash()
    git_branch = get_git_branch()
    
    header_content = f'''// Auto-generated version information
// Do not edit manually - regenerated on each build

#ifndef __VERSION_INFO_H__
#define __VERSION_INFO_H__

#define FIRMWARE_VERSION "1.0.0"
#define BUILD_DATE "{build_date}"
#define BUILD_TIME "{build_time}"
#define BUILD_TIMESTAMP "{build_timestamp}"
#define GIT_HASH "{git_hash}"
#define GIT_BRANCH "{git_branch}"

// Combined version string
#define VERSION_STRING "v" FIRMWARE_VERSION " (" BUILD_DATE " " BUILD_TIME " - " GIT_HASH ")"

#endif // __VERSION_INFO_H__
'''
    
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(header_content)
    
    print(f"✓ Generated version header: {output_path}")
    print(f"  Version: {FIRMWARE_VERSION}")
    print(f"  Build Date: {build_date} {build_time}")
    print(f"  Git Hash: {git_hash}")
    print(f"  Git Branch: {git_branch}")

if __name__ == "__main__":
    script_dir = Path(__file__).parent
    output_file = script_dir / "src" / "version_info.h"
    
    # Ensure output directory exists
    output_file.parent.mkdir(parents=True, exist_ok=True)
    
    # Define version here for use in print
    FIRMWARE_VERSION = "1.0.0"
    
    generate_version_header(output_file)
