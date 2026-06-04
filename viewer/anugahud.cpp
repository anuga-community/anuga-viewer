#include "anugahud.h"

// Canvas is ORTHO2D_WIDTH x ORTHO2D_HEIGHT (1280 x 1024), origin bottom-left.
//
// Left column (x=20, left-aligned, top-down below title):
//   wireframe, culling, color, wetdepth, data, mode, vscale, grid, recorder
// Filename row (x=20, left-aligned): bottom of screen, stretches freely right.
//
// Row spacing 38px.  First row 44px below title baseline (y ≈ 940), filename y=20.

static const float LX   = 20.0f;
static const float TSY  = ORTHO2D_HEIGHT - 84.0f;   // first status row (title at H-40, gap 44)
static const float DY   = 38.0f;
static const float FY   = 20.0f;    // filename row

AnugaHUD::AnugaHUD() :
	HeadsUpDisplay()
{
	osg::Geode* left = new osg::Geode;

	// Controls — top to bottom under the title
	addStatusLine("wireframe", "wireframe (w)",  left, osg::Vec3(LX, TSY,        0));
	addStatusLine("culling",   "culling (c)",    left, osg::Vec3(LX, TSY - DY,   0));
	addStatusLine("color",     "color (v/V)",    left, osg::Vec3(LX, TSY - 2*DY, 0));
	addStatusLine("wetdepth",  "wetdepth (a/A)", left, osg::Vec3(LX, TSY - 3*DY, 0));
	addStatusLine("data",      "data (q)",       left, osg::Vec3(LX, TSY - 4*DY, 0));
	addStatusLine("mode",      "mode (t)",       left, osg::Vec3(LX, TSY - 5*DY, 0));
	addStatusLine("vscale",    "vscale (z/Z)",   left, osg::Vec3(LX, TSY - 6*DY, 0));
	addStatusLine("grid",      "grid (g)",       left, osg::Vec3(LX, TSY - 7*DY, 0));
	addStatusLine("recorder",  "recorder (1)",   left, osg::Vec3(LX, TSY - 8*DY, 0));

	// Filename — bottom row, stretches freely right
	addStatusLine("filename",  "filename",       left, osg::Vec3(LX, FY, 0));

	_status_switch->addChild(left);
}


void AnugaHUD::setWireframe(WireframeMode aWireframeMode)
{
	std::string mode_string;
	switch(aWireframeMode)
	{
		case WF_BED:
			mode_string = "bed";
			break;

		case WF_BOTH:
			mode_string = "bed+water";
			break;

		case WF_WATER:
			mode_string = "water";
			break;

		case WF_NONE:
			mode_string = "off";
			break;

		default:
			assert(0);
			break;
	}

	setStatus("wireframe", mode_string);
}
