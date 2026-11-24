#!/usr/bin/env python3
"""
IIT Mandi Power Line Inspection Crawler Robot - Top-Level Launcher.
Starts Dashboard 1 (Port 8002) and Dashboard 2 (Port 8003) alongside
STM32 serial communication, camera streamer, and safety supervisors.
"""

import os
import sys

# Add both the parent directory and current directory to sys.path
# so execution works seamlessly whether invoked from ROBOTF or final_robot/
CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.dirname(CURRENT_DIR)

for p in (PARENT_DIR, CURRENT_DIR):
    if p and p not in sys.path:
        sys.path.insert(0, p)

try:
    from final_robot.backend.main import main
except ModuleNotFoundError:
    from backend.main import main

if __name__ == "__main__":
    main()
