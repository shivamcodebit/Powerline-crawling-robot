#!/usr/bin/env python3
"""
IIT Mandi Power Line Inspection Crawler Robot - Root Launcher.
Forwards to final_robot/run_robot.py.
"""

import os
import sys

WORKSPACE_ROOT = os.path.dirname(os.path.abspath(__file__))
if WORKSPACE_ROOT not in sys.path:
    sys.path.insert(0, WORKSPACE_ROOT)

from final_robot.backend.main import main

if __name__ == "__main__":
    main()
