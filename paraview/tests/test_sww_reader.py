"""Checks the ParaView SWW reader against the sample files.

Run with ParaView's Python, from the repository root:

    pvpython paraview/tests/test_sww_reader.py [file.sww ...]

With no arguments it reads every .sww in data/.
"""

import glob
import os
import sys

import numpy as np
import netCDF4
from paraview import simple
from paraview import servermanager
from vtkmodules.util.numpy_support import vtk_to_numpy

# The test lives in its own directory because ParaView loads every .py file
# in a PV_PLUGIN_PATH directory as a plugin.
HERE = os.path.dirname(os.path.abspath(__file__))
PLUGIN_DIR = os.path.dirname(HERE)
simple.LoadPlugin(os.path.join(PLUGIN_DIR, 'sww_reader.py'), ns=globals())
sys.path.insert(0, PLUGIN_DIR)
import sww_reader  # noqa: E402  (for SWWFile, checked directly below)


def fetch(reader, port, time):
    reader.UpdatePipeline(time)
    return servermanager.Fetch(reader, idx=port)


def array(data, name, cells=False):
    arrays = data.GetCellData() if cells else data.GetPointData()
    a = arrays.GetArray(name)
    assert a is not None, 'missing array %s' % name
    return vtk_to_numpy(a)


def check(filename):
    nc = netCDF4.Dataset(filename)
    nc.set_auto_mask(False)
    times = np.atleast_1d(nc['time'][:])
    bedname = 'elevation' if 'elevation' in nc.variables else 'z'
    ntris, npoints = nc['volumes'].shape[0], nc['x'].shape[0]

    reader = ANUGASWWReader(FileName=filename)
    reader.UpdatePipelineInformation()
    assert np.allclose(reader.TimestepValues, times), 'timesteps differ'

    # A timestep part way through, so stage differs from the initial state.
    index = len(times) * 2 // 3
    t = float(times[index])
    bed = fetch(reader, 0, t)
    water = fetch(reader, 1, t)

    assert bed.GetNumberOfPoints() == npoints
    assert bed.GetNumberOfCells() == ntris
    assert water.GetNumberOfCells() <= ntris

    stage = nc['stage'][index]
    elev = nc[bedname][index] if nc[bedname].ndim == 2 else nc[bedname][:]
    pts = vtk_to_numpy(bed.GetPoints().GetData())
    assert np.allclose(pts[:, 0], nc['x'][:]) and np.allclose(pts[:, 1], nc['y'][:])
    assert np.allclose(pts[:, 2], elev), 'bed z is not elevation'
    assert np.allclose(vtk_to_numpy(water.GetPoints().GetData())[:, 2], stage), \
        'water z is not stage'
    assert np.allclose(array(bed, 'stage'), stage)
    depth = array(bed, 'depth')
    assert np.allclose(depth, np.maximum(stage - elev, 0), atol=1e-5)

    names = ['elevation', 'stage', 'depth']
    if 'xmomentum' in nc.variables:
        uh, vh = nc['xmomentum'][index], nc['ymomentum'][index]
        speed = array(bed, 'speed')
        wet = depth > 0.001
        assert np.allclose(speed[wet], np.hypot(uh, vh)[wet] / depth[wet], rtol=1e-4)
        assert np.all(speed[~wet] == 0)
        assert array(bed, 'velocity').shape == (npoints, 3)
        names += ['momentum', 'speed', 'velocity']
    if 'stage_c' in nc.variables:
        assert np.allclose(array(bed, 'stage_c', cells=True), nc['stage_c'][index])
        names += ['stage_c', 'depth_c']

    # Dry triangles are dropped from Water unless asked for.
    tri = nc['volumes'][:]
    wet_tris = int((depth[tri] > 0.001).any(axis=1).sum())
    assert water.GetNumberOfCells() == wet_tris
    reader.ShowDryWater = 1
    assert fetch(reader, 1, t).GetNumberOfCells() == ntris
    reader.ShowDryWater = 0

    reader.VerticalScale = 2.0
    pts2 = vtk_to_numpy(fetch(reader, 0, t).GetPoints().GetData())
    assert np.allclose(pts2[:, 2], 2.0 * elev)
    reader.VerticalScale = 1.0

    xll = float(getattr(nc, 'xllcorner', 0.0))
    reader.Georeferenced = 1
    pts3 = vtk_to_numpy(fetch(reader, 0, t).GetPoints().GetData())
    assert np.allclose(pts3[:, 0], nc['x'][:].astype(np.float64) + xll)
    reader.Georeferenced = 0

    reader.ComputeMaxima = 1
    maxdepth = array(fetch(reader, 0, t), 'max_depth')
    assert np.all(maxdepth >= depth - 1e-6)
    reader.ComputeMaxima = 0

    # The array lists offer exactly what the reader outputs, all ticked.
    available = set(reader.PointArrays.Available)
    pd = bed.GetPointData()
    assert available == set(pd.GetArrayName(i) for i in range(pd.GetNumberOfArrays()))
    assert set(reader.PointArrays) == available
    assert set(reader.CellArrays.Available) == set(
        bed.GetCellData().GetArrayName(i) for i in range(bed.GetCellData().GetNumberOfArrays()))

    # Unticked arrays are left out, but the surfaces are still built from
    # elevation and stage, and dry triangles still dropped by depth.
    reader.PointArrays = ['stage']
    reader.CellArrays = []
    bed2, water2 = fetch(reader, 0, t), fetch(reader, 1, t)
    for out in (bed2, water2):
        assert out.GetPointData().GetNumberOfArrays() == 1
        assert out.GetPointData().GetArray('stage') is not None
        assert out.GetCellData().GetNumberOfArrays() == 0
    assert np.allclose(vtk_to_numpy(bed2.GetPoints().GetData())[:, 2], elev)
    assert water2.GetNumberOfCells() == wet_tris
    reader.PointArrays = sorted(available)
    reader.CellArrays = list(reader.CellArrays.Available)

    # ANUGA Timeseries at the centroid of a triangle: the mean of its corners.
    k = ntris // 2
    corners = tri[k]
    # In double precision: some files store full UTM coordinates as floats.
    px = nc['x'][:][corners].astype(np.float64).mean()
    py = nc['y'][:][corners].astype(np.float64).mean()
    series = ANUGATimeseries(Input=simple.OutputPort(reader, 0))
    series.Location = [float(px), float(py), 0.0]
    series.Quantities = ['stage', 'depth'] + (['speed'] if 'xmomentum' in nc.variables else [])
    table = servermanager.Fetch(series)
    assert table.GetNumberOfRows() == len(times)
    assert np.allclose(vtk_to_numpy(table.GetColumnByName('Time')), times)
    all_stage = nc['stage'][:]
    all_elev = nc[bedname][:]
    if all_elev.ndim == 1:
        all_elev = np.broadcast_to(all_elev, all_stage.shape)
    ref_stage = all_stage[:, corners].mean(axis=1)
    ref_depth = np.maximum(ref_stage - all_elev[:, corners].mean(axis=1), 0)
    assert np.allclose(vtk_to_numpy(table.GetColumnByName('stage')), ref_stage, atol=1e-5)
    assert np.allclose(vtk_to_numpy(table.GetColumnByName('depth')), ref_depth, atol=1e-5)
    assert table.GetColumnByName('elevation') is None      # not ticked
    series.Location = [-1e9, -1e9, 0.0]                    # outside the mesh
    assert servermanager.Fetch(series).GetNumberOfColumns() == 1
    simple.Delete(series)

    simple.Delete(reader)
    nc.close()

    # Only the variables needed for the wanted arrays are read from the file.
    sww = sww_reader.SWWFile(filename)
    read = []
    original = sww._read
    sww._read = lambda name, animated, i: read.append(name) or original(name, animated, i)
    sww.point_data(index, ['stage'])
    assert set(read) == set([bedname, 'stage']), read
    if 'xmomentum' in sww.point_vars:
        del read[:]
        sww.point_data(index, ['speed'])
        assert set(read) == set([bedname, 'stage', 'xmomentum', 'ymomentum']), read
    del read[:]
    sww.cell_data(index, [])
    assert read == []
    sww.close()
    print('ok  %-50s %5d tris %4d steps  %s' % (os.path.basename(filename), ntris,
                                                  len(times), ' '.join(names)))


files = sys.argv[1:] or sorted(glob.glob(os.path.join(PLUGIN_DIR, '..', 'data', '*.sww')))
for f in files:
    check(f)
