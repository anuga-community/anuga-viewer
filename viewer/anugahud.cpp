#include "anugahud.h"

// Canvas is ORTHO2D_WIDTH x ORTHO2D_HEIGHT (1280 x 1024), origin bottom-left.
//
// Left column  (x=20,  left-aligned):   wireframe, culling, color, wetdepth
// Filename row (x=20,  left-aligned):   dedicated row — can stretch freely rightward
// Right column (x=1260, right-aligned): recorder, grid, vscale, mode
//
// Row spacing 38px.  Bottom row y=100 (columns), y=62 (filename), y=20 (title/time).

static const float LX   = 20.0f;    // left column x
static const float RX   = 1260.0f;  // right column x (right-aligned)
static const float Y0   = 100.0f;   // first column row
static const float DY   = 38.0f;    // row step
static const float FY   = 62.0f;    // filename row

AnugaHUD::AnugaHUD() :
	HeadsUpDisplay()
{
	osg::Geode* left  = new osg::Geode;
	osg::Geode* right = new osg::Geode;

	// Left column — bottom to top
	addStatusLine("wireframe", left,  osg::Vec3(LX, Y0,           0));
	addStatusLine("culling",   left,  osg::Vec3(LX, Y0 + DY,      0));
	addStatusLine("color",     left,  osg::Vec3(LX, Y0 + 2*DY,    0));
	addStatusLine("wetdepth",  left,  osg::Vec3(LX, Y0 + 3*DY,    0));

	// Filename — dedicated row, left-aligned, free to stretch right
	addStatusLine("filename",  left,  osg::Vec3(LX, FY,            0));

	// Right column — bottom to top, right-aligned
	addStatusLine("recorder",  right, osg::Vec3(RX, Y0,           0), true);
	addStatusLine("grid",      right, osg::Vec3(RX, Y0 + DY,      0), true);
	addStatusLine("vscale",    right, osg::Vec3(RX, Y0 + 2*DY,    0), true);
	addStatusLine("mode",      right, osg::Vec3(RX, Y0 + 3*DY,    0), true);

	_status_switch->addChild(left);
	_status_switch->addChild(right);
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
