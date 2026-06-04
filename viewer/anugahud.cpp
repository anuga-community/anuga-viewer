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
	addStatusLine("mode",      "(t) mode",       left, osg::Vec3(LX, TSY,        0));
	addStatusLine("color",     "(v/V) colour",   left, osg::Vec3(LX, TSY - DY,   0));
	addStatusLine("data",      "(q) data",       left, osg::Vec3(LX, TSY - 2*DY, 0));
	addStatusLine("vscale",    "(z/Z) vscale",   left, osg::Vec3(LX, TSY - 3*DY, 0));
	addStatusLine("wetdepth",  "(a/A) wetdepth", left, osg::Vec3(LX, TSY - 4*DY, 0));
	addStatusLine("wireframe", "(w) wireframe",  left, osg::Vec3(LX, TSY - 5*DY, 0));
	addStatusLine("grid",      "(g) grid",       left, osg::Vec3(LX, TSY - 6*DY, 0));
	addStatusLine("culling",   "(c) culling",    left, osg::Vec3(LX, TSY - 7*DY, 0));
	addStatusLine("recorder",  "(1) recorder",   left, osg::Vec3(LX, TSY - 8*DY, 0));

	// Filename — bottom row, stretches freely right
	addStatusLine("filename",  "filename",       left, osg::Vec3(LX, FY, 0));

	_status_switch->addChild(left);

	// ---- Help panel (shown on 'h') ----------------------------------------
	// Positioned to the right of the controls column.
	// Two sub-columns: keys right-aligned at HX_KEY, descriptions left-aligned at HX_DESC.
	static const float HX_KEY  = 490.0f;
	static const float HX_DESC = 510.0f;
	static const float HY      = TSY;          // top of first line, same as controls

	static const osg::Vec4 WHITE(1.0f, 1.0f, 1.0f, 1.0f);

	// Keys column (right-aligned, white)
	osg::Geode* helpGeode = new osg::Geode;
	{
		osgText::Text* t = addText(osg::Vec3(HX_KEY, HY, 0), *_font, false, true);
		t->setColor(WHITE);
		t->setText(
			"Space\n"
			"v / V\n"
			"[ / ]\n"
			"{ / }\n"
			", / .\n"
			"a / A\n"
			"z / Z\n"
			"w\n"
			"t\n"
			"g\n"
			"i\n"
			"q\n"
			"b\n"
			"c\n"
			"l\n"
			"x\n"
			"r\n"
			"1 / 2 / 3\n"
			"O\n"
			"Shift+LMB\n"
			"Shift+arrows\n"
			"Escape"
		);
		helpGeode->addDrawable(t);
	}

	// Descriptions column (left-aligned, white)
	{
		osgText::Text* t = addText(osg::Vec3(HX_DESC, HY, 0), *_font, false, false);
		t->setColor(WHITE);
		t->setText(
			"Pause / resume\n"
			"Cycle colour mode fwd / back\n"
			"Colour scale right endpoint +/-20%\n"
			"Colour scale left endpoint +/-20%  (stage modes)\n"
			"Pan colour scale range  (stage modes)\n"
			"Shallow-water opacity threshold\n"
			"Vertical exaggeration x1.5\n"
			"Cycle wireframe\n"
			"Cycle view mode  (bedslope / OSM / satellite)\n"
			"Cycle grid / colorbar\n"
			"Cycle HUD  (full / minimal / off)\n"
			"Toggle vertex / centroid / faceted data\n"
			"Backface culling\n"
			"Steep-triangle culling\n"
			"Toggle lighting\n"
			"Reset camera\n"
			"Reset to timestep 0\n"
			"Record / play / save camera path\n"
			"Screenshot\n"
			"Show timeseries for clicked point\n"
			"Pan camera\n"
			"Quit"
		);
		helpGeode->addDrawable(t);
	}

	_help_switch->addChild(helpGeode);
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
