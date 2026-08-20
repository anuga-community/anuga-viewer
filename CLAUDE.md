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
- `epsgToUTM(epsg, zone, south, crsName)` — static; maps an EPSG code to a UTM zone/hemisphere. Shared with the viewer's `-epsg` option so both accept the same set of codes.

On load, the reader resolves the projection from the SWW global attributes. It prefers the `epsg` attribute (accepted as an integer or as `"EPSG:nnnn"` text) because that pins down the hemisphere as well as the zone, and because ANUGA only back-fills `zone` for WGS 84 UTM codes — a GDA2020 / MGA file carries `epsg = 7856` with `zone = -1`. Failing that it falls back to `zone` plus `hemisphere`, or to the sign of `false_northing` when `hemisphere` is absent (older files). The result drives map tile fetching; `getUTMZone()` returns -1 when nothing resolves.

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
| `-texture <file>` | Apply image/GDAL texture to bedslope (overrides auto tile fetch) |
| `-maptiles osm\|satellite\|none` | Map tile source when SWW has UTM zone (default: `osm`) |
| `--epsg <code>` | Override/supply UTM zone, taking precedence over the SWW's own `epsg`/`zone` attributes. WGS 84 UTM `32601`-`32660`/`32701`-`32760`, GDA2020 MGA `7846`-`7859`, GDA94 MGA `28348`-`28358`, AGD84 AMG `20348`-`20358`, AGD66 AMG `20248`-`20258` (e.g. `32755` = zone 55S, `7856` = MGA zone 56) |
| `-scale <float>` | Initial vertical exaggeration factor (default: 1.0) |
| `-tps <float>` | Timesteps per second (default: 10) |
| `-wetdepth <float>` | Depth (m) below which water fades transparent (rain-on-grid) |
| `-hmin`/`-hmax` | Water depth colour scale limits (metres) |
| `-speedmax <float>` | Speed colour scale maximum (m/s) |
| `-momentummax <float>` | Momentum colour scale maximum (m²/s) |
| `-alphamin`/`-alphamax` | Water transparency limits |
| `-lightpos x,y,z` | Directional light position |
| `-nosky` | Disable skybox |
| `-transparent` | Transparent background: no sky, zero-alpha clear colour, screenshots written as PNG with alpha. Toggle at runtime with `B`. |
| `-movie <dir>` | Export frames to directory (with `.swm` input) |
| `-- screen <n>` | Select display screen (OSG standard) |

## In-App Keyboard Shortcuts

| Key | Action |
|-----|--------|
| Space | Pause/resume |
| `v` / `V` | Cycle water colour mode forward/backward: blue / stage / depth / speed / momentum / max depth / max speed / max momentum / max stage |
| `[` / `]` | Decrease/increase colour scale right endpoint |
| `{` / `}` | Decrease/increase colour scale left endpoint (stage modes) |
| `,` / `.` | Pan colour scale range left/right (stage modes) |
| `a` / `A` | Decrease/increase shallow-water transparency threshold (wetdepth) |
| `z` / `Z` | Decrease/increase vertical exaggeration by factor of 1.5 |
| `w` | Cycle wireframe mode (off / water / bed / both) |
| `g` | Cycle grid/colorbar overlay |
| `i` | Cycle HUD: full → minimal (title + time only) → off |
| `l` | Toggle lighting |
| `t` | Cycle view mode: landscape → colour (osm) → colour |
| `b` | Toggle backface culling |
| `B` | Toggle transparent background (screenshots switch between `.jpg` and `.png`) |
| `c` | Toggle steep-triangle culling |
| `x` | Reset camera to default position |
| `r` | Reset animation to timestep 0 |
| `1` | Start/stop recording camera path |
| `2` | Play/stop recorded macro |
| `3` | Save macro as `movie.swm` |
| `O` | Screenshot |
| Shift+LMB | Show timeseries for clicked polygon |
| Escape | Quit |
