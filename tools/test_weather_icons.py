#!/usr/bin/env python3
"""
Test script to verify weather icons are properly generated and can be loaded.
"""

from PIL import Image
from pathlib import Path
import sys

def test_weather_icons():
    """Verify all weather icon PNG files exist and have correct properties."""
    
    icons_dir = Path("assets/icons")
    weather_icons = [
        "weather_sunny.png",
        "weather_cloudy.png", 
        "weather_rainy.png",
        "weather_snowy.png"
    ]
    
    print("Testing weather icon files...\n")
    
    all_passed = True
    
    for icon_name in weather_icons:
        icon_path = icons_dir / icon_name
        
        if not icon_path.exists():
            print(f"❌ FAIL: {icon_name} does not exist")
            all_passed = False
            continue
        
        try:
            img = Image.open(icon_path)
            
            # Check dimensions
            if img.size != (64, 64):
                print(f"❌ FAIL: {icon_name} has wrong size {img.size}, expected (64, 64)")
                all_passed = False
                continue
            
            # Check mode
            if img.mode != "RGBA":
                print(f"️  WARNING: {icon_name} is in {img.mode} mode, expected RGBA")
                # Convert to RGBA for consistency
                img = img.convert("RGBA")
            
            # Check that there's actual content (not completely transparent)
            pixels = list(img.getdata())
            opaque_pixels = sum(1 for p in pixels if p[3] > 0)
            
            if opaque_pixels == 0:
                print(f"❌ FAIL: {icon_name} is completely transparent")
                all_passed = False
                continue
            
            # Calculate percentage of opaque pixels
            total_pixels = len(pixels)
            opacity_ratio = opaque_pixels / total_pixels
            
            if opacity_ratio < 0.05:
                print(f"⚠️  WARNING: {icon_name} has very few opaque pixels ({opacity_ratio*100:.1f}%)")
            
            print(f"✅ PASS: {icon_name}")
            print(f"   Size: {img.size}, Mode: {img.mode}")
            print(f"   Opaque pixels: {opaque_pixels}/{total_pixels} ({opacity_ratio*100:.1f}%)")
            
        except Exception as e:
            print(f"❌ FAIL: {icon_name} - {str(e)}")
            all_passed = False
    
    print("\n" + "="*60)
    if all_passed:
        print("✅ All weather icon tests passed!")
        return 0
    else:
        print("❌ Some tests failed!")
        return 1


if __name__ == "__main__":
    sys.exit(test_weather_icons())
