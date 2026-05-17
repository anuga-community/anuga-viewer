# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

ANUGA Viewer is a C++ OpenSceneGraph (OSG) application for visualizing ANUGA shallow water simulation output files (`.sww`). SWW files are NetCDF-format files containing bedslope geometry and water stage data at each timestep.

## Dependencies

- **OpenSceneGraph** (`libopenscenegraph-dev`) — 3D scene graph and rendering
- **NetCDF** (`libnetcdf-dev`) — reading SWW file format
- **GDAL** (`libgdal-dev`) — georeferenced texture mapping
- **CppUnit** (`libcppunit-dev`) — unit testing (optional)

## Build Commands

```bash
# Build both swwreader library and viewer binary
make

# Install the swwreader shared library to /usr/local/lib (requires sudo)
sudo make install

# Run tests
make test

# Clean build artifacts
make clean
```

The binary is output to `bin/anuga_viewer`. The swwreader shared library is output to `bin/libswwreader.so` (Linux) or `bin/libswwreader.dylib` (macOS), then moved to `/usr/local/lib` on `make install`.

> If the build fails with missing includes, deactivate any active conda environment first — conda can shadow system include paths.

## Environment Setup

After building and installing, add to `~/.bashrc`:

```bash
export SWOLLEN_BINDIR=/path/to/anuga-viewer/bin
export PATH=$PATH:$SWOLLEN_BINDIR
```

`SWOLLEN_BINDIR` is used at runtime to locate resource files (fonts, sky texture).

## Running

```bash
anuga_viewer data/cairns.sww
anuga_viewer -texture images/bedslope.jpg data/cairns.sww
anuga_viewer -scale 1.5 -tps 20 data/laminar.sww
anuga_viewer -help
```

## Architecture

The codebase has two separately compiled components:

### `swwreader/` — Shared Library

`SWWReader` (`include/swwreader.h`) is the core data layer. It opens an SWW file via NetCDF, reads bedslope geometry (x/y/z vertices, triangle indices) and per-timestep stage/momentum arrays into OSG array types. Key methods:

- `loadBedslopeVertexArray(index)` — loads bedslope z-heights at a given timestep (static or animated)
- `loadStageVertexArray(index)` — loads water surface heights and computes per-vertex colors/normals
- `getTimeSeries(polyIndex, type, data)` — extracts stage or momentum timeseries for a clicked polygon
- `refresh()` — reloads file if it has changed on disk (`FileChangedCheck`)

`filechangedcheck.cpp` provides file-modification monitoring.

### `viewer/` — Main Executable

The OSG scene graph hierarchy built in `main.cpp`:

```
rootnode (Group)
  ├── AnugaHUD (text overlay / timeseries graph)
  ├── DirectionalLight
  ├── sky_switch (Switch → Skybox sphere)
  └── model (PositionAttitudeTransform — applies vertical scale)
        ├── BedSlope (terrain geometry)
        ├── WaterSurface (animated water mesh)
        └── grid_switch (Switch → Axes/colorbar)
```

Key viewer classes:

- **`BedSlope`** / **`WaterSurface`** — OSG geometry nodes; call `update()` each frame; dirty-check their own state before uploading new geometry
- **`KeyboardEventHandler`** — OSG event handler; tracks timestep, wireframe mode, recording/playback state, grid mode, and shift+click polygon picking
- **`AnugaHUD`** / **`LineGraph`** — HUD overlay built with osgText; `LineGraph` renders the timeseries popup on shift+click
- **`State`** / **`StateList`** — serializable camera/settings snapshots recorded to `.swm` macro files for movie export
- **`CustomArgumentParser`** — wraps `osg::ArgumentParser`; detects `.swm` vs `.sww` input
- **`CustomViewer`** — thin subclass of `osgViewer::CompositeViewer`; adds grid/axes switching

The main loop drives timestep advance (real-time or playback), recording, and per-frame OSG `viewer.frame()`.

### `tests/`

CppUnit tests for `SWWReader` (`swwreadertest.cpp`) and `FileChangedCheck` (`touchedfiletest.cpp`). Run with `make test` from the top-level or `tests/` directory. The test binary is `tests/tests`.

## Key Command-Line Parameters

| Parameter | Description |
|-----------|-------------|
| `-texture <file>` | Apply image/GDAL texture to bedslope |
| `-scale <float>` | Vertical exaggeration factor |
| `-tps <float>` | Timesteps per second (default: 10) |
| `-hmin`/`-hmax` | Water height color scale limits |
| `-alphamin`/`-alphamax` | Water transparency limits |
| `-lightpos x,y,z` | Directional light position |
| `-nosky` | Disable skybox |
| `-movie <dir>` | Export frames to directory (with `.swm` input) |
| `-- screen <n>` | Select display screen (OSG standard) |

## In-App Keyboard Shortcuts

| Key | Action |
|-----|--------|
| Space | Pause/resume |
| `1` | Start/stop recording |
| `2` | Play/stop recorded macro |
| `3` | Save macro as `movie.swm` |
| `l` | Toggle lighting |
| `t` | Toggle texturing |
| `b` | Toggle backface culling |
| `O` | Screenshot |
| Shift+LMB | Show timeseries for clicked polygon |
