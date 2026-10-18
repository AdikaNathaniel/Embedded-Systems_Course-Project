# Intruder Detection System Using an LDR Sensor

An Arduino Uno security system that watches the light level in a room and raises an alarm when it detects the kind of light change an intruder causes, such as a torch beam sweeping across a dark space.

Built for **CPEN 403 – Embedded Systems** at the **University of Ghana**, and designed and tested in **Proteus**.

## The problem

Break-ins and unauthorized access are a real threat to homes, offices, storerooms and restricted areas, especially at night when spaces are dark and unattended. Intruders often need light to find their way, typically from a torch or by switching on a light. That sudden, unexpected change in brightness is a clear sign that someone is there who shouldn't be.

Common security options don't always fit:

- **Professional alarm and CCTV systems** are expensive to buy, install and maintain, which puts them out of reach for many small businesses and households.
- **Motion and infrared sensors** can miss an intruder who moves slowly or stays outside their field of view, and are prone to false alarms from pets, heat sources or air movement.
- **A simple light sensor on its own** reacts to every flicker and passing shadow, so it raises too many false alarms to be trusted.

This project addresses that gap with a **low-cost, reliable detector** built from an LDR and an Arduino. It continuously monitors light levels, uses digital filtering and hysteresis to tell an intruder's light apart from ordinary noise and flicker, and alerts people straight away with sound, light and an on-screen message. It also keeps a permanent count of detections so owners can see how often incidents occur, keeps itself running with a watchdog timer, and saves power so it suits long-term, unattended use.

## How it works

1. **Sense.** A light-dependent resistor (LDR) in a voltage divider feeds the Arduino's analog input `A0`.
2. **Sample.** Timer1 fires an interrupt about 100 times a second. The interrupt service routine reads the sensor and scales it to the range 0.0–1.0.
3. **Filter.** Each sample goes through a second-order low-pass IIR (biquad) filter, implemented in Direct Form II, with a cutoff of about 3 Hz. This keeps the slow change caused by an intruder's light and suppresses faster flicker and noise. A 6 Hz noise test in Proteus showed it being filtered out.
4. **Detect.** Hysteresis thresholds stop the alarm flickering on and off around a single value. The system enters the detected state above **0.50** and only leaves it below **0.45**.
5. **Alert.** On each new detection the system enters alarm mode for **5 seconds**:
   - the buzzer plays a tone that sweeps smoothly between 200 and 600 Hz
   - the red LED blinks every half second
   - the LCD shows `ALARM: DETECTED`
6. **Log.** Every detection increments a counter that is saved to **EEPROM**, so the total survives power cuts and resets.
7. **Stay reliable and efficient.** A **watchdog timer** resets the board if the code ever hangs for more than 2 seconds, and the CPU drops into **idle sleep** between samples to save power.

In normal operation the green LED is on and the LCD shows `Status: Normal`. Filtered sensor values, detection events and the stored event count are also printed to the serial monitor at 9600 baud.

## Hardware

| Component | Purpose | Arduino pin |
|---|---|---|
| Arduino Uno (ATmega328P, 16 MHz) | Sampling, filtering, detection and control | — |
| LDR + resistor (voltage divider) | Light sensor | A0 |
| 16×2 LCD (LM016L / HD44780) | Status display | RS 13, E 12, D4–D7 on 11, 10, 9, 8 |
| Buzzer | Audible alarm (`tone()`) | 6 |
| Red LED + resistor | Alarm indicator | 3 |
| Green LED + resistor | Normal-operation indicator | 5 |
| 555 timer (simulation only) | Generates the ~3 Hz test signal an LDR would see in real life, which Proteus can't otherwise produce | — |
| 5 V regulated supply | Power | — |

## Embedded techniques used

- **Timer interrupts:** Timer1 in CTC mode with a 1024 prescaler and `OCR1A = 156` gives a sampling rate of about 100 Hz.
- **Digital signal processing:** a fixed-coefficient biquad filter that runs one sample at a time inside the interrupt.
- **Hysteresis:** two-threshold switching to avoid false triggering.
- **Non-volatile storage:** `EEPROM.get()` and `EEPROM.put()` keep the event counter between power cycles.
- **Watchdog timer:** `wdt_enable(WDTO_2S)`, reset on every pass of the main loop.
- **Low-power mode:** `SLEEP_MODE_IDLE` between interrupts.
- **Non-blocking timing:** the alarm pattern is driven by `millis()`, not `delay()`.

## Repository contents

```
├── LDR_System- Project_Report.pdf      Full project report: design, calculations, schematics, simulations
├── LIGHT-DETECTION-SYSTEM.pptx         Presentation slides
└── SecurityDetection_Code-Files/
    ├── SecurityDetectionSystem.pdsprj  Proteus schematic and simulation project
    └── SecurityDetectionArduinoCode/
        └── SecurityDetection/
            ├── SecurityDetection.ino   Arduino sketch
            └── build/arduino.avr.uno/  Compiled firmware (.hex) for loading into Proteus
```

## Running it

### In Proteus
1. Open `SecurityDetection_Code-Files/SecurityDetectionSystem.pdsprj`.
2. Double-click the Arduino and set its program file to `SecurityDetection.ino.hex` from the `build/arduino.avr.uno/` folder.
3. Run the simulation, then change the LDR's light level, or the 555 test signal, to trigger the alarm.

### On real hardware
1. Wire the components as shown in the table above.
2. Open `SecurityDetection.ino` in the Arduino IDE. It only uses libraries that come with the IDE: `LiquidCrystal`, `EEPROM` and the AVR headers.
3. Select **Arduino Uno**, then upload.
4. Open the serial monitor at 9600 baud to watch the filtered readings.

## Notes and possible improvements

- **Push-button feature isn't in this version of the sketch.** The report describes a button on pin 7 for showing the stored detection count and time on the LCD. The sketch in this repository logs the count to EEPROM and prints it over serial, but doesn't yet read the button or store detection times.
- **Thresholds need tuning per room.** The 0.50 and 0.45 thresholds suit the simulation; a real installation would set them for its own lighting.
- **Pin 3 shares Timer2 with `tone()`.** That's fine here because the red LED only uses `digitalWrite()`. Fading it with PWM would need it moved to another pin.
- **Proteus can't mix frequencies well.** It struggles to simulate several light frequencies at once, so the 6 Hz noise test can make the LDR look unresponsive. This is a limit of the simulator, not of the filter, and is explained in the report.

## Course details

- **Course:** CPEN 403 – Embedded Systems, University of Ghana
- **Course instructor:** Professor Godfrey Mills
- **Lab instructor:** Kelvin Prince Boateng
