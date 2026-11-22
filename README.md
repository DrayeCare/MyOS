<div align="center">

<h1>MyOS</h1>

<p>
  <strong>A minimal, experimental operating system built from the ground up.</strong>
</p>

<p>
  <img src="https://img.shields.io/badge/Architecture-x86__64-0078D6?style=flat-square" alt="x86_64" />
  <img src="https://img.shields.io/badge/Language-C-A8B9CC?style=flat-square&logo=c&logoColor=black" alt="C" />
  <img src="https://img.shields.io/badge/Low--Level-Assembly-654FF0?style=flat-square" alt="Assembly" />
  <img src="https://img.shields.io/badge/Emulator-QEMU-FF6600?style=flat-square" alt="QEMU" />
  <img src="https://img.shields.io/badge/Bootloader-Limine-2EA44F?style=flat-square" alt="Limine" />
</p>

<p>
  <code>x86_64</code> · <code>C</code> · <code>Assembly</code> · <code>QEMU</code> · <code>Limine</code>
</p>

</div>

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
```

---

## System Components

- **Boot & Initialization** — loads the kernel and initializes essential subsystems.
- **Thread Manager** — manages kernel threads and their lifecycle.
- **Scheduler** — implements preemptive CPU scheduling.
- **Synchronization** — protects shared data and demonstrates concurrency mechanisms.
- **Memory Manager** — manages dynamic memory allocation and deallocation.
- **RAMFS** — provides basic in-memory file operations.
- **Access Control** — restricts access to protected kernel resources.
- **Shell** — provides an interactive interface for system inspection and testing.

---

## MyOS Shell

MyOS will provide an internal command-line interface for interacting with and inspecting the kernel.

```text
 __  __ __   __
|  \/  |\ \ / /
| |\/| | \ V /
| |  | |  | |
|_|  |_|  |_|

Minimal Interactive Operating System

MyOS is initializing...

myos@kernel:~$ help

  help             Show available commands
  sysinfo          Display kernel information
  thread list      List kernel threads
  thread demo      Run thread demonstration
  sched test       Test scheduler
  sync test        Test synchronization
  deadlock test    Demonstrate deadlock
  mem status       Display memory usage
  mem test         Test memory allocation
  fs ls            List filesystem entries
  security test    Test access permissions

myos@kernel:~$
```

<sub>The shell interface above is a conceptual preview of the planned interface.</sub>

---

## Project Requirements

MyOS is developed as an academic operating system project covering core operating system concepts.

| Topic | Planned Implementation |
| :--- | :--- |
| **Bootstrapping** | Bootloader loads and executes kernel |
| **Processes & Threads** | Thread control structures and lifecycle |
| **CPU Scheduling** | Preemptive Round Robin |
| **Synchronization** | Mutex and shared counter |
| **Deadlock** | Deadlock demonstration and prevention |
| **Memory Management** | Dynamic heap allocation and deallocation |
| **Virtual Filesystem** | RAM-based file storage and CRUD |
| **Security** | Basic permission-based access control |

---

## Technology Stack

| Technology | Purpose |
| :--- | :--- |
| **C** | Kernel implementation |
| **x86_64 Assembly** | Low-level operations and context switching |
| **Limine** | Bootloader |
| **QEMU** | Virtual hardware and kernel testing |
| **Make** | Build automation |
| **GDB** | Kernel debugging |

---

## Development Roadmap

### Phase I — Kernel Foundation
- [ ] Establish development environment
- [ ] Configure bootloader
- [ ] Boot kernel in QEMU
- [ ] Initialize serial output
- [ ] Implement shell
- [ ] Implement basic shell commands

### Phase II — Threading & Scheduling
- [ ] Define thread control block
- [ ] Implement thread creation
- [ ] Implement thread states
- [ ] Implement context switching
- [ ] Implement timer interrupts
- [ ] Implement Round Robin scheduler
- [ ] Verify preemptive scheduling

### Phase III — Synchronization
- [ ] Implement mutex
- [ ] Demonstrate race condition
- [ ] Demonstrate synchronized counter
- [ ] Implement deadlock demonstration
- [ ] Implement deadlock prevention strategy

### Phase IV — Memory Management
- [ ] Implement kernel heap
- [ ] Implement dynamic allocation
- [ ] Implement deallocation
- [ ] Track memory usage
- [ ] Handle Out-of-Memory conditions

### Phase V — Filesystem & Security
- [ ] Implement RAMFS
- [ ] Implement file creation
- [ ] Implement file reading
- [ ] Implement file updates
- [ ] Implement file deletion
- [ ] Implement access control
- [ ] Verify permission enforcement

### Phase VI — Bootable System
- [ ] Generate bootable ISO
- [ ] Verify standalone boot
- [ ] Test USB boot
- [ ] Test on compatible physical hardware

---

## AI-Assisted Development

Generative AI tools, including Hermes Agent, may be used as development assistants throughout the project.

AI assistance may be used for:
- Code generation and review
- Debugging and troubleshooting
- Architectural discussions
- Test planning
- Technical documentation
- Learning and understanding OS mechanisms

AI-generated code will be reviewed, tested, and documented. The project's development process will maintain documentation of AI usage to support transparency and technical understanding.

AI tools are not part of the kernel's required runtime dependencies.

---

## Project Structure

```text
MyOS/
├── boot/
├── kernel/
│   ├── interrupt/
│   ├── scheduler/
│   ├── sync/
│   ├── memory/
│   ├── fs/
│   ├── security/
│   └── shell/
├── include/
├── docs/
├── Makefile
└── README.md
```

<sub>The repository structure is a preliminary design and may evolve during development.</sub>

---

## Disclaimer

MyOS is an experimental operating system developed for educational and research purposes.

It is not intended for production use, and its hardware compatibility, security, and stability are not guaranteed.

Testing will initially be conducted in QEMU to reduce the risk of affecting physical hardware or existing operating systems.

---

## Project Goals

MyOS aims to provide a practical understanding of how an operating system works beneath the interface of everyday computing.

Rather than abstracting away the mechanisms, the project explores them directly through implementation, testing, and observation.

---

<p align="center">
  <strong>Built to understand what runs beneath the system.</strong>
</p>

## Documentation
- [System Architecture](myos_system_architecture.md)
