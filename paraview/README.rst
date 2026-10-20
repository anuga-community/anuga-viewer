ParaView reader for SWW files
=============================

``sww_reader.py`` is a ParaView plugin that opens ANUGA ``.sww`` files.  It
is a prototype.

Requirements
------------

ParaView 6.2 (the version tested; older 5.x releases may work but are
untested), and in ParaView's Python either ``netCDF4`` or ``scipy``.  The
conda-forge build of ParaView makes this easy::

    conda create -n paraview -c conda-forge paraview netcdf4

Quick start
-----------

``anuga-paraview`` starts ParaView with the reader loaded, and given a
file, opens it with a ready-made view::

    paraview/anuga-paraview            # open .sww files with File > Open
    paraview/anuga-paraview run.sww

The bed is coloured by elevation and the water by depth, with water under
5 cm faded out so the terrain shows through, and the depth colour range
fixed over the run.  A time label sits in the corner; press Play on the
animation toolbar.  The setup is in ``scripts/view_sww.py``: to apply it
to a ``.sww`` opened with File > Open, select the file in the Pipeline
Browser and run the script from View > Python Shell > Run Script, or add
it once as a toolbar button with Macros > Import new macro.

The launcher uses the ``paraview`` on the PATH, or ``$PARAVIEW``, and runs it
on X11 when the session is Wayland.

Loading
-------

In the ParaView GUI, use Tools > Manage Plugins > Load New... and pick
``sww_reader.py``.  Tick *Auto Load* to have it every session.  ``.sww``
files then open with File > Open like any other format.

From ``pvpython``::

    from paraview.simple import *
    LoadPlugin('/path/to/anuga-viewer/paraview/sww_reader.py', ns=globals())
    sww = OpenDataFile('run.sww')

To read files on a remote machine with ``pvserver``, load the plugin on both
the client and the server (the Manage Plugins dialog has a list for each),
and install ``netCDF4`` in the server's Python.

Outputs
-------

The reader has two outputs, matching ``anuga_viewer``:

``Bed``
    The terrain, with z = elevation.
``Water``
    The water surface, with z = stage.  Triangles whose three vertices are
    all shallower than 1 mm are left out, unless *ShowDryWater* is on.

Both carry the same arrays at each timestep:

- every per-point variable in the file (``stage``, ``elevation``,
  ``xmomentum``, ``friction``, ...) as point data;
- every per-triangle variable (``stage_c``, ``max_depth_c``, ...) as cell
  data;
- derived ``depth``, and where momentum is present ``momentum``, ``speed``
  and a ``velocity`` vector, with speed zero where depth is below 1 mm.
  The same are derived per triangle (``depth_c``, ``speed_c``, ...) when the
  file has ``stage_c`` and ``elevation_c``.

Properties
----------

``Point Arrays`` / ``Cell Arrays``
    Shown with the gear (advanced) button at the top of the Properties
    panel.  The arrays the file offers, per point and per triangle, all
    ticked to begin with.  Unticked arrays are not read, except where a
    ticked derived quantity needs them (``speed`` reads both momentum
    components).  Elevation and stage are always read, since the surfaces
    are built from them.
``VerticalScale``
    Vertical exaggeration of both surfaces, like ``anuga_viewer -scale``.
``Georeferenced``
    Add the file's ``xllcorner``/``yllcorner``, giving projected (e.g. UTM)
    coordinates rather than the local ones stored in the file.
``ShowDryWater``
    Keep dry triangles in the ``Water`` output.
``ComputeMaxima``
    Add ``max_stage``, ``max_depth``, ``max_speed`` and ``max_momentum`` over
    all timesteps.  This reads the whole file once.

``Bed`` is coloured by ``elevation`` and ``Water`` by ``depth`` to begin
with; ``speed`` is often more telling.  ParaView fixes the colour range when
you press Apply, which is usually the dry first timestep, so press *Rescale
to data range over all timesteps* on the colour toolbar before playing the
animation.

On Linux under Wayland, if ParaView hangs at startup, run it on X11::

    QT_QPA_PLATFORM=xcb paraview

If it still hangs, add ``LIBGL_ALWAYS_SOFTWARE=1`` to use software OpenGL.

Setting ``PV_PLUGIN_PATH`` to this directory loads the reader without the
Manage Plugins dialog::

    PV_PLUGIN_PATH=/path/to/anuga-viewer/paraview paraview run.sww

Timeseries at a point
---------------------

The plugin also adds an **ANUGA Timeseries** filter.  It reads the values
at one location straight from the file, so it is instant even for long
runs, where ParaView's Plot Selection Over Time re-reads every timestep in
full.

1. Select ``Bed`` (or ``Water``) in the Pipeline Browser.
2. Add the filter: Filters > ANUGA > ANUGA Timeseries, or Ctrl+Space and
   type "ANUGA".
3. Click Apply.  A line chart of depth and speed against time opens beside
   the 3D view, for the middle of the domain.
4. Move the location: click in the 3D view, hover over the spot and press
   **P**, or drag the marker, or type coordinates into *Location*.  Then
   click Apply.  For the chart to follow the marker without Apply, tick
   *Auto Apply* in Edit > Settings > General (under Properties Panel
   Options).
5. Tick other quantities under *Quantities*: stage, elevation, momentum
   and its components.

Values are interpolated from the corners of the triangle containing the
location, as ANUGA's own gauge tools do.  Only x and y matter.  File >
Save Data writes the chart's table as CSV.

Testing
-------

From the repository root::

    pvpython paraview/tests/test_sww_reader.py

This checks the reader against every file in ``data/``, comparing the
geometry and arrays with values read directly from the file.
