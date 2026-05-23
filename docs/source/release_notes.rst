Release Notes
=============


v0.5.1 — 2026-05-23
---------------------

Bug Fixes
~~~~~~~~~

- **Momentum units garbled** — ``m²/s`` (UTF-8 superscript) has been replaced
  with ``m^2/s`` (plain ASCII) everywhere it appears: the ``-help`` text, the
  HUD colour-scale label, and the timeseries plot y-axis.

- **Help screen unreadable** — ``-help`` previously printed hundreds of lines
  of OSG internal options, burying the viewer's own flags.  It now shows a
  compact, single-screen summary of viewer options and keyboard shortcuts.
  The full OSG reference is still accessible via ``--osg-help``.


v0.5.0 — 2026-05-23
---------------------

New Features
~~~~~~~~~~~~

**Automatic OpenStreetMap texture draping**

  When an SWW file contains georeferencing metadata (UTM zone and hemisphere),
  the viewer automatically downloads OpenStreetMap tiles and drapes them as a
  texture over the bedslope terrain — no extra flags required.  The resulting
  GeoTIFF is cached next to the SWW file so subsequent launches use it
  instantly.

  Attribution: map data © OpenStreetMap contributors —
  `openstreetmap.org/copyright <https://www.openstreetmap.org/copyright>`_

**Satellite imagery option** (``-maptiles satellite``)

  Pass ``-maptiles satellite`` to use ESRI World Imagery instead of the
  default OpenStreetMap street map.  Both GeoTIFFs are cached independently,
  so you can switch between them without re-downloading.

  Attribution: imagery © Esri, Maxar, Earthstar Geographics, and GIS User
  Community

**Map tile source flag** (``-maptiles osm|satellite|none``)

  ============= =====================================================
  Value         Effect
  ============= =====================================================
  ``osm``       OpenStreetMap street map (default)
  ``satellite`` ESRI World Imagery
  ``none``      Skip tile fetch; show bedslope without texture
  ============= =====================================================

Bug Fixes
~~~~~~~~~

- **UTM zone not read from SWW** — the zone, hemisphere, and corner-offset
  attributes were read *after* ``nc_close()``, so all NetCDF calls silently
  failed and the zone was reported as −1.  Moved all global-attribute reads
  to before the file is closed.

- **GDAL path compiled out** — the Makefile linked ``-lgdal`` but never
  defined ``-DHAVE_GDAL``, so ``osmtexture.cpp`` always compiled the no-op
  stub.  ``GDAL_CFLAGS`` and ``-DHAVE_GDAL`` are now set automatically when
  ``gdal-config`` is present.


v0.4.0 — 2026-05-21
---------------------

New Features
~~~~~~~~~~~~

**Timeseries plot for dry points**

  Shift + left-click now works anywhere on the terrain, not just on wet
  triangles.  Clicking a dry area shows the timeseries of the selected
  quantity at that point for the full simulation duration.

**Timeseries quantity matches colour mode**

  The timeseries graph always shows the same quantity as the active colour
  mode.  Pressing ``v`` or ``V`` to change colour mode now immediately
  updates the graph — no need to re-click the point.

**Improved timeseries graph appearance**

  The graph panel now uses a semi-transparent navy background that lets the
  3D scene show through.  The data line is 2.5 px wide and the grid border
  is 1.5 px wide, making both clearly visible.  Y-axis labels include the
  quantity unit (m, m/s, m²/s).

Bug Fixes
~~~~~~~~~

- **Timeseries graph invisible** — the background panel was assigned to
  OSG's ``TRANSPARENT_BIN``, which caused it to be rendered *after* all
  opaque geometry and paint over the grid and data line.  Removed the hint
  so the geode draws in insertion order (background first, then lines on
  top).

- **AppImage crash on startup** — ``libfreetype.so.6`` was not bundled in
  the AppImage (linuxdeploy blacklists it as "standard"), but conda
  environments can expose an incompatible version.  The library is now
  included explicitly.


v0.3.0 — 2026-05-21
---------------------

`GitHub release (v0.3.0) <https://github.com/anuga-community/anuga-viewer/releases/tag/v0.3.0>`_

New Features
~~~~~~~~~~~~

**Stage colour mode** — current absolute water surface elevation is now the
first (default) colour mode.  The full cycle (``v`` / ``V``) is: stage →
depth → speed → momentum → max depth → max speed → max momentum → max stage.

**Reverse colour mode cycling** (``V``)

  Press ``V`` (Shift+v) to step backwards through the colour modes.

**Keyboard pan for touchpad users** (Shift + arrow keys)

  Hold Shift and press any arrow key to pan the camera.  Each keypress moves
  the scene by 3 % of the current eye distance, so panning stays proportional
  at any zoom level.  This provides middle-mouse-button panning for users on
  touchpads.

Bug Fixes
~~~~~~~~~

- **Camera reset** (``x`` key) — now reliably resets the view to the default
  position.  Previously the reset was queued as an event and did not always
  fire.


v0.2.0 — 2026-05-20
---------------------

`GitHub release (v0.2.0) <https://github.com/anuga-community/anuga-viewer/releases/tag/v0.2.0>`_

New Features
~~~~~~~~~~~~

**Seven water colour modes** (cycle with ``v``)

  Four new *max-over-time* modes join the existing momentum, speed, and depth
  modes.  The max modes are computed once when the file is opened and show the
  worst-case flood extent frozen in time, independent of the current animation
  timestep:

  - **max depth** — peak water depth at each point over all timesteps
  - **max speed** — peak flow speed at each point over all timesteps
  - **max momentum** — peak momentum magnitude at each point over all timesteps (m²/s)
  - **max stage** — depth above bed at the moment of peak stage (high-water mark)

**Interactive vertical exaggeration** (``z`` / ``Z`` keys)

  Press ``Z`` to exaggerate or ``z`` to flatten the vertical scale.  Each
  keypress changes the scale by a factor of 1.5.  The current multiplier is
  shown in the HUD alongside the other status lines.  The ``-scale`` command-line
  option sets the initial value.

**Interactive colour scale** (``[`` / ``]`` keys)

  Nudge the colour saturation scale for the active mode up or down by 20%
  without leaving the viewer.  The mode name and current scale value are shown
  in the HUD.  New ``-speedmax`` and ``-momentummax`` command-line options set
  the initial values.

Bug Fixes
~~~~~~~~~

- **Lighting at non-unit z scale** — surface normals are now recomputed in
  world space when vertical exaggeration is changed, giving correct shading at
  any scale.  Previously, normals were distorted by the inverse-transpose of the
  scale transform, making gently-sloping surfaces appear to be lit from the side.

- **Speed colour mode** — speed is now computed as ``momentum_magnitude / depth`` using
  raw NetCDF values in metres, not normalised vertex coordinates.  Previously,
  physically reasonable speeds required an unrealistically large ``speedmax``
  (≈ 1000 m/s).

- **Colour gradient direction** — the gradient was inverted (red = low,
  blue = high).  It now follows the standard heatmap convention: blue = low,
  green = mid, red = high.

- **Depth colour mode** — was rendering as uniform grey.  Now uses the same
  blue→green→red gradient scaled to ``heightmax`` metres.

- **Switching colour modes** — pressing ``v`` now immediately redraws the water
  surface.  Previously the display was not updated until the next timestep
  change.

- **Max colour modes animation** — the water surface geometry is now frozen at
  the high-water mark when in any max mode.  Previously the mesh still animated
  with the current timestep, causing flickering wet/dry changes.

- **Startup window** — the viewer now opens in a decorated window (1024 × 768)
  rather than a borderless full-screen-sized window.


v0.1.0 — 2026-05-19
---------------------

`GitHub release (v0.1.0) <https://github.com/anuga-community/anuga-viewer/releases/tag/v0.1.0>`_

First packaged release.  Pre-built packages available for Linux (AppImage),
macOS (DMG), and Windows (ZIP).

Includes the core ANUGA Viewer features:

- Animated 3D water surface over bedslope terrain
- Bedslope texture mapping (projected or GDAL georeferenced)
- Water colour modes: momentum, speed, depth
- Timeseries plot on shift+click
- Wireframe, culling, lighting, and grid overlay toggles
- Camera path recording and JPEG frame export for movie production
- Skybox environment
