# PacketLab

`PacketLab` is a Qt-based desktop skeleton for a future Cisco Packet Tracer alternative.

## Current scope

- `Qt Widgets` desktop shell
- topology list with starter devices
- minimal simulation core
- JSON project save/load
- `bash` scripts for configure/build/run

## Planned MVP

- topology canvas
- ports and links
- IPv4 addressing
- ARP + ICMP ping simulation
- static routing
- project save/load

## Build requirements

- CMake 3.21+
- C++20 compiler
- Qt 6 Widgets development package

## Quick start

```bash
./scripts/build.sh
./scripts/run.sh
```

Sample project:

- `assets/demo-project.json`
