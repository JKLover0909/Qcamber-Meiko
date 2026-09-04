# QCamber Documentation

QCamber is a Qt-based PCB (Printed Circuit Board) design viewer and export tool. This directory contains setup, architecture, and development documentation.

## Contents

- **[Architecture](./architecture.md)** — Project structure, core components, and design patterns
- **[Setup & Build](./setup.md)** — Prerequisites, compilation, and environment setup
- **[PNG Export Feature](./png-export.md)** — High-resolution PCB layer export capabilities

## Quick Start

1. Clone repository
2. Install Qt 5.x (see [Setup Guide](./setup.md))
3. Build: `qmake && make`
4. Run QCamber from `bin/` directory

## Requirements

- **Qt 5.x** with Widgets module
- **C++17** compatible compiler
- **4GB RAM** minimum (8GB recommended for exports)

## License

See [COPYING](../COPYING) — GPL v2 and later
