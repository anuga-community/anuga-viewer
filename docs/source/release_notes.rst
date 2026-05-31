Release Notes
=============


v0.5.4 — 2026-05-31
---------------------

New Features
~~~~~~~~~~~~

**Precomputed max quantities** (faster startup for large simulations)

  When an SWW file was produced with ANUGA's ``Collect_max_quantities_operator``
  (or ``set_collect_max_quantities``), the viewer reads the pre-stored centroid
  arrays (``max_stage_c``, ``max_depth_c``, ``max_speed_c``, ``max_uh_c``)
  directly instead of scanning every timestep.  This can reduce startup time
  significantly for large multi-processor simulations.  For SWW files without
  these variables the viewer falls back to scanning all timesteps as before.

**Independent bedslope texture cycling** (``t`` key)

  The ``t`` key now controls only the bedslope texture, independently of the
  water colour mode and transparency.  The available modes cycle in sequence
  (only modes with tiles present are offered):

  ============= =====================================================
  Mode          Bedslope appearance
  ============= =====================================================
  ``colour``    Terrain shaded by elevation (no texture)
  ``osm``       OpenStreetMap tiles draped over terrain
  ``satellite`` ESRI satellite imagery draped over terrain
  ============= =====================================================

  A ``-texture`` file on the command line adds a ``map`` mode alongside
  ``colour``.  Water transparency is now fully independent of ``t``.

**Smarter colour range defaults**

  Colour scale limits are now initialised from actual data values:

  - Stage and max-stage ranges are detected from the wet-cell values at
    startup, not the full terrain elevation span.
  - Depth, speed, and momentum limits are set from the per-vertex maxima.
  - All limits are rounded to a *nice* ``{1, 2, 5} × 10ⁿ`` boundary.
    The ``[`` / ``]`` nudge keys also snap to nice values.

**Stage range pan** (``,`` / ``.`` keys)

  Press ``,`` to shift the stage / max-stage colour window downward by one
  step, or ``.`` to shift it upward.  Combined with ``[`` / ``]`` this lets
  you zoom and pan the colour range without leaving the viewer.

Bug Fixes
~~~~~~~~~

- **Map texture distorted at startup** — OSG's default power-of-two
  requirement was silently downscaling GeoTIFFs with non-power-of-two
  dimensions (e.g. 2048 × 1392 → 2048 × 1024, a 26 % height loss).
  Fixed with ``setResizeNonPowerOfTwoHint(false)``; modern OpenGL handles
  non-power-of-two textures natively.

- **Map tile reloaded unnecessarily at startup** — the bedslope constructor
  called ``sww->getBedslopeTexture()`` even when the viewer starts in
  untextured mode, triggering a redundant disk read on every launch.

- **CM_MAX_STAGE colour scale wrong** — was mapping max-depth values through
  the depth colour scale instead of mapping absolute max-stage elevation
  through the stage scale.  Now uses the same ``stageoffset`` /
  ``stageheightmax`` reference as ``CM_STAGE``.

- **``[`` / ``]`` keys did not repaint water surface** — nudging the colour
  scale required waiting for the next timestep change before the new range
  was visible.  A forced refresh is now triggered immediately.


v0.5.2 — 2026-05-24
---------------------

New Features
~~~~~~~~~~~~

**Three-state view mode cycling** (``t`` key)

  With an OSM or satellite texture loaded, pressing ``t`` now cycles through
  three distinct view modes:

  =================== ================================================
  Mode                Description
  =================== ================================================
  ``landscape (osm)`` Map tiles on terrain, water surface hidden —
                      pure map view for navigating the domain
  ``colour (osm)``    Water coloured by quantity, map tile background
  ``colour``          Water coloured by quantity, plain terrain
  =================== ================================================

  The viewer opens in ``landscape (osm)`` mode so the terrain map is
  immediately visible.  Pressing ``t`` once reveals the flood with the
  map as a background; pressing again hides the map.

**Stage colour scale relative to domain elevation** (``-stagemin``)

  The stage colour mode now shows stage elevation above the domain's
  lowest bedslope point (auto-detected from the SWW file) rather than
  above absolute zero.  This makes the default ``heightmax`` of 1.0 m
  give a meaningful colour range for typical shallow-water simulations
  even when the domain sits at 600+ m above sea level.

  Use ``-stagemin <float>`` to set a custom reference elevation (e.g.
  ``-stagemin 620`` for a domain whose lowest point is 620 m ASL).
  Combine with ``-hmax`` to set the upper bound:
  ``-stagemin 620 -hmax 10`` colours the range 620–630 m.


Bug Fixes
~~~~~~~~~

- **Bedslope texture suppressed after file refresh** — ``onRefreshData()``
  was unconditionally setting the geometry colour array to the default brown,
  which in ``GL_MODULATE`` mode multiplied against the active map tile
  texture and made it nearly invisible.  The colour is now white when a
  texture is active, and ``onRefreshTextured()`` is called at the end of
  every refresh to restore the correct tex-coord array and material diffuse.


v0.5.3 — 2026-05-25
---------------------

New Features
~~~~~~~~~~~~

**Natural water colour mode** (``CM_BLUE``)

  A new ``CM_BLUE`` mode renders the water surface as a flat steel blue with
  a sphere-mapped environment reflection, giving a realistic water appearance
  without heat-map colouring.  This is now the first (default) colour mode.
  Press ``v`` to advance to the existing stage / depth / speed / momentum /
  max-* modes.

Bug Fixes
~~~~~~~~~

- **AppImage resources not found** — ``fonts/`` and ``images/`` were not
  bundled inside the AppImage.  They are now included under ``usr/bin/`` and
  located via ``SWOLLEN_BINDIR=$APPDIR/usr/bin`` in a custom ``AppRun``
  script, so the AppImage works from any working directory.

- **Makefile default target rebuilt only one object** — placing
  ``-include $(OBJ:.o=.d)`` before the ``$(TARGET)`` rule allowed ``.d``
  dependency files to hijack the default target.  Moved to after the target
  rule; ``make`` now rebuilds the full binary as expected.


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
