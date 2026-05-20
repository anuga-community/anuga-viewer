ANUGA Viewer
============

A 3D viewer for `ANUGA <https://github.com/anuga-community/anuga_core>`_ shallow-water
simulation output files (``.sww``).  SWW files contain bedslope geometry and per-timestep
water stage and momentum data.  The viewer renders the animated water surface over the
terrain and provides interactive controls for exploring the simulation.


Quick Start
-----------

Download the latest AppImage from the
`Releases <https://github.com/anuga-community/anuga-viewer/releases>`_ page, make it
executable, and run it::

   chmod +x ANUGA_Viewer-x86_64.AppImage
   ./ANUGA_Viewer-x86_64.AppImage path/to/simulation.sww

Or install it to your path::

   cp ANUGA_Viewer-x86_64.AppImage ~/.local/bin/anuga_viewer
   anuga_viewer path/to/simulation.sww


Mouse Controls
--------------

+--------------------------------------+--------------------------------------+
| Action                               | Effect                               |
+======================================+======================================+
| Left-drag                            | Rotate/spin the model                |
+--------------------------------------+--------------------------------------+
| Right-drag                           | Zoom in/out                          |
+--------------------------------------+--------------------------------------+
| Middle-drag (or both buttons)        | Pan                                  |
+--------------------------------------+--------------------------------------+
| Shift + left-click on water          | Show timeseries plot for that        |
|                                      | triangle                             |
+--------------------------------------+--------------------------------------+
| Click anywhere else                  | Hide timeseries plot                 |
+--------------------------------------+--------------------------------------+


Keyboard Shortcuts
------------------

Animation
~~~~~~~~~

+------------+-------------------------------------------------------------+
| Key        | Action                                                      |
+============+=============================================================+
| Space      | Pause / resume animation                                    |
+------------+-------------------------------------------------------------+
| Left arrow | Step back one timestep (when paused)                        |
+------------+-------------------------------------------------------------+
| Right arrow| Step forward one timestep (when paused)                     |
+------------+-------------------------------------------------------------+
| r          | Reset animation to timestep 0                               |
+------------+-------------------------------------------------------------+

Display
~~~~~~~

+------------+-------------------------------------------------------------+
| Key        | Action                                                      |
+============+=============================================================+
| v          | Cycle water colour mode (see `Colour Modes`_ below)         |
+------------+-------------------------------------------------------------+
| ``[``      | Decrease colour scale maximum by 20%                        |
+------------+-------------------------------------------------------------+
| ``]``      | Increase colour scale maximum by 20%                        |
+------------+-------------------------------------------------------------+
| z          | Decrease vertical exaggeration by 33%                       |
+------------+-------------------------------------------------------------+
| Z          | Increase vertical exaggeration by 50%                       |
+------------+-------------------------------------------------------------+
| w          | Cycle wireframe mode (off / water / bed / both)             |
+------------+-------------------------------------------------------------+
| l          | Toggle lighting                                             |
+------------+-------------------------------------------------------------+
| t          | Toggle bedslope texture                                     |
+------------+-------------------------------------------------------------+
| b          | Toggle backface culling                                     |
+------------+-------------------------------------------------------------+
| g          | Cycle grid/colorbar overlay                                 |
+------------+-------------------------------------------------------------+
| i          | Toggle information HUD                                      |
+------------+-------------------------------------------------------------+
| x          | Reset camera to default position                            |
+------------+-------------------------------------------------------------+
| c          | Toggle steep-triangle culling                               |
+------------+-------------------------------------------------------------+
| O          | Screenshot                                                  |
+------------+-------------------------------------------------------------+
| Escape     | Quit                                                        |
+------------+-------------------------------------------------------------+

Recording
~~~~~~~~~

+------------+-------------------------------------------------------------+
| Key        | Action                                                      |
+============+=============================================================+
| 1          | Start / stop recording camera path                          |
+------------+-------------------------------------------------------------+
| 2          | Play / stop recorded macro                                  |
+------------+-------------------------------------------------------------+
| 3          | Save recorded macro to ``movie.swm``                        |
+------------+-------------------------------------------------------------+


Colour Modes
------------

Press ``v`` to cycle through seven water colour modes.  All use a
blue (low) → green (mid) → red (high) gradient.  Use ``[`` / ``]`` to
adjust the saturation scale for the active mode.

+----------------------+----------------------------------------------------+
| Mode                 | Description                                        |
+======================+====================================================+
| momentum             | Current momentum magnitude (m²/s)                  |
+----------------------+----------------------------------------------------+
| speed                | Current flow speed = |momentum| / depth (m/s)      |
+----------------------+----------------------------------------------------+
| depth                | Current water depth (m)                            |
+----------------------+----------------------------------------------------+
| max depth            | Maximum depth at each point over all timesteps     |
+----------------------+----------------------------------------------------+
| max speed            | Maximum speed at each point over all timesteps     |
+----------------------+----------------------------------------------------+
| max momentum         | Maximum momentum at each point over all timesteps  |
+----------------------+----------------------------------------------------+
| max stage            | Depth above bed at the moment of peak stage,       |
|                      | frozen at the high-water mark                      |
+----------------------+----------------------------------------------------+

The four ``max`` modes display a static snapshot of the worst-case flood
extent and are independent of the current animation timestep.


Vertical Exaggeration
---------------------

Press ``z`` to flatten or ``Z`` to exaggerate the vertical scale.  Each
keypress changes the scale by a factor of 1.5.  The current multiplier is
shown in the HUD.  Lighting normals are recomputed to match the displayed
geometry, so shading remains correct at any exaggeration level.

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

::

   anuga_viewer [options] <file.sww>

+-------------------------+--------------------------------------------------+
| Option                  | Description                                      |
+=========================+==================================================+
| ``-texture <file>``     | Apply image/GDAL texture to bedslope             |
+-------------------------+--------------------------------------------------+
| ``-scale <float>``      | Initial vertical exaggeration (default: 1.0)     |
+-------------------------+--------------------------------------------------+
| ``-tps <float>``        | Timesteps per second (default: 10)               |
+-------------------------+--------------------------------------------------+
| ``-hmin <float>``       | Water depth colour scale minimum (m)             |
+-------------------------+--------------------------------------------------+
| ``-hmax <float>``       | Water depth colour scale maximum (m)             |
+-------------------------+--------------------------------------------------+
| ``-speedmax <float>``   | Speed colour scale maximum (m/s)                 |
+-------------------------+--------------------------------------------------+
| ``-momentummax <float>``| Momentum colour scale maximum (m²/s)             |
+-------------------------+--------------------------------------------------+
| ``-alphamin <float>``   | Water minimum opacity (0–1)                      |
+-------------------------+--------------------------------------------------+
| ``-alphamax <float>``   | Water maximum opacity (0–1)                      |
+-------------------------+--------------------------------------------------+
| ``-lightpos x,y,z``     | Directional light position (z is up)             |
+-------------------------+--------------------------------------------------+
| ``-nosky``              | Disable skybox                                   |
+-------------------------+--------------------------------------------------+
| ``-movie <dir>``        | Export frames to directory (use with ``.swm``)   |
+-------------------------+--------------------------------------------------+
| ``-- screen <n>``       | Select display screen (OSG standard)             |
+-------------------------+--------------------------------------------------+


How to Make a Movie
-------------------

1. Press ``1`` to start recording the camera path, ``1`` again to stop.
   Press ``3`` to save the macro as ``movie.swm`` in the viewer's bin folder.

2. Export the macro as JPEG frames::

      anuga_viewer -movie myframes movie.swm

3. Stitch frames into a video with ffmpeg::

      ffmpeg -framerate 25 -i myframes/frame_%d_%d.jpg output.mp4


Building from Source
--------------------

See `INSTALL_ubuntu.rst <INSTALL_ubuntu.rst>`_ for Ubuntu/Linux build instructions.


Troubleshooting
---------------

**I have two monitors and the viewer opens on the wrong one.**
  Pass ``-- screen 0`` (or ``1``) on the command line::

     anuga_viewer -- screen 0 simulation.sww

**TIF textures fail to load.**
  Re-save the file in GIMP or another image editor.  Very large textures
  (> 4096 × 4096) may also exceed GPU limits.

**The viewer crashes with a GL error on WSL2.**
  Make sure you are using the system OpenSceneGraph (``libopenscenegraph-dev``)
  and not one installed in a conda environment.  Conda's ``libGLX.so``
  conflicts with the WSL2 Intel GPU driver.
