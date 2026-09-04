# QCamber Architecture

## Project Structure

```
qcamber/
├── src/              # Source code
│   ├── main.cpp      # Application entry point
│   ├── viewer.h/cpp  # Main viewer widget
│   ├── pngexporter.h/cpp  # PNG export engine
│   └── ...
├── bin/              # Compiled binaries
├── prebuilt/         # Pre-built dependencies
├── wiki/             # Design documents
├── config.ini        # Configuration file
└── qcamber.sln       # Visual Studio solution file
```

## Core Components

### 1. **ViewerWindow**
Main UI window for PCB design visualization. Handles:
- Graphics rendering via `QGraphicsScene`
- User interaction (pan, zoom, selection)
- Menu and toolbar management

### 2. **PngExporter**
High-resolution layer export system:
- Supports resolutions up to 50,000×50,000 pixels
- Handles step-repeat (panel array) transformation
- Configurable background colors, DPI, anti-aliasing
- Memory-aware rendering (400MP default limit)

### 3. **ODB++ Parser**
Reads ODB++ format PCB design files:
- Layer extraction
- Step-repeat block parsing
- Transformation matrix application

### 4. **ExportDialog**
Configuration UI for PNG export:
- Resolution presets (Full HD, 4K, 10K, 20K, custom)
- Layer selection dropdown
- Background color picker
- Step-repeat toggle

## Data Flow

```
[ODB++ File] 
    ↓
[ODB++ Parser] → [QGraphicsScene]
    ↓
[ExportDialog] → [PngExporter] → [PNG File]
```

## Design Patterns

- **MVC**: ViewerWindow (controller) + QGraphicsScene (model) + rendering (view)
- **Factory**: ExportDialog creates PngExporter with configuration
- **Observer**: Scene updates trigger viewport refresh

## Dependencies

- **Qt 5.x**: Core GUI framework
- **C++17**: Modern C++ features

## Build System

- **qmake**: Qt build system
- Generates platform-specific Makefiles
- Custom build configuration via `config.ini`
