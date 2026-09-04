# Setup & Build Guide

## Prerequisites

### Windows
- **Qt 5.x** (5.12 or later) — download from [qt.io](https://www.qt.io/download)
- **Visual Studio 2019+** or **MinGW** compiler
- **CMake 3.16+** (optional, if using CMake build)
- **4GB RAM** minimum

### Linux (Ubuntu/Debian)
```bash
sudo apt-get install qt5-qmake qt5-default libqt5gui5 libqt5core5a
sudo apt-get install build-essential
```

### macOS
```bash
brew install qt@5
```

## Building from Source

### 1. Clone Repository
```bash
git clone https://github.com/JKLover0909/Qcamber-Meiko.git
cd Qcamber-Meiko
```

### 2. Configure Build

**Using qmake (recommended):**
```bash
qmake qcamber.pro
```

**Or with custom Qt path:**
```bash
/path/to/Qt/5.x/bin/qmake qcamber.pro
```

### 3. Build

**Linux/macOS:**
```bash
make -j$(nproc)
```

**Windows (MinGW):**
```bash
mingw32-make
```

**Windows (Visual Studio):**
```bash
nmake
```

### 4. Run

Binary location depends on OS:
- **Windows**: `bin/qcamber.exe`
- **Linux/macOS**: `bin/qcamber`

```bash
./bin/qcamber
```

## Configuration

Edit `config.ini` to customize:

```ini
[Application]
DefaultPath=./designs
MaxMemoryMB=4096
DefaultExportDPI=300

[Export]
MaxResolution=50000
DefaultBackground=black
EnableAntiAliasing=true
```

## Development Environment

### Qt Creator (Recommended IDE)
1. Open `qcamber.pro` in Qt Creator
2. Configure kit (Qt version, compiler, build system)
3. Build & Run (Ctrl+R)

### VS Code + Qt Extensions
- Install "Qt Tools" extension
- Configure `c_cpp_properties.json` with Qt include paths
- Build via integrated terminal

## Troubleshooting

### "Qt not found"
- Ensure Qt is installed and in PATH
- Set `QT_PATH` environment variable:
  ```bash
  export QT_PATH=/path/to/Qt/5.x
  ```

### "Cannot find shared library"
On Linux, add Qt lib to runtime path:
```bash
export LD_LIBRARY_PATH=$QT_PATH/lib:$LD_LIBRARY_PATH
```

### Build fails with C++17
Ensure compiler supports C++17. Update in `qcamber.pro`:
```qmake
CONFIG += c++17
```

## Testing PNG Export

After building, test the PNG export feature:

1. Launch QCamber
2. Open an ODB++ design file (or create test geometry)
3. File → Export to PNG
4. Configure: 10000×10000 pixels, Layer L2, background black
5. Select output path
6. Click Export
7. Verify PNG file generated without errors

## Next Steps

- See [Architecture](./architecture.md) for codebase overview
- Check [PNG Export Feature](./png-export.md) for export details
- Read [Contributing Guidelines](../README.md#contributing) for PR workflow
