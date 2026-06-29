#!/usr/bin/env python3
"""
Demo script to show all weather types in the home info card.
Modifies app_state.c to set different weather types and generates snapshots.
"""

import subprocess
from pathlib import Path
import shutil

def modify_weather_type(weather_type):
    """Modify app_state.c to set a specific weather type."""
    app_state_file = Path("src/app/app_state.c")
    content = app_state_file.read_text()
    
    # Replace the weather_type initialization line
    old_line = "app->weather_type = 0; /* sunny */"
    new_line = f"app->weather_type = {weather_type}; /* {'sunny' if weather_type == 0 else 'cloudy' if weather_type == 1 else 'rainy' if weather_type == 2 else 'snowy'} */"
    
    content = content.replace(old_line, new_line)
    app_state_file.write_text(content)


def build_and_snapshot(weather_name):
    """Build the simulator and capture a snapshot."""
    print(f"\n{'='*60}")
    print(f"Building for {weather_name} weather...")
    print('='*60)
    
    # Build SDL simulator
    result = subprocess.run(["make", "reader_sim_sdl"], 
                          capture_output=True, text=True)
    if result.returncode != 0:
        print(f" Build failed:\n{result.stderr}")
        return False
    
    # Run simulator (it will automatically create snapshot)
    print(f"Running simulator for {weather_name} weather...")
    result = subprocess.run(["./reader_sim_sdl"], 
                          capture_output=True, text=True, timeout=5)
    
    return True


def main():
    """Generate snapshots for all weather types."""
    
    weather_types = [
        (0, "sunny"),
        (1, "cloudy"),
        (2, "rainy"),
        (3, "snowy"),
    ]
    
    output_dir = Path("out/weather_demo")
    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True)
    
    original_app_state = Path("src/app/app_state.c").read_text()
    
    try:
        for weather_type, weather_name in weather_types:
            print(f"\nGenerating {weather_name} weather snapshot...")
            
            # Modify weather type
            modify_weather_type(weather_type)
            
            # Build and run
            success = build_and_snapshot(weather_name)
            
            if success:
                # Find the latest snapshot
                snapshots_dir = Path("snapshots")
                latest_snapshot = max(snapshots_dir.iterdir(), key=lambda x: x.stat().st_mtime)
                
                # Copy home.png to our demo directory
                home_png = latest_snapshot / "home.png"
                if home_png.exists():
                    dest = output_dir / f"{weather_name}.png"
                    shutil.copy(home_png, dest)
                    print(f"✅ Saved: {dest}")
    
    finally:
        # Restore original app_state.c
        Path("src/app/app_state.c").write_text(original_app_state)
        print("\n✅ Restored original app_state.c")
    
    print(f"\n{'='*60}")
    print(f"All weather demos saved to: {output_dir}/")
    print(f"{'='*60}\n")


if __name__ == "__main__":
    main()
