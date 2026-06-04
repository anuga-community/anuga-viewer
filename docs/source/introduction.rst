Usage
=====

ANUGA Viewer displays the animated output of an `ANUGA
<https://github.com/anuga-community/anuga_core>`_ shallow-water simulation.
Open a ``.sww`` file from the command line::

   anuga_viewer path/to/simulation.sww


Mouse Controls
--------------

.. list-table::
   :widths: 40 60
   :header-rows: 1

   * - Action
     - Effect
   * - Left-drag
     - Rotate / spin the model
   * - Right-drag
     - Zoom in / out
   * - Middle-drag (or both buttons)
     - Pan (or use Shift + arrow keys on a touchpad)
   * - ``t`` key
     - Toggle between **colour** mode (water coloured by data) and **landscape** mode (bedslope texture / aerial image)
   * - Shift + left-click
     - Show timeseries plot for the clicked triangle (wet or dry)
   * - Click anywhere (no Shift)
     - Hide timeseries plot


Keyboard Shortcuts
------------------

Animation
~~~~~~~~~

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Key
     - Action
   * - Space
     - Pause / resume animation
   * - Left arrow
     - Step back one timestep (when paused); speed down (when playing)
   * - Right arrow
     - Step forward one timestep (when paused); speed up (when playing)
   * - r
     - Reset animation to timestep 0

Display
~~~~~~~

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Key
     - Action
   * - v / V
     - Cycle water colour mode forward / backward: blue / stage / depth / speed / momentum / max depth / max speed / max momentum / max stage
   * - ``[`` / ``]``
     - Decrease / increase the colour scale right endpoint (max)
   * - ``{`` / ``}``
     - Decrease / increase the colour scale left endpoint (min) — stage modes only
   * - ``,`` / ``.``
     - Pan the entire colour range left / right — stage modes only
   * - a / A
     - Decrease / increase shallow-water transparency threshold (wetdepth)
   * - z / Z
     - Decrease / increase vertical exaggeration by 1.5×
   * - t
     - Cycle view mode: landscape → colour (OSM) → colour (satellite)
   * - q
     - Cycle data source: vertex (interpolated) → centroid (smooth) → faceted (flat per-triangle)
   * - w
     - Cycle wireframe mode (off / water / bed / both)
   * - g
     - Cycle grid / colour bar overlay
   * - c
     - Toggle steep-triangle culling
   * - b
     - Toggle backface culling
   * - l
     - Toggle lighting
   * - i
     - Cycle information HUD: full → minimal (title + time only) → off
   * - h
     - Toggle help panel (key binding reference)
   * - Shift + arrow keys
     - Pan camera (useful on touchpads without middle mouse)
   * - x
     - Reset camera to default position
   * - O
     - Screenshot
   * - Escape
     - Quit

Recording
~~~~~~~~~

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Key
     - Action
   * - 1
     - Start / stop recording camera path
   * - 2
     - Play / stop recorded macro
   * - 3
     - Save recorded macro to ``movie.swm``


.. _colour-modes:

Colour Modes
------------

Press ``v`` to cycle through eight water colour modes.  The default
**blue** mode renders water as a natural blue colour.  The remaining modes
use a blue (low) → green (mid) → red (high) gradient to map a data
quantity to colour.  Use the colour range keys (see below) to adjust the
scale limits for the active mode without changing colour mode.

.. list-table::
   :widths: 25 75
   :header-rows: 1

   * - Mode
     - Description
   * - blue
     - Natural blue water colour (default; no data mapping)
   * - stage
     - Current absolute water surface elevation above datum (m)
   * - depth
     - Current water depth above the bed (m)
   * - speed
     - Current flow speed = momentum / depth (m/s)
   * - momentum
     - Current momentum magnitude (m²/s)
   * - max depth
     - Maximum depth at each point over all timesteps
   * - max speed
     - Maximum speed at each point over all timesteps
   * - max momentum
     - Maximum momentum at each point over all timesteps
   * - max stage
     - Depth above bed at the moment of peak stage, frozen at the
       high-water mark

The four **max** modes show a static snapshot of worst-case flood extent
and do not animate with the timestep.  They are computed once when the file
is opened.

The current mode and scale limits are shown in the information HUD (press
``i`` to toggle).


.. _colour-range-controls:

Colour Range Controls
---------------------

The colour scale endpoints can be moved independently so you can zoom in on a
narrow band of values or pan the range across the data without leaving the
current colour mode.

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Key
     - Effect
   * - ``]``
     - Increase the right endpoint (max) by one step
   * - ``[``
     - Decrease the right endpoint (max) by one step
   * - ``}``
     - Increase the left endpoint (min) by one step *(stage modes only)*
   * - ``{``
     - Decrease the left endpoint (min) by one step *(stage modes only)*
   * - ``.``
     - Pan the entire range right by one step *(stage modes only)*
   * - ``,``
     - Pan the entire range left by one step *(stage modes only)*

The step size is proportional to the current range width, so adjustments
remain responsive at any zoom level.  Neither endpoint will cross zero in
the direction it came from.

The left-endpoint and pan keys apply only to **stage** and **max stage**
modes, where the range minimum carries physical meaning (e.g. the
background terrain elevation).  In depth, speed, and momentum modes the
minimum is always zero and only the right endpoint is adjustable.


Shallow-water Transparency
--------------------------

For rain-on-grid simulations, very shallow water can clutter the view with
near-invisible colour noise.  The ``-wetdepth`` option sets a depth threshold
below which water fades from fully transparent (dry) to the minimum opacity
(``-alphamin``, default 0.8):

.. code-block:: text

   anuga_viewer -wetdepth 0.2 simulation.sww

With ``-wetdepth 0.2``, triangles with less than 0.2 m of water are partially
transparent, making dry areas visually distinct from shallow inundation.

Use ``a`` / ``A`` at runtime to decrease / increase the threshold in
``{1, 2, 5} × 10ⁿ`` steps (0.05 → 0.1 → 0.2 → …).  Set to 0 to return to
the standard opacity ramp (controlled by ``-alphamin`` / ``-alphamax``).

The current threshold is shown in the HUD as ``(a/A) wetdepth: 0–0.2 m``
or ``(a/A) wetdepth: off``.


Timeseries Plot
---------------

Hold **Shift** and left-click any triangle — wet or dry — to display a
timeseries graph of the active colour quantity at that point over the full
simulation duration.

The graph updates automatically when you change colour mode with ``v`` or
``V``, so the plotted quantity always matches the colour you are looking at.

Click anywhere **without** Shift to dismiss the graph.


Vertical Exaggeration
---------------------

Press ``z`` to flatten or ``Z`` to exaggerate the vertical scale.  Each
keypress changes the scale by a factor of 1.5.  The current multiplier is
shown in the HUD.

Lighting normals are recomputed automatically to match the displayed
geometry, so shading remains consistent at any exaggeration level.

The initial vertical scale can also be set on the command line::

   anuga_viewer -scale 5 simulation.sww


Applying Textures
-----------------

Apply an image or georeferenced raster to the bedslope with ``-texture``::

   anuga_viewer -texture images/bedslope.jpg simulation.sww

There are two mapping modes:

1. **Georeferenced** — if the file contains GDAL geodata (e.g. a GeoTIFF),
   the texture is registered to real-world coordinates automatically.

2. **Projected** — otherwise, the image is draped from above, scaled to
   exactly cover the bedslope bounding rectangle.

If the SWW file contains UTM zone metadata, OpenStreetMap or ESRI satellite
tiles are fetched automatically.  Use ``-maptiles satellite`` or
``-maptiles none`` to choose the source.  If the UTM zone is absent or
incorrect, supply it with ``-epsg``::

   anuga_viewer --epsg 32755 simulation.sww   # UTM zone 55 South

Press ``t`` to cycle between landscape and colour view modes at runtime.


Command-Line Options
--------------------

.. code-block:: text

   anuga_viewer [options] <file.sww>

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Option
     - Description
   * - ``-texture <file>``
     - Apply image / GDAL texture to bedslope
   * - ``-maptiles osm|satellite|none``
     - Map tile source when SWW has UTM zone (default: ``osm``)
   * - ``--epsg <code>``
     - Override / supply UTM georeferencing (e.g. ``32755`` = zone 55 South)
   * - ``-scale <float>``
     - Initial vertical exaggeration (default: 1.0)
   * - ``-tps <float>``
     - Timesteps per second (default: 10)
   * - ``-fps <float>``
     - Maximum display frame rate (default: 30); reduce to cut X11 display bandwidth
   * - ``-wetdepth <float>``
     - Depth (m) below which water fades to transparent (rain-on-grid)
   * - ``-hmin <float>``
     - Water depth colour scale minimum (m)
   * - ``-hmax <float>``
     - Water depth colour scale maximum (m)
   * - ``-speedmax <float>``
     - Speed colour scale maximum (m/s)
   * - ``-momentummax <float>``
     - Momentum colour scale maximum (m²/s)
   * - ``-alphamin <float>``
     - Water minimum opacity (0–1)
   * - ``-alphamax <float>``
     - Water maximum opacity (0–1)
   * - ``-lightpos x,y,z``
     - Directional light position (z is up)
   * - ``-nosky``
     - Disable skybox
   * - ``-movie <dir>``
     - Export frames to directory (use with ``.swm`` input)
   * - ``-- screen <n>``
     - Select display screen (OSG standard)


How to Make a Movie
-------------------

1. Press ``1`` to start recording the camera path, ``1`` again to stop.
   Press ``3`` to save the macro as ``movie.swm``.

2. Export the macro as JPEG frames::

      anuga_viewer -movie myframes movie.swm

3. Stitch the frames into a video with ffmpeg::

      ffmpeg -framerate 25 -i myframes/frame_%d_%d.jpg output.mp4


Lighting
--------

A single directional light is placed at a default position.  To change
its direction::

   anuga_viewer -lightpos 1,1,2 simulation.sww

Press ``l`` to toggle lighting off for flat (unlit) rendering.


Troubleshooting
---------------

**TIF textures fail to load.**
  Re-save the file in GIMP or another image editor.  Very large textures
  (> 4096 × 4096) may also exceed GPU limits — try resizing first.

**The viewer opens on the wrong monitor.**
  Pass ``-- screen 0`` (or ``1``) on the command line::

     anuga_viewer -- screen 0 simulation.sww

**The viewer crashes with a GL / segfault error on WSL2.**
  Make sure you are using the system OpenSceneGraph (``libopenscenegraph-dev``)
  and not one installed inside a conda environment.  Conda's ``libGLX.so``
  conflicts with the WSL2 Intel GPU driver.
