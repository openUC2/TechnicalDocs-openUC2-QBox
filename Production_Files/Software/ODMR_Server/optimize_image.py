#!/usr/bin/env python3
"""
Image Optimizer for ODMR Server
Optimizes images to reduce file size while maintaining quality
"""

from PIL import Image
import os
from pathlib import Path

def optimize_image(input_path, output_path, max_width=600, quality=85):
    """Optimize an image by resizing and compressing"""
    # Load the image
    img = Image.open(input_path)
    print(f'Original size: {img.size}')
    print(f'Original mode: {img.mode}')
    
    # Calculate new dimensions
    aspect_ratio = img.height / img.width
    new_width = min(max_width, img.width)  # Don't upscale
    new_height = int(new_width * aspect_ratio)
    
    # Resize image if needed
    if new_width < img.width:
        img_resized = img.resize((new_width, new_height), Image.Resampling.LANCZOS)
    else:
        img_resized = img
    
    # Convert to RGB if it has alpha channel (to further reduce size)
    if img_resized.mode == 'RGBA':
        # Create a white background
        background = Image.new('RGB', img_resized.size, (255, 255, 255))
        background.paste(img_resized, mask=img_resized.split()[3])  # 3 is the alpha channel
        img_resized = background
    
    # Save optimized version
    img_resized.save(output_path, optimize=True, quality=quality)
    print(f'Optimized size: {img_resized.size}')
    
    # Check file sizes
    orig_size = os.path.getsize(input_path)
    opt_size = os.path.getsize(output_path)
    print(f'Original file: {orig_size:,} bytes')
    print(f'Optimized file: {opt_size:,} bytes')
    print(f'Reduction: {(1 - opt_size/orig_size)*100:.1f}%')

def main():
    """Main optimization function"""
    script_dir = Path(__file__).parent
    html_dir = script_dir / "src" / "website_html"
    
    # Optimize NVGitter.png
    input_file = html_dir / "NVGitter.png"
    output_file = html_dir / "NVGitter_optimized.png"
    
    if input_file.exists():
        print(f"Optimizing {input_file.name}...")
        optimize_image(input_file, output_file)
        print(f"\n✓ Optimization complete! Saved to {output_file.name}")
    else:
        print(f"⚠ Image file not found: {input_file}")

if __name__ == "__main__":
    main()
