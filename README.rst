ANUGA Viewer
============

A 3D viewer for `ANUGA <https://github.com/anuga-community/anuga_core>`_ shallow-water
simulation output files (``.sww``).  SWW files contain bedslope geometry and per-timestep
water stage and momentum data.  The viewer renders the animated water surface over the
terrain and provides interactive controls for exploring the simulation.


Installation
------------

Pre-built packages for Linux, macOS, and Windows are available on the
`Releases <https://github.com/anuga-community/anuga-viewer/releases>`_ page.

**Linux — AppImage**

Download ``anuga-viewer-linux-x86_64.AppImage``, make it executable, and run it::

   chmod +x anuga-viewer-linux-x86_64.AppImage

   # Run directly
   ./anuga-viewer-linux-x86_64.AppImage path/to/simulation.sww

   # Or install to your PATH (no root required)
   cp anuga-viewer-linux-x86_64.AppImage ~/.local/bin/anuga_viewer
   anuga_viewer path/to/simulation.sww

**macOS**

Download ``anuga-viewer-macos.dmg``, open it, and drag **ANUGA Viewer** to
**Applications**.  On first launch macOS may show a security warning — right-click
the app and choose **Open** to bypass it.

**Windows**

Download and run ``anuga-viewer-windows-setup.exe``.  The installer adds Start Menu
and Desktop shortcuts and registers ``.sww`` file associations.

.. note::

   Windows may show a **SmartScreen** warning because the installer is not yet signed
   with a commercial certificate.  Click **More info** → **Run anyway** to proceed.

**Build from source (Ubuntu / Debian)**

::

   sudo apt-get install git build-essential libcurl4-openssl-dev libcppunit-dev \
                        libopenscenegraph-dev libnetcdf-dev
   git clone https://github.com/anuga-community/anuga-viewer.git
   cd anuga-viewer
   make
   sudo make install

Add to ``~/.bashrc``::

   export SWOLLEN_BINDIR=/path/to/anuga-viewer/bin
   export PATH=$PATH:$SWOLLEN_BINDIR

.. note::

   If the build fails with missing includes, deactivate any active conda environment
   first — conda can shadow system include paths.


Mouse Controls
--------------

+--------------------------------------+--------------------------------------+
| Action                               | Effect                               |
+======================================+======================================+
| Left-drag                            | Rotate / spin the model              |
+--------------------------------------+--------------------------------------+
| Right-drag                           | Zoom in / out                        |
+--------------------------------------+--------------------------------------+
| Middle-drag (or both buttons)        | Pan                                  |
+--------------------------------------+--------------------------------------+
| Shift + left-click                   | Show timeseries plot for that        |
|                                      | triangle (wet or dry)                |
+--------------------------------------+--------------------------------------+
| Click anywhere (no Shift)            | Hide timeseries plot                 |
+--------------------------------------+--------------------------------------+


Keyboard Shortcuts
------------------

Animation
~~~~~~~~~

+-------------+-------------------------------------------------------------+
| Key         | Action                                                      |
+=============+=============================================================+
| Space       | Pause / resume animation                                    |
+-------------+-------------------------------------------------------------+
| Left arrow  | Step back one timestep (paused) / speed down (playing)      |
+-------------+-------------------------------------------------------------+
| Right arrow | Step forward one timestep (paused) / speed up (playing)     |
+-------------+-------------------------------------------------------------+
| r           | Reset animation to timestep 0                               |
+-------------+-------------------------------------------------------------+

Display
~~~~~~~

+--------------------+--------------------------------------------------------+
| Key                | Action                                                 |
+====================+========================================================+
| v / V              | Cycle water colour mode forward / backward             |
+--------------------+--------------------------------------------------------+
| ``[`` / ``]``      | Decrease / increase colour scale right endpoint (max)  |
+--------------------+--------------------------------------------------------+
| ``{`` / ``}``      | Decrease / increase colour scale left endpoint (min)   |
|                    | — stage modes only                                     |
+--------------------+--------------------------------------------------------+
| ``,`` / ``.``      | Pan the entire colour range left / right               |
|                    | — stage modes only                                     |
+--------------------+--------------------------------------------------------+
| z / Z              | Decrease / increase vertical exaggeration by 1.5×      |
+--------------------+--------------------------------------------------------+
| w                  | Cycle wireframe mode (off / water / bed / both)        |
+--------------------+--------------------------------------------------------+
| l                  | Toggle lighting                                        |
+--------------------+--------------------------------------------------------+
| t                  | Cycle view: landscape → colour (osm) → colour          |
+--------------------+--------------------------------------------------------+
| b                  | Toggle backface culling                                |
+--------------------+--------------------------------------------------------+
| c                  | Toggle steep-triangle culling                          |
+--------------------+--------------------------------------------------------+
| g                  | Cycle grid / colorbar overlay                          |
+--------------------+--------------------------------------------------------+
| i                  | Toggle information HUD                                 |
+--------------------+--------------------------------------------------------+
| Shift + arrows     | Pan camera (useful on touchpads)                       |
+--------------------+--------------------------------------------------------+
| x                  | Reset camera to default position                       |
+--------------------+--------------------------------------------------------+
| O                  | Screenshot                                             |
+--------------------+--------------------------------------------------------+
| Escape             | Quit                                                   |
+--------------------+--------------------------------------------------------+

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

Press ``v`` to cycle through water colour modes.  Use ``[`` / ``]`` to adjust
the right endpoint of the colour scale without changing mode.  In stage modes
``{`` / ``}`` move the left endpoint and ``,`` / ``.`` pan the whole range.

+----------------------+----------------------------------------------------+
| Mode                 | Description                                        |
+======================+====================================================+
| natural blue         | Solid blue water (default — no data colouring)     |
+----------------------+----------------------------------------------------+
| stage                | Current absolute water surface elevation (m)       |
+----------------------+----------------------------------------------------+
| depth                | Current water depth above the bed (m)              |
+----------------------+----------------------------------------------------+
| speed                | Current flow speed = momentum / depth (m/s)        |
+----------------------+----------------------------------------------------+
| momentum             | Current momentum magnitude (m²/s)                  |
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
extent and do not animate with the timestep.


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

Apply an image to the bedslope with ``-texture``::

   anuga_viewer -texture images/bedslope.jpg simulation.sww

If the SWW file contains UTM zone metadata, OpenStreetMap or ESRI satellite
tiles are fetched automatically and draped over the terrain.  Use
``-maptiles satellite`` to select satellite imagery or ``-maptiles none`` to
disable tile fetching.

Press ``t`` to cycle between landscape (textured) and colour (data) view modes.


Command-Line Options
--------------------

::

   anuga_viewer [options] <file.sww>

+---------------------------+------------------------------------------------+
| Option                    | Description                                    |
+===========================+================================================+
| ``-texture <file>``       | Apply image texture to bedslope                |
+---------------------------+------------------------------------------------+
| ``-maptiles osm|sat|none``| Map tile source when SWW has UTM zone          |
|                           | (default: ``osm``)                             |
+---------------------------+------------------------------------------------+
| ``-scale <float>``        | Initial vertical exaggeration (default: 1.0)   |
+---------------------------+------------------------------------------------+
| ``-tps <float>``          | Timesteps per second (default: 10)             |
+---------------------------+------------------------------------------------+
| ``-hmin <float>``         | Water depth colour scale minimum (m)           |
+---------------------------+------------------------------------------------+
| ``-hmax <float>``         | Water depth colour scale maximum (m)           |
+---------------------------+------------------------------------------------+
| ``-speedmax <float>``     | Speed colour scale maximum (m/s)               |
+---------------------------+------------------------------------------------+
| ``-momentummax <float>``  | Momentum colour scale maximum (m²/s)           |
+---------------------------+------------------------------------------------+
| ``-alphamin <float>``     | Water minimum opacity (0–1)                    |
+---------------------------+------------------------------------------------+
| ``-alphamax <float>``     | Water maximum opacity (0–1)                    |
+---------------------------+------------------------------------------------+
| ``-lightpos x,y,z``       | Directional light position (z is up)           |
+---------------------------+------------------------------------------------+
| ``-nosky``                | Disable skybox                                 |
+---------------------------+------------------------------------------------+
| ``-movie <dir>``          | Export frames to directory (use with ``.swm``) |
+---------------------------+------------------------------------------------+
| ``-- screen <n>``         | Select display screen (OSG standard)           |
+---------------------------+------------------------------------------------+


How to Make a Movie
-------------------

1. Press ``1`` to start recording the camera path, ``1`` again to stop.
   Press ``3`` to save the macro as ``movie.swm``.

2. Export the macro as JPEG frames::

      anuga_viewer -movie myframes movie.swm

3. Stitch frames into a video with ffmpeg::

      ffmpeg -framerate 25 -i myframes/frame_%d_%d.jpg output.mp4


Troubleshooting
---------------

**The viewer opens on the wrong monitor.**
  Pass ``-- screen 0`` (or ``1``) on the command line::

     anuga_viewer -- screen 0 simulation.sww

**TIF textures fail to load.**
  Re-save the file in GIMP or another image editor.  Very large textures
  (> 4096 × 4096) may also exceed GPU limits — try resizing first.

**The viewer crashes with a GL / segfault error on WSL2.**
  Make sure you are using the system OpenSceneGraph (``libopenscenegraph-dev``)
  and not one installed inside a conda environment.  Conda's ``libGLX.so``
  conflicts with the WSL2 Intel GPU driver.
