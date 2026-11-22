<h1 align="center">MyOS</h1>

<p align="center">
  <strong>A minimal, experimental operating system built from the ground up.</strong>
</p>
<p align="center">
  <code>x86_64</code> · <code>C</code> · <code>Assembly</code> · <code>QEMU</code> · <code>Limine</code>
</p>

An exploration of operating system fundamentals through a small, bootable kernel.

MyOS is an experimental operating system project designed to explore the fundamental mechanisms behind modern operating systems. Built as a standalone kernel, MyOS aims to implement essential OS components including thread management, preemptive scheduling, synchronization, memory allocation, a virtual filesystem, and basic access control.

The goal is not to build another application running on an existing operating system, but to build an operating system that boots and runs independently.

---

## Overview

MyOS is developed as a minimal x86_64 operating system, initially targeting QEMU for development and testing, with bootable ISO and USB support as future milestones.

The project emphasizes three principles:
- **Control** — understanding and controlling the fundamental mechanisms of the system.
- **Clarity** — making kernel behavior observable and explainable.
- **Minimalism** — implementing essential features without unnecessary complexity.

---

## Features

| Component | Description | Status |
| :--- | :--- | :--- |
| **Bootloader** | Kernel loading through Limine | Planned |
| **Internal Shell** | Interactive kernel CLI | Planned |
| **Thread Management** | Kernel thread creation and state management | Planned |
| **CPU Scheduler** | Preemptive Round Robin scheduling | Planned |
| **Synchronization** | Mutex-based synchronization | Planned |
| **Deadlock Demo** | Circular wait demonstration and prevention | Planned |
| **Memory Management** | Simple dynamic heap allocator | Planned |
| **RAM Filesystem** | Basic file CRUD operations | Planned |
| **Access Control** | Basic resource permission system | Planned |
| **QEMU Support** | Virtual machine testing | Planned |
| **Bootable ISO** | Standalone bootable image | Planned |
| **USB Boot** | Boot on compatible physical hardware | Planned |

---

## Architecture

```text
                    ┌─────────────────────────┐
                    │       MyOS Shell        │
                    │   CLI & Command Layer   │
                    └────────────┬────────────┘
                                 │
        ┌────────────────────────┼────────────────────────┐
        │                        │                        │
 ┌──────▼────────┐      ┌────────▼────────┐     ┌────────▼────────┐
 │ Thread Manager│      │  CPU Scheduler  │     │ Memory Manager  │
 └───────────────┘      └─────────────────┘     └─────────────────┘
        │                        │                        │
        ├────────────────────────┼────────────────────────┤
        │                        │                        │
 ┌──────▼────────┐      ┌────────▼────────┐     ┌────────▼────────┐
 │Synchronization│      │      RAMFS      │     │ Access Control  │
 └───────────────┘      └─────────────────┘     └─────────────────┘
                                 │
                    ┌────────────▼────────────┐
                    │ Interrupts & Hardware   │
                    │ Abstraction             │
                    └─────────────────────────┘
