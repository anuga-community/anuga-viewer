Release Notes
=============


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
