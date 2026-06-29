#!/usr/bin/env python3
"""
Create weather icon PNG files for the AI Mobile E-Ink Reader.

Generates 4 weather icons: sunny, cloudy, rainy, snowy
Each icon is 64x64 pixels with transparent background and black drawing.
"""

from PIL import Image, ImageDraw
import math

ICON_SIZE = 64
OUTPUT_DIR = "assets/icons"


def create_sunny_icon():
    """Create a sun icon - circle with rays."""
    img = Image.new("RGBA", (ICON_SIZE, ICON_SIZE), (255, 255, 255, 0))
    draw = ImageDraw.Draw(img)
    
    center = ICON_SIZE // 2
    radius = 14
    
    # Draw sun circle (outline)
    draw.ellipse(
        (center - radius, center - radius, center + radius, center + radius),
        outline=(0, 0, 0, 255),
        width=2
    )
    
    # Draw 8 rays around the sun
    ray_length = 8
    for i in range(8):
        angle = i * 45  # degrees
        rad = math.radians(angle)
        
        # Start point (on circle edge)
        x1 = center + int((radius + 3) * math.cos(rad))
        y1 = center + int((radius + 3) * math.sin(rad))
        
        # End point
        x2 = center + int((radius + ray_length) * math.cos(rad))
        y2 = center + int((radius + ray_length) * math.sin(rad))
        
        draw.line((x1, y1, x2, y2), fill=(0, 0, 0, 255), width=2)
    
    return img


def create_cloudy_icon():
    """Create a cloud icon - fluffy cloud shape."""
    img = Image.new("RGBA", (ICON_SIZE, ICON_SIZE), (255, 255, 255, 0))
    draw = ImageDraw.Draw(img)
    
    center_x = ICON_SIZE // 2
    center_y = ICON_SIZE // 2 + 4
    
    # Draw cloud using multiple overlapping circles
    # Main body circles
    draw.ellipse((center_x - 20, center_y - 8, center_x - 4, center_y + 8), 
                 outline=(0, 0, 0, 255), width=2)
    draw.ellipse((center_x - 12, center_y - 12, center_x + 4, center_y + 4), 
                 outline=(0, 0, 0, 255), width=2)
    draw.ellipse((center_x + 4, center_y - 10, center_x + 20, center_y + 6), 
                 outline=(0, 0, 0, 255), width=2)
    
    # Bottom line to connect
    draw.line((center_x - 20, center_y + 6, center_x + 20, center_y + 6), 
              fill=(0, 0, 0, 255), width=2)
    
    return img


def create_rainy_icon():
    """Create a rain icon - cloud with raindrops."""
    img = Image.new("RGBA", (ICON_SIZE, ICON_SIZE), (255, 255, 255, 0))
    draw = ImageDraw.Draw(img)
    
    center_x = ICON_SIZE // 2
    cloud_y = ICON_SIZE // 2 - 8
    
    # Draw cloud (smaller, at top)
    draw.ellipse((center_x - 16, cloud_y - 6, center_x - 2, cloud_y + 6), 
                 outline=(0, 0, 0, 255), width=2)
    draw.ellipse((center_x - 10, cloud_y - 9, center_x + 2, cloud_y + 3), 
                 outline=(0, 0, 0, 255), width=2)
    draw.ellipse((center_x + 2, cloud_y - 8, center_x + 16, cloud_y + 4), 
                 outline=(0, 0, 0, 255), width=2)
    draw.line((center_x - 16, cloud_y + 4, center_x + 16, cloud_y + 4), 
              fill=(0, 0, 0, 255), width=2)
    
    # Draw raindrops below cloud
    rain_y_start = cloud_y + 10
    for i in range(3):
        x_offset = (i - 1) * 10
        # Diagonal raindrop lines
        draw.line((center_x + x_offset - 2, rain_y_start, 
                   center_x + x_offset - 6, rain_y_start + 10), 
                  fill=(0, 0, 0, 255), width=2)
    
    return img


def create_snowy_icon():
    """Create a snow icon - cloud with snowflakes."""
    img = Image.new("RGBA", (ICON_SIZE, ICON_SIZE), (255, 255, 255, 0))
    draw = ImageDraw.Draw(img)
    
    center_x = ICON_SIZE // 2
    cloud_y = ICON_SIZE // 2 - 8
    
    # Draw cloud (same as rainy)
    draw.ellipse((center_x - 16, cloud_y - 6, center_x - 2, cloud_y + 6), 
                 outline=(0, 0, 0, 255), width=2)
    draw.ellipse((center_x - 10, cloud_y - 9, center_x + 2, cloud_y + 3), 
                 outline=(0, 0, 0, 255), width=2)
    draw.ellipse((center_x + 2, cloud_y - 8, center_x + 16, cloud_y + 4), 
                 outline=(0, 0, 0, 255), width=2)
    draw.line((center_x - 16, cloud_y + 4, center_x + 16, cloud_y + 4), 
              fill=(0, 0, 0, 255), width=2)
    
    # Draw snowflakes below cloud
    snow_y_start = cloud_y + 12
    for i in range(3):
        x_offset = (i - 1) * 12
        cx = center_x + x_offset
        cy = snow_y_start + (i % 2) * 6
        
        # Small asterisk-like snowflake
        size = 3
        # Vertical line
        draw.line((cx, cy - size, cx, cy + size), 
                  fill=(0, 0, 0, 255), width=1)
        # Horizontal line
        draw.line((cx - size, cy, cx + size, cy), 
                  fill=(0, 0, 0, 255), width=1)
        # Diagonal lines
        draw.line((cx - size, cy - size, cx + size, cy + size), 
                  fill=(0, 0, 0, 255), width=1)
        draw.line((cx - size, cy + size, cx + size, cy - size), 
                  fill=(0, 0, 0, 255), width=1)
    
    return img


def main():
    import os
    
    # Create output directory if it doesn't exist
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    
    # Generate all weather icons
    icons = {
        "weather_sunny": create_sunny_icon(),
        "weather_cloudy": create_cloudy_icon(),
        "weather_rainy": create_rainy_icon(),
        "weather_snowy": create_snowy_icon(),
    }
    
    for name, icon in icons.items():
        filepath = f"{OUTPUT_DIR}/{name}.png"
        icon.save(filepath)
        print(f"Generated: {filepath}")
    
    print(f"\nAll weather icons created in {OUTPUT_DIR}/")


if __name__ == "__main__":
    main()
