# Firmware guide

[Project overview](../README.md) · [Engineering notes](../docs/engineering.md)

These are seven historical Arduino sketches from the project archive. Their file contents have been preserved. Only the containing directory and sketch filenames were normalized so Arduino IDE can open each sketch independently.

## Line-tracer versions

| Sketch | Kp / Ki / Kd | Base PWM | EMA alpha | Distinguishing feature |
| --- | --- | --- | --- | --- |
| [HW2 baseline](line-tracer/hw2_baseline/hw2_baseline.ino) | 30 / 1 / 40 | 180 | None | History-based error average, earlier sensing and motor conventions |
| [HW3 baseline](line-tracer/hw3_baseline/hw3_baseline.ino) | 15 / 0 / 35 | 90 | None | Earlier HW3 revision saved with an October 28 timestamp |
| [LPF](line-tracer/lpf/lpf.ino) | 58 / 0 / 105 | 170 | 0.93 | Filtered line-position estimate |
| [LPF 12s](line-tracer/lpf_12s/lpf_12s.ino) | 45 / 0 / 80 | 130 | 0.87 | Alternative lower-speed tuning |
| [LPF 10s](line-tracer/lpf_10s/lpf_10s.ino) | 135 / 0 / 235 | 250 | 0.30 | Speed ramp and an experimental line-position offset |
| [LPF 10s v2](line-tracer/lpf_10s_v2/lpf_10s_v2.ino) | 105 / 0 / 200 | 200 | 0.977 | Speed ramp with revised pin assignments |

PWM values are command values in the Arduino `analogWrite` range, not measured vehicle speed. The names containing lap times come from the original folders. They do not establish a benchmark or prove which version ran at the competition.

For code review, `lpf_10s_v2` is a useful starting point because it contains the filtered-position logic and gradual restart described in the design review. The archive does not establish it as the final competition firmware.

## Car board and connections

The line-tracer sketches use digital pins 22–36 and are consistent with an Arduino Mega-style pinout. The exact board revision was not identified in the project reports. Choose the board to match the physical controller.

All six sketches use IR inputs `{22, 24, 26, 28, 30, 32, 34, 36}`, ultrasonic echo on pin 8, and trigger on pin 9. Other wiring and polarity choices vary. The table below describes **code identifiers in `lpf_10s_v2`**, rather than assuming which physical wheel is left or right.

| Code identifier | Pin(s) in `lpf_10s_v2` |
| --- | --- |
| `IRPins` | 22, 24, 26, 28, 30, 32, 34, 36 |
| `Dir1Pin_A`, `Dir2Pin_A`, `SpeedPin_A` | 27, 29, 3 |
| `Dir1Pin_B`, `Dir2Pin_B`, `SpeedPin_B` | 25, 23, 2 |
| `EchoPin`, `TrigPin` | 8, 9 |

That version treats `LOW` as a line detection and measures an echo `HIGH` pulse. The HW2 baseline instead tests `HIGH` for the line and uses a `LOW` echo pulse. Confirm the actual sensors and driver direction against the selected sketch.

## Opening a car sketch

1. Open one `.ino` file in Arduino IDE. Keep its same-named parent directory.
2. Select the physical board and port. The car sketches use Arduino core functions without additional includes.
3. Match the selected sketch's pin constants, sensor polarity, motor direction, and supply arrangement to the hardware.
4. Review the initial PWM and gains for the current mechanism before powering the drive system. Historical values are specific to the original build.

No sketch was compiled, flashed, or rerun on hardware while this archive was prepared.

## Manipulator follower

[follower_modified.ino](manipulator/follower_modified/follower_modified.ino) adapts the course workshop follower example and includes `<Dynamixel2Arduino.h>`. Its `ARDUINO_OpenRB` branch assigns:

| Setting | Archived value |
| --- | --- |
| DYNAMIXEL port | `Serial1` |
| Command input | `Serial2`, 57,600 baud |
| Debug serial | `Serial`, 57,600 baud |
| DYNAMIXEL bus | 1,000,000 baud, protocol 2.0 |
| Joint IDs | 7, 8, 10, 12, 13, 14 |
| Gripper ID | 14 |
| Gripper mode | PWM control |
| Gripper PWM limit | ±700 in the archived control-table units |

The saved code receives a newline-terminated, comma-separated command through `Serial2`. A matching leader/controller is not included. The sketch only defines its serial aliases for `ARDUINO_OpenRB`, so other boards require port configuration.

### Review before operating

The follower is preserved as an unfinished historical revision. Its receive loop increments `sw_infos.xel_count` both while parsing and while packing a non-gripper joint. With a complete command this can exceed the six-element `sw_data`/`info_xels_sw` arrays. Resolve the indexing and packet-packing logic before using it to move hardware. The saved sketch also lacks a command-loss timeout.

For the car, `pulseIn` is called without an explicit timeout argument and the loop timing is not fixed. A zero ultrasonic reading bypasses the obstacle condition. These details matter when adapting the controller to a new build.

## Attribution

The modified follower derives from the 2025 CED course workshop material. The original workshop distribution and its leader sketch are not part of this project archive. `Dynamixel2Arduino` and its hardware ecosystem are third-party dependencies. See [RIGHTS.md](../RIGHTS.md).
