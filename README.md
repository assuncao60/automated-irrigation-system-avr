# Automated Irrigation System (C & Assembly)

An embedded systems project that models and implements an automated irrigation controller, combining high-level state control in C with low-level hardware execution in AVR Assembly.

*Note: The comprehensive technical engineering report included in this repository is written in Portuguese.*

## Key Features

* **Hybrid Architecture:** Developed using an optimized mix of **C** for high-level state control and **AVR Assembly** for precise low-level driver handling and hardware execution.
* **Finite State Machine (FSM):** Structured around a robust FSM implemented in C to cycle through safe irrigation states based on environmental thresholds.
* **Hardware Interfacing:** Direct register manipulation in Assembly to handle timers, interrupts, and I/O pins, simulating sensors and valves without operating system overhead.

## Technologies Used

* **Languages:** C and AVR Assembly
* **Target Hardware:** Atmel AVR Microcontroller Architecture (ATmega 328P)
* **Core Concepts:** Finite State Machines, Hardware Timers, Interrupt Service Routines (ISR), and Bitwise Register Manipulation.

## Repository Structure

* `main.c`: Core application logic and Finite State Machine execution.
* `controlo_nivel.s`: Low-level Assembly routines for direct hardware and timer control.
* `/docs`: Contains the full technical engineering report (PDF) detailing state diagrams, hardware schematics, and memory mapping.
