Installation
============


Linux — AppImage (recommended)
-------------------------------

The easiest way to install ANUGA Viewer on Linux is to download the pre-built
AppImage from the
`Releases <https://github.com/anuga-community/anuga-viewer/releases>`_ page.
An AppImage is a self-contained executable — no installation or root access is
required.

.. code-block:: bash

   # Download the AppImage (replace x.y with the version number)
   wget https://github.com/anuga-community/anuga-viewer/releases/latest/download/ANUGA_Viewer-x86_64.AppImage

   # Make it executable
   chmod +x ANUGA_Viewer-x86_64.AppImage

   # Run directly
   ./ANUGA_Viewer-x86_64.AppImage path/to/simulation.sww

   # Or install to your PATH
   cp ANUGA_Viewer-x86_64.AppImage ~/.local/bin/anuga_viewer
   anuga_viewer path/to/simulation.sww

.. note::

   On WSL2 (Windows Subsystem for Linux), the AppImage requires WSLg for
   display support, which is included in Windows 11 and recent Windows 10
   builds.


Linux — Build from Source (Ubuntu / Debian)
--------------------------------------------

#. Install the required packages::

      sudo apt-get install git build-essential libgdal-dev libcppunit-dev libopenscenegraph-dev libnetcdf-dev

#. Clone the source::

      git clone https://github.com/anuga-community/anuga-viewer.git
      cd anuga-viewer

   .. note::

      If you have an active conda environment that provides compilers, deactivate
      it before building — conda's compilers can shadow system headers and
      produce libraries incompatible with the system OpenSceneGraph::

         conda deactivate

#. Build and install::

      make
      sudo make install

#. Add the viewer's bin directory to your shell environment by appending the
   following to ``~/.bashrc``::

      export SWOLLEN_BINDIR=/path/to/anuga-viewer/bin
      export PATH=$PATH:$SWOLLEN_BINDIR

   Then reload::

      source ~/.bashrc

#. Test the install::

      anuga_viewer ~/anuga-viewer/data/cairns.sww

   Press Escape to exit.


Windows — Pre-compiled Installer
---------------------------------

A pre-compiled Windows build is available on Sourceforge and works on
Windows 10 and 11:

`Download anuga_viewer for Windows <https://sourceforge.net/projects/anuga/files/anuga_viewer_windows/>`_

Download ``anuga_viewer.zip``, extract it, and run ``bin/viewer.exe``.
You can associate ``.sww`` files with ``viewer.exe`` for double-click
opening.

.. note::

   A modern Windows build using GitHub Actions CI is under development.
   Check the
   `Releases <https://github.com/anuga-community/anuga-viewer/releases>`_
   page for updated packages.


Windows — Build from Source
----------------------------

Building from source on Windows is not actively maintained.  The pre-compiled
package above is the recommended option.  If you need to build from source,
the general requirements are:

- Visual Studio 2019 or later
- `OpenSceneGraph <https://www.openscenegraph.com>`_ (3.6.x recommended)
- `NetCDF for Windows <https://www.unidata.ucar.edu/software/netcdf/>`_
- `GDAL <https://gdal.org>`_

Set environment variables ``OSG_ROOT``, ``NETCDF_DIR``, and ``GDAL_DIR``
to the respective installation roots, then open and build the Visual Studio
solution in the ``viewer/visualstudio`` subdirectory.
