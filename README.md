# Modular Embedded Control System

This project implements a modular embedded system on the TM4C123GH6PM microcontroller, integrating multiple display devices, user input, and a digital temperature sensor.

## Features
- Multi-mode user interface using DIP switches
- Seven-segment numeric display (decimal and hexadecimal modes)
- LCD static and scrolling text display
- DS1620 digital thermometer via bit-banged GPIO serial protocol
- Mutual exclusion between display modes and sensor override mode

## Architecture
- TM4C123GH6PM microcontroller
- PCF8574 I/O expanders over I²C for displays and user input
- GPIO-based bit-banged serial communication for DS1620

## Project Resources
- 📄 Project Overview: `docs/Project_Overview.pdf`
- 📄 Project Code: `docs/DS1620_Technical_Report.pdf`
- 🎥 Demo Video: `demo/system_demo.mp4`
