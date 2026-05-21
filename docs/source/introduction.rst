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
   * - Shift + left-click on water
     - Show timeseries plot for that triangle
   * - Click anywhere else
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
   * - v
     - Cycle water colour mode: stage / depth / speed / momentum / max depth / max speed / max momentum / max stage
   * - ``[``
     - Decrease colour scale maximum by 20%
   * - ``]``
     - Increase colour scale maximum by 20%
   * - z
     - Decrease vertical exaggeration by 33%
   * - Z
     - Increase vertical exaggeration by 50%
   * - w
     - Cycle wireframe mode (off / water / bed / both)
   * - l
     - Toggle lighting
   * - t
     - Toggle bedslope texture
   * - b
     - Toggle backface culling
   * - c
     - Toggle steep-triangle culling
   * - g
     - Cycle grid / colorbar overlay
   * - i
     - Toggle information HUD
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

Press ``v`` to cycle through seven water colour modes.  All use a
blue (low) → green (mid) → red (high) gradient.  Use ``[`` and ``]`` to
adjust the saturation scale for the active mode without changing colour mode.

.. list-table::
   :widths: 25 75
   :header-rows: 1

   * - Mode
     - Description
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

The current mode and scale value are shown in the information HUD (press
``i`` to toggle).


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

Press ``t`` to toggle the texture on and off at runtime.


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
   * - ``-scale <float>``
     - Initial vertical exaggeration (default: 1.0)
   * - ``-tps <float>``
     - Timesteps per second (default: 10)
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
