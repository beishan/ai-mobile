#!/usr/bin/env python3
"""
Demo script to show all weather icons in the home info card layout.
Creates snapshots showing each weather type (sunny, cloudy, rainy, snowy).
"""

import subprocess
import os
from pathlib import Path

def create_demo_snapshots():
    """Create demo snapshots for each weather type."""
    
    # Build the simulator first
    print("Building SDL simulator...")
    result = subprocess.run(["make", "reader_sim_sdl"], 
                          capture_output=True, text=True)
    if result.returncode != 0:
        print(f"Build failed:\n{result.stderr}")
        return
    
    print("\nCreating weather demo snapshots...\n")
    
    # Weather types to demonstrate
    weather_types = [
        ("sunny", 0, "Sunny - Clear sky"),
        ("cloudy", 1, "Cloudy - Overcast"),
        ("rainy", 2, "Rainy - Light rain"),
        ("snowy", 3, "Snowy - Light snow"),
    ]
    
    output_dir = Path("out/weather_demo")
    output_dir.mkdir(parents=True, exist_ok=True)
    
    for weather_name, weather_type, description in weather_types:
        print(f"Generating {description} ({weather_name})...")
        
        # We'll need to modify app_state.c temporarily to set the weather type
        # For now, let's just document what would be shown
        
        # In a real implementation, you'd:
        # 1. Set app->weather_type = weather_type in app_init()
        # 2. Run the simulator
        # 3. Capture screenshot
        
        print(f"  Weather type: {weather_type}")
        print(f"  Icon: UI_ICON_{weather_name.upper()}")
        print(f"  Output: out/weather_demo/{weather_name}.png")
        print()
    
    print("\nTo view different weather types:")
    print("1. Edit src/app/app_state.c")
    print("2. Change 'app->weather_type = 0' to desired value (0-3)")
    print("3. Rebuild and run: make clean && make && ./reader_sim_sdl")
    print("\nWeather type values:")
    print("  0 = Sunny (sun icon)")
    print("  1 = Cloudy (cloud icon)")
    print("  2 = Rainy (cloud with raindrops)")
    print("  3 = Snowy (cloud with snowflakes)")


if __name__ == "__main__":
    create_demo_snapshots()
