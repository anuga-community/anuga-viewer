"""ParaView reader for ANUGA .sww files.

Load it in ParaView with Tools > Manage Plugins > Load New..., then open a
.sww file as usual.  From pvpython:

    from paraview.simple import *
    LoadPlugin('/path/to/sww_reader.py', ns=globals())
    sww = ANUGASWWReader(FileName='run.sww')

The reader has two outputs, matching anuga_viewer:

    0  Bed    terrain, z = elevation
    1  Water  water surface, z = stage, with dry triangles removed

Both carry every per-point and per-triangle variable in the file, plus the
derived depth, momentum, speed and velocity.

The plugin also provides the ANUGA Timeseries filter: apply it to either
output to plot quantities over time at a location, read directly from the
file.  Point variables become point
data; per-triangle variables (the *_c ones written by newer ANUGA) become
cell data under the same name.

Reading needs the netCDF4 Python module, or failing that scipy, which reads
the NetCDF3 files ANUGA writes.
"""

import weakref

import numpy as np

from vtkmodules.util.vtkAlgorithm import VTKPythonAlgorithmBase
from vtkmodules.util import numpy_support
from vtkmodules.vtkCommonCore import vtkDataArraySelection, vtkPoints, vtkStringArray
from vtkmodules.vtkCommonDataModel import vtkCellArray, vtkPolyData, vtkTable
from vtkmodules.vtkCommonExecutionModel import vtkStreamingDemandDrivenPipeline

try:
    from paraview.util.vtkAlgorithm import smproxy, smproperty, smdomain, smhint
except ImportError:
    # Outside ParaView (plain VTK) the decorators only describe the GUI, so
    # stand them in with no-ops and the class works as an ordinary algorithm.
    class _NoDecorators(object):
        def __getattr__(self, name):
            return lambda *args, **kwargs: (lambda f: f)
    smproxy = smproperty = smdomain = smhint = _NoDecorators()


# Depth (m) below which water is treated as dry, as in anuga_viewer.
DRY_DEPTH = 0.001


def _open_netcdf(filename):
    """Return (variables, global attributes) for a NetCDF file."""
    try:
        import netCDF4
        ds = netCDF4.Dataset(filename, 'r')
        ds.set_auto_mask(False)
        attrs = dict((k, ds.getncattr(k)) for k in ds.ncattrs())
    except ImportError:
        from scipy.io import netcdf_file
        ds = netcdf_file(filename, 'r', mmap=False)
        attrs = dict(ds._attributes)
    return ds, attrs


class SWWFile(object):
    """The mesh and variables of an .sww file, read one timestep at a time."""

    def __init__(self, filename):
        self.filename = filename
        self.ds, self.attrs = _open_netcdf(filename)
        var = self.ds.variables

        self.x = np.asarray(var['x'][:], dtype=np.float64)
        self.y = np.asarray(var['y'][:], dtype=np.float64)
        self.triangles = np.asarray(var['volumes'][:], dtype=np.int64)
        self.time = np.atleast_1d(np.asarray(var['time'][:], dtype=np.float64))
        self.xllcorner = float(self.attrs.get('xllcorner', 0.0))
        self.yllcorner = float(self.attrs.get('yllcorner', 0.0))

        npoints, ntris = len(self.x), len(self.triangles)

        # Sort the remaining variables by what they are defined on, and
        # whether they change with time.  Each entry maps name -> animated.
        self.point_vars, self.cell_vars = {}, {}
        for name, v in var.items():
            if name in ('x', 'y', 'volumes', 'time') or name.endswith('_range'):
                continue
            shape = v.shape
            animated = len(shape) == 2 and shape[0] == len(self.time)
            n = shape[-1] if shape else 0
            if len(shape) not in (1, 2) or (len(shape) == 2 and not animated):
                continue
            if n == npoints:
                self.point_vars[name] = animated
            elif n == ntris:
                self.cell_vars[name] = animated

        # Older files call the bed 'z' rather than 'elevation'.
        self.bed = 'elevation' if 'elevation' in self.point_vars else 'z'
        if self.bed not in self.point_vars or 'stage' not in self.point_vars:
            raise ValueError('%s: not an .sww file (no elevation or stage)' % filename)

    def close(self):
        self.ds.close()

    def _read(self, name, animated, index):
        v = self.ds.variables[name]
        return np.asarray(v[index] if animated else v[:], dtype=np.float32)

    @staticmethod
    def _derived_names(names, suffix=''):
        """The quantities _derive can add given the variables in names."""
        if 'stage' + suffix not in names or 'elevation' + suffix not in names:
            return []
        derived = ['depth' + suffix]
        if 'xmomentum' + suffix in names and 'ymomentum' + suffix in names:
            derived += [n + suffix for n in ('momentum', 'speed', 'velocity')]
        return derived

    @staticmethod
    def _inputs(wanted, suffix=''):
        """The file variables needed to derive the wanted quantities."""
        needed = set()
        if wanted & set(n + suffix for n in ('depth', 'momentum', 'speed', 'velocity')):
            needed |= set(['stage' + suffix, 'elevation' + suffix])
        if wanted & set(n + suffix for n in ('momentum', 'speed', 'velocity')):
            needed |= set(['xmomentum' + suffix, 'ymomentum' + suffix])
        return needed

    def point_names(self):
        """Point arrays available: the file's variables, then derived ones."""
        names = sorted('elevation' if n == self.bed else n for n in self.point_vars)
        return names + self._derived_names(names)

    def cell_names(self):
        """Per-triangle arrays available: the file's variables, then derived ones."""
        names = sorted(self.cell_vars)
        return names + self._derived_names(names, '_c')

    def point_data(self, index, wanted=None):
        """Point arrays at a timestep: those wanted (default all), plus the
        elevation, stage and depth the surfaces are built from."""
        wanted = set(self.point_names() if wanted is None else wanted)
        wanted |= set(['elevation', 'stage', 'depth'])
        read = wanted | self._inputs(wanted)
        data = {}
        for name, animated in self.point_vars.items():
            key = 'elevation' if name == self.bed else name
            if key in read:
                data[key] = self._read(name, animated, index)
        self._derive(data)
        return dict((n, a) for n, a in data.items() if n in wanted)

    def cell_data(self, index, wanted=None):
        """Per-triangle arrays at a timestep: those wanted (default all)."""
        wanted = set(self.cell_names() if wanted is None else wanted)
        read = wanted | self._inputs(wanted, '_c')
        data = dict((n, self._read(n, a, index))
                    for n, a in self.cell_vars.items() if n in read)
        if 'stage_c' in data and 'elevation_c' in data:
            self._derive(data, suffix='_c')
        return dict((n, a) for n, a in data.items() if n in wanted)

    @staticmethod
    def _derive(data, suffix=''):
        stage, bed = data['stage' + suffix], data['elevation' + suffix]
        depth = np.maximum(stage - bed, 0.0)
        data['depth' + suffix] = depth
        uh, vh = data.get('xmomentum' + suffix), data.get('ymomentum' + suffix)
        if uh is not None and vh is not None:
            data['momentum' + suffix] = np.hypot(uh, vh)
            wet = depth > DRY_DEPTH
            safe = np.where(wet, depth, 1.0)
            u = np.where(wet, uh / safe, 0.0).astype(np.float32)
            v = np.where(wet, vh / safe, 0.0).astype(np.float32)
            data['speed' + suffix] = np.hypot(u, v)
            data['velocity' + suffix] = np.column_stack((u, v, np.zeros_like(u)))

    def locate(self, x, y):
        """The corners of the triangle containing (x, y), with the weights
        that interpolate their values to it, or None outside the mesh."""
        if not hasattr(self, '_corners'):
            tri = self.triangles
            self._corners = [(self.x[tri[:, k]], self.y[tri[:, k]]) for k in range(3)]
        (ax, ay), (bx, by), (cx, cy) = self._corners
        with np.errstate(divide='ignore', invalid='ignore'):
            det = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
            w1 = ((by - cy) * (x - cx) + (cx - bx) * (y - cy)) / det
            w2 = ((cy - ay) * (x - cx) + (ax - cx) * (y - cy)) / det
        w3 = 1.0 - w1 - w2
        eps = -1e-9
        inside = np.flatnonzero((w1 >= eps) & (w2 >= eps) & (w3 >= eps))
        if len(inside) == 0:
            return None
        k = inside[0]
        return self.triangles[k], np.array([w1[k], w2[k], w3[k]])

    def timeseries(self, x, y, wanted):
        """Values over time at (x, y), interpolated within the triangle
        containing it, for the wanted point arrays; None outside the mesh.

        Only the vertices of that triangle are read, so this is quick even
        for a long run."""
        found = self.locate(x, y)
        if found is None:
            return None
        corners, weights = found
        order = np.argsort(corners)         # netCDF wants increasing indices
        corners, weights = corners[order], weights[order]

        wanted = set(wanted)
        read = wanted | self._inputs(wanted)
        data = {}
        for name, animated in self.point_vars.items():
            key = 'elevation' if name == self.bed else name
            if key not in read:
                continue
            v = self.ds.variables[name]
            values = np.asarray(v[:, corners] if animated else v[:][corners], dtype=np.float64)
            series = values @ weights if animated else np.full(len(self.time), values @ weights)
            data[key] = series.astype(np.float32)
        if 'stage' in data and 'elevation' in data:
            self._derive(data)
        return dict((n, a) for n, a in data.items() if n in wanted)

    def maxima(self):
        """Point-wise maxima over all timesteps of stage, depth, speed, momentum."""
        result = {}
        for index in range(len(self.time)):
            data = self.point_data(index, ('speed', 'momentum'))
            for name in ('stage', 'depth', 'speed', 'momentum'):
                if name in data:
                    key = 'max_' + name
                    result[key] = data[name] if key not in result \
                        else np.maximum(result[key], data[name])
        return result


def _modified_callback(algorithm):
    """An observer that marks algorithm modified, without keeping it alive."""
    ref = weakref.ref(algorithm)

    def callback(*args):
        alg = ref()
        if alg is not None:
            alg.Modified()
    return callback


def _enabled(selection):
    return set(selection.GetArrayName(i) for i in range(selection.GetNumberOfArrays())
               if selection.GetArraySetting(i))


def _add_file_info(poly, filename, xoffset, yoffset):
    """Field data telling ANUGATimeseries which file and coordinates these are."""
    name = vtkStringArray()
    name.SetName('SWWFileName')
    name.InsertNextValue(filename)
    poly.GetFieldData().AddArray(name)
    poly.GetFieldData().AddArray(_vtk_array('SWWOffset', np.array([xoffset, yoffset])))


def _vtk_array(name, values):
    array = numpy_support.numpy_to_vtk(np.ascontiguousarray(values), deep=1)
    array.SetName(name)
    return array


def _surface(x, y, z, triangles, point_data, cell_data, scalars, keep=None):
    """A vtkPolyData of triangles coloured by scalars by default, optionally
    with only the triangles where keep is True."""
    if keep is not None:
        triangles = triangles[keep]
        cell_data = dict((n, a[keep]) for n, a in cell_data.items())

    points = vtkPoints()
    points.SetData(_vtk_array('Points', np.column_stack((x, y, z))))

    ntris = len(triangles)
    offsets = np.arange(0, 3 * ntris + 1, 3, dtype=np.int64)
    cells = vtkCellArray()
    cells.SetData(numpy_support.numpy_to_vtkIdTypeArray(offsets, deep=1),
                  numpy_support.numpy_to_vtkIdTypeArray(triangles.ravel(), deep=1))

    poly = vtkPolyData()
    poly.SetPoints(points)
    poly.SetPolys(cells)
    for name, values in point_data.items():
        poly.GetPointData().AddArray(_vtk_array(name, values))
    for name, values in cell_data.items():
        poly.GetCellData().AddArray(_vtk_array(name, values))
    if scalars in point_data:
        poly.GetPointData().SetActiveScalars(scalars)
    return poly


def _array_selection_xml(name, command):
    """Proxy XML for an array selection list, as @smproperty.dataarrayselection
    generates, but shown only with the Properties panel's advanced (gear)
    toggle: every array is wanted by default, so the list is rarely needed."""
    return """
        <StringVectorProperty name="{name}Info" command="{command}"
                              number_of_elements_per_command="2"
                              information_only="1"
                              si_class="vtkSIDataArraySelectionProperty" />
        <StringVectorProperty name="{name}" information_property="{name}Info"
                              command="{command}"
                              number_of_elements_per_command="2"
                              element_types="2 0" repeat_command="1"
                              si_class="vtkSIDataArraySelectionProperty"
                              panel_visibility="advanced">
          <ArraySelectionDomain name="array_list">
            <RequiredProperties>
              <Property function="ArrayList" name="{name}Info" />
            </RequiredProperties>
          </ArraySelectionDomain>
        </StringVectorProperty>""".format(name=name, command=command)


@smproxy.reader(name="ANUGASWWReader", label="ANUGA SWW Reader",
                extensions="sww", file_description="ANUGA SWW files")
@smproperty.xml('<OutputPort name="Bed" index="0" />'
                 '<OutputPort name="Water" index="1" />')
class ANUGASWWReader(VTKPythonAlgorithmBase):
    """Reads an ANUGA .sww file into Bed and Water surfaces over time."""

    def __init__(self):
        VTKPythonAlgorithmBase.__init__(self, nInputPorts=0, nOutputPorts=2,
                                        outputType='vtkPolyData')
        self._filename = None
        self._sww = None
        self._maxima = None
        self._vscale = 1.0
        self._georeferenced = False
        self._compute_maxima = False
        self._show_dry = False

        # The arrays offered in the GUI, all ticked to begin with.
        self._point_arrays = vtkDataArraySelection()
        self._cell_arrays = vtkDataArraySelection()
        for selection in (self._point_arrays, self._cell_arrays):
            selection.AddObserver('ModifiedEvent', _modified_callback(self))

    def _file(self):
        if self._sww is None and self._filename:
            self._sww = SWWFile(self._filename)
            for selection, names in ((self._point_arrays, self._sww.point_names()),
                                     (self._cell_arrays, self._sww.cell_names())):
                selection.RemoveAllArrays()
                for name in names:
                    selection.AddArray(name)
        return self._sww

    @smproperty.stringvector(name="FileName")
    @smdomain.filelist()
    @smhint.filechooser(extensions="sww", file_description="ANUGA SWW files")
    def SetFileName(self, name):
        if name != self._filename:
            if self._sww is not None:
                self._sww.close()
            self._filename, self._sww, self._maxima = name, None, None
            self.Modified()

    @smproperty.doublevector(name="TimestepValues", information_only="1",
                             si_class="vtkSITimeStepsProperty")
    def GetTimestepValues(self):
        sww = self._file()
        return sww.time.tolist() if sww is not None else None

    @smproperty.xml(_array_selection_xml("PointArrays", "GetPointArraySelection"))
    def GetPointArraySelection(self):
        """Per-point arrays to read: the file's variables and derived ones."""
        return self._point_arrays

    @smproperty.xml(_array_selection_xml("CellArrays", "GetCellArraySelection"))
    def GetCellArraySelection(self):
        """Per-triangle arrays to read (the *_c variables)."""
        return self._cell_arrays

    @smproperty.doublevector(name="VerticalScale", default_values=1.0)
    @smdomain.doublerange(min=0.1, max=100.0)
    def SetVerticalScale(self, value):
        """Vertical exaggeration of both surfaces, like anuga_viewer's -scale."""
        if value != self._vscale:
            self._vscale = value
            self.Modified()

    @smproperty.xml("""
        <IntVectorProperty name="Georeferenced" command="SetGeoreferenced"
                           number_of_elements="1" default_values="0">
          <BooleanDomain name="bool" />
          <Documentation>Add the file's xllcorner/yllcorner, giving
          projected (e.g. UTM) coordinates instead of local ones.</Documentation>
        </IntVectorProperty>""")
    def SetGeoreferenced(self, value):
        if bool(value) != self._georeferenced:
            self._georeferenced = bool(value)
            self.Modified()

    @smproperty.xml("""
        <IntVectorProperty name="ShowDryWater" command="SetShowDryWater"
                           number_of_elements="1" default_values="0">
          <BooleanDomain name="bool" />
          <Documentation>Keep dry triangles in the Water output instead of
          removing those whose vertices are all shallower than 1 mm.</Documentation>
        </IntVectorProperty>""")
    def SetShowDryWater(self, value):
        if bool(value) != self._show_dry:
            self._show_dry = bool(value)
            self.Modified()

    @smproperty.xml("""
        <IntVectorProperty name="ComputeMaxima" command="SetComputeMaxima"
                           number_of_elements="1" default_values="0">
          <BooleanDomain name="bool" />
          <Documentation>Add max_stage, max_depth, max_speed and max_momentum
          over all timesteps.  Reads the whole file once.</Documentation>
        </IntVectorProperty>""")
    def SetComputeMaxima(self, value):
        if bool(value) != self._compute_maxima:
            self._compute_maxima = bool(value)
            self.Modified()

    def RequestInformation(self, request, inInfo, outInfo):
        sww = self._file()
        if sww is None:
            return 1
        executive = vtkStreamingDemandDrivenPipeline
        for port in range(self.GetNumberOfOutputPorts()):
            info = outInfo.GetInformationObject(port)
            info.Remove(executive.TIME_STEPS())
            info.Remove(executive.TIME_RANGE())
            for t in sww.time:
                info.Append(executive.TIME_STEPS(), t)
            info.Append(executive.TIME_RANGE(), sww.time[0])
            info.Append(executive.TIME_RANGE(), sww.time[-1])
        return 1

    def _timestep(self, info):
        sww = self._sww
        executive = vtkStreamingDemandDrivenPipeline
        if not info.Has(executive.UPDATE_TIME_STEP()):
            return 0
        t = info.Get(executive.UPDATE_TIME_STEP())
        return int(np.argmin(np.abs(sww.time - t)))

    def RequestData(self, request, inInfo, outInfo):
        sww = self._file()
        if sww is None:
            return 0

        index = self._timestep(outInfo.GetInformationObject(0))
        points_wanted = _enabled(self._point_arrays)
        # point_data adds what the surfaces are built from; keep those for
        # the geometry, but only output the arrays that are ticked.
        surface_data = sww.point_data(index, points_wanted)
        point_data = dict((n, a) for n, a in surface_data.items() if n in points_wanted)
        cell_data = sww.cell_data(index, _enabled(self._cell_arrays))
        if self._compute_maxima:
            if self._maxima is None:
                self._maxima = sww.maxima()
            point_data.update(self._maxima)

        xoffset, yoffset = (sww.xllcorner, sww.yllcorner) if self._georeferenced else (0.0, 0.0)
        x, y = sww.x + xoffset, sww.y + yoffset
        scale = self._vscale

        bed = _surface(x, y, surface_data['elevation'] * scale, sww.triangles,
                       point_data, cell_data, 'elevation')

        keep = None
        if not self._show_dry:
            keep = (surface_data['depth'][sww.triangles] > DRY_DEPTH).any(axis=1)
        water = _surface(x, y, surface_data['stage'] * scale, sww.triangles,
                         point_data, cell_data, 'depth', keep)

        time = sww.time[index]
        for port, poly in enumerate((bed, water)):
            _add_file_info(poly, self._filename, xoffset, yoffset)
            out = vtkPolyData.GetData(outInfo, port)
            out.ShallowCopy(poly)
            out.GetInformation().Set(out.DATA_TIME_STEP(), time)
        return 1


# Quantities ANUGATimeseries offers, and those ticked to begin with.
TIMESERIES_QUANTITIES = ('depth', 'speed', 'stage', 'elevation', 'momentum',
                         'xmomentum', 'ymomentum')
TIMESERIES_DEFAULT = ('depth', 'speed')


@smproxy.filter(name="ANUGATimeseries", label="ANUGA Timeseries")
@smhint.xml('<ShowInMenu category="ANUGA" />')
@smhint.xml('<View type="XYChartView" />')
@smproperty.input(name="Input", port_index=0)
@smdomain.datatype(dataTypes=["vtkPolyData"], composite_data_supported=False)
class ANUGATimeseries(VTKPythonAlgorithmBase):
    """Timeseries at a location, read straight from the .sww file.

    Apply it to the Bed or Water output of the reader, then move the marker
    (or press P with the mouse over the surface).  Unlike Plot Selection
    Over Time, which runs the whole reader at every timestep, this reads
    only the three vertices around the location."""

    def __init__(self):
        VTKPythonAlgorithmBase.__init__(self, nInputPorts=1, nOutputPorts=1,
                                        outputType='vtkTable')
        self._location = (0.0, 0.0, 0.0)
        self._sww = None
        self._quantities = vtkDataArraySelection()
        for name in TIMESERIES_QUANTITIES:
            self._quantities.AddArray(name, name in TIMESERIES_DEFAULT)
        self._quantities.AddObserver('ModifiedEvent', _modified_callback(self))

    def FillInputPortInformation(self, port, info):
        info.Set(self.INPUT_REQUIRED_DATA_TYPE(), 'vtkPolyData')
        return 1

    # The property and the draggable marker that sets it, starting at the
    # middle of the input.  The group must follow the properties it names.
    @smproperty.xml("""
        <DoubleVectorProperty name="Location" command="SetLocation"
                              number_of_elements="3" default_values="0 0 0">
          <BoundsDomain name="range" mode="normal" default_mode="mid">
            <RequiredProperties>
              <Property function="Input" name="Input" />
            </RequiredProperties>
          </BoundsDomain>
          <Documentation>Where to read the timeseries; only x and y
          matter.  Drag the marker, or press P over the surface.</Documentation>
        </DoubleVectorProperty>
        <PropertyGroup label="Location" panel_widget="InteractiveHandle">
          <Property function="WorldPosition" name="Location" />
          <Property function="Input" name="Input" />
        </PropertyGroup>""")
    def SetLocation(self, x, y, z):
        """Where to read the timeseries; only x and y matter."""
        if (x, y, z) != self._location:
            self._location = (x, y, z)
            self.Modified()

    @smproperty.dataarrayselection(name="Quantities")
    def GetQuantitySelection(self):
        return self._quantities

    def RequestInformation(self, request, inInfo, outInfo):
        # The table covers the whole run, so it does not change with time.
        executive = vtkStreamingDemandDrivenPipeline
        info = outInfo.GetInformationObject(0)
        info.Remove(executive.TIME_STEPS())
        info.Remove(executive.TIME_RANGE())
        return 1

    def RequestData(self, request, inInfo, outInfo):
        source = vtkPolyData.GetData(inInfo[0])
        fields = source.GetFieldData()
        name = fields.GetAbstractArray('SWWFileName')
        if name is None:
            print('ANUGA Timeseries: apply it to the Bed or Water output of an .sww file')
            return 0
        filename = name.GetValue(0)
        if self._sww is None or self._sww.filename != filename:
            if self._sww is not None:
                self._sww.close()
            self._sww = SWWFile(filename)
        xoffset, yoffset = numpy_support.vtk_to_numpy(fields.GetArray('SWWOffset'))

        x, y = self._location[0] - xoffset, self._location[1] - yoffset
        series = self._sww.timeseries(x, y, _enabled(self._quantities))
        table = vtkTable.GetData(outInfo)
        table.AddColumn(_vtk_array('Time', self._sww.time))
        if series is None:
            print('ANUGA Timeseries: (%g, %g) is outside the mesh' % tuple(self._location[:2]))
            return 1
        for quantity in TIMESERIES_QUANTITIES:
            if quantity in series:
                table.AddColumn(_vtk_array(quantity, series[quantity]))
        return 1
