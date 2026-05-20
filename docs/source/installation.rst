Installation
============

Pre-built packages for Linux, macOS, and Windows are available on the
`GitHub Releases page <https://github.com/anuga-community/anuga-viewer/releases>`_.


Linux — AppImage
-----------------

Download ``anuga-viewer-linux-x86_64.AppImage`` from the
`Releases page <https://github.com/anuga-community/anuga-viewer/releases>`_,
make it executable, and run it::

   chmod +x anuga-viewer-linux-x86_64.AppImage

   # Run directly
   ./anuga-viewer-linux-x86_64.AppImage path/to/simulation.sww

   # Or install to your PATH (no root required)
   mkdir -p ~/.local/bin
   cp anuga-viewer-linux-x86_64.AppImage ~/.local/bin/anuga_viewer
   anuga_viewer path/to/simulation.sww

.. note::

   On WSL2 (Windows Subsystem for Linux), the AppImage requires WSLg for
   display support, which is included in Windows 11 and recent Windows 10
   builds.


macOS
-----

Download ``anuga-viewer-macos.dmg`` from the
`Releases page <https://github.com/anuga-community/anuga-viewer/releases>`_.

1. Open the ``.dmg`` file.
2. Drag **ANUGA Viewer** to your **Applications** folder.
3. Open it from Applications or run from the terminal::

      anuga_viewer path/to/simulation.sww

.. note::

   On first launch macOS may show a security warning because the app is not
   signed through the App Store.  Right-click the app and choose **Open** to
   bypass this, or go to **System Settings → Privacy & Security** and click
   **Open Anyway**.


Windows
-------

Download ``anuga-viewer-windows.zip`` from the
`Releases page <https://github.com/anuga-community/anuga-viewer/releases>`_.

1. Extract the zip to a convenient location (e.g. ``C:\anuga-viewer``).
2. Run ``anuga_viewer.exe`` directly, or double-click ``run_viewer.bat``.

To open a simulation file::

   anuga_viewer.exe path\to\simulation.sww

You can also associate ``.sww`` files with ``anuga_viewer.exe`` for
double-click opening in Windows Explorer.


Linux — Build from Source (Ubuntu / Debian)
--------------------------------------------

If you need to build from source (e.g. to modify the code or for an
unsupported architecture):

#. Install the required packages::

      sudo apt-get install git build-essential libgdal-dev libcppunit-dev \
                           libopenscenegraph-dev libnetcdf-dev

#. Clone the source::

      git clone https://github.com/anuga-community/anuga-viewer.git
      cd anuga-viewer

   .. note::

      If you have an active conda environment that provides compilers,
      deactivate it before building — conda's compilers can shadow system
      headers and produce libraries incompatible with the system
      OpenSceneGraph::

         conda deactivate

#. Build and install::

      make
      sudo make install

#. Add the viewer's bin directory to your shell environment by appending
   the following to ``~/.bashrc``::

      export SWOLLEN_BINDIR=/path/to/anuga-viewer/bin
      export PATH=$PATH:$SWOLLEN_BINDIR

   Then reload::

      source ~/.bashrc

#. Test the install::

      anuga_viewer ~/anuga-viewer/data/cairns.sww

   Press Escape to exit.
