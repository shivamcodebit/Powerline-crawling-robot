"""
Unit tests for Safety Interlocks, Deadbands, and E-Stop Latching.
"""

import pytest
from final_robot.backend.motor_controller import MotorController
from final_robot.backend.telemetry_parser import TelemetryStore
from final_robot.backend.serial_manager import SerialManager
from final_robot.tests.mock_stm32 import MockSTM32Controller

class DummySerial:
    def __init__(self):
        self.sent_commands = []
        self.is_connected = True
        self.port_name = "MOCK"

    def send_command(self, cmd):
        self.sent_commands.append(cmd)
        return True

def test_motor_controller_deadband_and_clamping():
    dummy = DummySerial()
    ctrl = MotorController(dummy)

    # Deadband check (default +/- 2%)
    ctrl.set_crawler_speed(1)
    assert ctrl.crawler_speed == 0

    ctrl.set_crawler_speed(-2)
    assert ctrl.crawler_speed == 0

    # Normal speed
    ctrl.set_crawler_speed(50)
    assert ctrl.crawler_speed == 50

    # Clamping
    ctrl.set_crawler_speed(150)
    assert ctrl.crawler_speed == 100

    ctrl.set_crawler_speed(-150)
    assert ctrl.crawler_speed == -100

def test_independent_gripper_control():
    dummy = DummySerial()
    ctrl = MotorController(dummy)

    ctrl.set_gripper_motor_a(40)
    assert ctrl.gripper_a_speed == 40
    assert ctrl.gripper_b_speed == 0  # Motor B must remain strictly 0

    ctrl.set_gripper_motor_b(-60)
    assert ctrl.gripper_a_speed == 40
    assert ctrl.gripper_b_speed == -60

    ctrl.stop_gripper_a()
    assert ctrl.gripper_a_speed == 0
    assert ctrl.gripper_b_speed == -60

def test_mock_stm32_safety_state_machine():
    out_lines = []
    mock = MockSTM32Controller(lambda line: out_lines.append(line))

    # 1. Initially DISARMED
    assert mock.state == "DISARMED"

    # Motion while disarmed should be rejected
    mock.receive_command_line('{"type":"crawler","speed":50}')
    assert mock.crawl_a == 0

    # 2. Arm robot while controls are neutral (0)
    mock.receive_command_line('{"type":"arm","active":true}')
    assert mock.state == "ARMED"

    # Now movement commands are accepted
    mock.receive_command_line('{"type":"crawler","speed":75}')
    assert mock.crawl_a == 75

    # 3. Emergency Stop Trigger
    mock.receive_command_line('{"type":"estop","active":true}')
    assert mock.state == "ESTOP_LATCHED"
    assert mock.crawl_a == 0

    # 4. Normal commands while E-stop is latched must be strictly rejected
    mock.receive_command_line('{"type":"crawler","speed":30}')
    assert mock.crawl_a == 0
    assert mock.state == "ESTOP_LATCHED"

    # Attempting to arm directly from E-Stop must fail
    mock.receive_command_line('{"type":"arm","active":true}')
    assert mock.state == "ESTOP_LATCHED"

    # 5. Clear E-Stop explicitly
    mock.receive_command_line('{"type":"reset_estop"}')
    assert mock.state == "DISARMED" # Returns to DISARMED, not directly armed!

    # 6. Cannot arm if non-zero command is present
    mock.crawl_a = 50 # Simulated non-neutral
    mock.receive_command_line('{"type":"arm","active":true}')
    assert mock.state == "DISARMED" # Arming rejected!

    mock.crawl_a = 0 # Neutralize
    mock.receive_command_line('{"type":"arm","active":true}')
    assert mock.state == "ARMED" # Now armed successfully!
