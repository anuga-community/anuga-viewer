"""Set up a ready-made ParaView view of an ANUGA .sww file.

Normally run through the launcher, which loads the reader and passes the
file in ANUGA_SWW_FILE:

    paraview/anuga-paraview run.sww

It can also be run in an open ParaView session with View > Python Shell >
Run Script, where it sets up the view for the selected .sww reader.

The view shows the bed coloured by elevation and the water coloured by
depth, with very shallow water faded out, the depth colour range fixed over
the run (sampling up to 50 timesteps), a time label, and an oblique camera
looking north.
"""

import os

import paraview.simple as pv

# This file sits in paraview/scripts/, the reader one level up.
PLUGIN = os.path.join(os.path.dirname(os.path.dirname(
    os.path.abspath(globals().get('__file__', os.getcwd())))), 'sww_reader.py')

# Depth (m) over which water fades from transparent to opaque.
FADE_DEPTH = 0.05

# Timesteps sampled to find the depth colour range.
RANGE_SAMPLES = 50


def _reader():
    """The .sww reader to show: the launcher's file, or the selected source."""
    if not hasattr(pv, 'ANUGASWWReader') and os.path.exists(PLUGIN):
        pv.LoadPlugin(PLUGIN, ns=vars(pv))
    # Use the launcher's file once: running the script again in the same
    # session (as a macro, say) should act on the selected source instead.
    filename = os.environ.pop('ANUGA_SWW_FILE', None)
    if filename:
        reader = pv.OpenDataFile(filename)
        pv.RenameSource(os.path.basename(filename), reader)
        return reader
    source = pv.GetActiveSource()
    if source is not None and source.GetXMLName() == 'ANUGASWWReader':
        return source
    raise RuntimeError('Open a .sww file and select it, or run paraview/anuga-paraview file.sww')


def _timesteps(reader):
    reader.UpdatePipelineInformation()
    times = reader.TimestepValues
    return list(times) if hasattr(times, '__len__') else [times]


def _max_over_time(reader, port, array, times):
    """Largest value of a point array over (a sample of) the timesteps."""
    step = max(1, len(times) // RANGE_SAMPLES)
    high = 0.0
    for t in times[::step] + times[-1:]:
        reader.UpdatePipeline(t)
        info = reader.GetDataInformation(port).GetPointDataInformation() \
            .GetArrayInformation(array)
        if info is not None:
            high = max(high, info.GetComponentRange(0)[1])
    return high


def setup_view(reader):
    view = pv.GetActiveViewOrCreate('RenderView')
    times = _timesteps(reader)
    scene = pv.GetAnimationScene()
    scene.UpdateAnimationUsingDataTimeSteps()
    scene.AnimationTime = times[0]

    bed = pv.Show(pv.OutputPort(reader, 0), view)
    pv.ColorBy(bed, ('POINTS', 'elevation'))
    pv.GetColorTransferFunction('elevation').ApplyPreset('gist_earth', True)
    bed.SetScalarBarVisibility(view, False)

    water = pv.Show(pv.OutputPort(reader, 1), view)
    pv.ColorBy(water, ('POINTS', 'depth'))
    depth = pv.GetColorTransferFunction('depth')
    # Fix the range over the whole run, so colours mean the same depth at
    # every timestep instead of following the first (often dry) one.
    high = max(_max_over_time(reader, 1, 'depth', times), 0.01)
    # Light blue to navy: deeper is darker, and even shallow water is
    # clearly blue rather than the near-white of most sequential maps.
    depth.RGBPoints = [0.0, 0.62, 0.84, 1.0, high, 0.02, 0.11, 0.40]
    depth.ColorSpace = 'Lab'
    depth.AutomaticRescaleRangeMode = 'Never'

    # Fade very shallow water out, like anuga_viewer -wetdepth, so a film of
    # rain on the ground does not hide the terrain.
    fade = min(FADE_DEPTH, high / 2)
    opacity = pv.GetOpacityTransferFunction('depth')
    opacity.Points = [0.0, 0.0, 0.5, 0.0, fade, 1.0, 0.5, 0.0, high, 1.0, 0.5, 0.0]
    depth.EnableOpacityMapping = 1
    water.SetScalarBarVisibility(view, True)
    bar = pv.GetScalarBar(depth, view)
    bar.Title = 'Depth (m)'
    bar.ComponentTitle = ''
    bar.HorizontalTitle = 1
    bar.RangeLabelFormat = '{:.2f}'

    label = pv.AnnotateTimeFilter(reader)
    pv.RenameSource('Time label', label)
    label.Format = 'Time: {time:.1f} s'
    label_display = pv.Show(label, view)
    label_display.WindowLocation = 'Upper Left Corner'
    label_display.FontSize = 16

    # Oblique view from the south, fitted to the domain.
    reader.UpdatePipeline(times[0])
    xmin, xmax, ymin, ymax, zmin, zmax = reader.GetDataInformation(0).GetBounds()
    centre = [(xmin + xmax) / 2, (ymin + ymax) / 2, (zmin + zmax) / 2]
    size = max(xmax - xmin, ymax - ymin)
    view.CameraFocalPoint = centre
    view.CameraPosition = [centre[0], centre[1] - 1.2 * size, centre[2] + 0.9 * size]
    view.CameraViewUp = [0, 0, 1]
    pv.ResetCamera(view)
    view.OrientationAxesVisibility = 1

    pv.SetActiveSource(reader)
    pv.Render(view)
    return view


setup_view(_reader())
