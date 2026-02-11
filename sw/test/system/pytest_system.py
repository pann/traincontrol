"""
Automated system tests for traincontrol motor_control module.

Sends serial commands to the test firmware REPL and verifies text responses.
Does NOT verify oscilloscope timing — that remains a manual step.

Prerequisites:
    pip install pytest-embedded pytest-embedded-serial-esp pytest-embedded-idf

Run:
    pytest --target esp32s3 pytest_system.py -v
"""

import pytest


@pytest.mark.esp32s3
@pytest.mark.parametrize('speed', [0, 1, 500, 1000])
def test_speed_set_and_readback(dut, speed):
    """Set speed and verify readback via status command."""
    dut.write(f'speed {speed}')
    dut.expect_exact(f'OK speed={speed}')

    dut.write('status')
    dut.expect(f'speed={speed}')


@pytest.mark.esp32s3
def test_direction_forward(dut):
    """Set direction forward and verify."""
    dut.write('dir fwd')
    dut.expect_exact('OK dir=fwd')

    dut.write('status')
    dut.expect('dir=fwd')


@pytest.mark.esp32s3
def test_direction_reverse(dut):
    """Set direction reverse and verify."""
    dut.write('dir rev')
    dut.expect_exact('OK dir=rev')

    dut.write('status')
    dut.expect('dir=rev')


@pytest.mark.esp32s3
def test_enable_disable(dut):
    """Enable then disable, verify speed resets to 0."""
    dut.write('enable 1')
    dut.expect_exact('OK enable=1')

    dut.write('speed 750')
    dut.expect_exact('OK speed=750')

    dut.write('enable 0')
    dut.expect_exact('OK enable=0')

    dut.write('status')
    dut.expect('enabled=0')
    dut.expect('speed=0')


@pytest.mark.esp32s3
def test_emergency_stop(dut):
    """Enable motor, set speed, then estop — verify everything resets."""
    dut.write('enable 1')
    dut.expect_exact('OK enable=1')

    dut.write('speed 500')
    dut.expect_exact('OK speed=500')

    dut.write('estop')
    dut.expect_exact('OK emergency_stop')

    dut.write('status')
    dut.expect('enabled=0')
    dut.expect('speed=0')
