/*
  SWWViewer

  An OpenSceneGraph viewer for ANUGA .sww files.
  Copyright (C) 2004-2005, 2009 Geoscience Australia
*/

#include <iostream>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#endif

#include <osg/Group>
#include <osg/Material>
#include <osg/MatrixTransform>
#include <osg/Notify>
#include <osg/PositionAttitudeTransform>
#include <osg/StateAttribute>
#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include <osgViewer/ViewerEventHandlers>
#include <osgText/Text>

#include <osgGA/TrackballManipulator>
#include <osgGA/FlightManipulator>
#include <osgGA/DriveManipulator>
#include <osgGA/KeySwitchMatrixManipulator>
#include <osgGA/StateSetManipulator>
#include <osgGA/AnimationPathManipulator>
#include <osgGA/TerrainManipulator>

#include <project.h>
#include <swwreader.h>
#include <bedslope.h>
#include <keyboardeventhandler.h>
#include <directionallight.h>
#include <state.h>
#include <watersurface.h>
#include <customargumentparser.h>

#include "skybox.h"
#include "anugahud.h"
#include "linegraph.h"
#include "customviewer.h"
#include "osmtexture.h"

// prototypes
extern const char* version();

unsigned int playback_index = 0;

static const char * S_VIEWER_TITLE = "ANUGA Viewer";

AnugaHUD * g_hud = NULL;

// OSG's StandardManipulator calls home() on every RESIZE event, which causes
// the view to jump to the home position when toggling fullscreen.  Override
// handle() to silently drop RESIZE so the camera stays where the user left it.
class TerrainManipulatorNoHomeOnResize : public osgGA::TerrainManipulator {
public:
	bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& us) override {
		if (ea.getEventType() == osgGA::GUIEventAdapter::RESIZE)
			return false;
		return osgGA::TerrainManipulator::handle(ea, us);
	}
};


// HelpHandler subclass that increases the font size after the overlay is first built.
class AnugaHelpHandler : public osgViewer::HelpHandler
{
public:
    AnugaHelpHandler() : _fontFixed(false) {}

    virtual bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa)
    {
        bool result = osgViewer::HelpHandler::handle(ea, aa);
        if (!_fontFixed && getCamera())
        {
            fixFont(getCamera());
            _fontFixed = true;
        }
        return result;
    }

private:
    bool _fontFixed;

    void fixFont(osg::Node* node)
    {
        if (osg::Geode* geode = dynamic_cast<osg::Geode*>(node))
        {
            for (unsigned int i = 0; i < geode->getNumDrawables(); ++i)
            {
                osgText::Text* t = dynamic_cast<osgText::Text*>(geode->getDrawable(i));
                if (t) t->setCharacterSize(24.0f);
            }
        }
        if (osg::Group* group = dynamic_cast<osg::Group*>(node))
        {
            for (unsigned int i = 0; i < group->getNumChildren(); ++i)
                fixFont(group->getChild(i));
        }
    }
};


int main( int argc, char **argv )
{
#ifdef _WIN32
   // Set OSG_LIBRARY_PATH to the exe's own directory so OSG finds the bundled
   // osgPlugins-* directory when the exe is launched directly (not via bat file).
   if (!getenv("OSG_LIBRARY_PATH")) {
      char exePath[MAX_PATH];
      if (GetModuleFileNameA(NULL, exePath, MAX_PATH)) {
         std::string exeDir = osgDB::getFilePath(std::string(exePath));
         if (!exeDir.empty()) {
            std::string envStr = "OSG_LIBRARY_PATH=" + exeDir;
            _putenv(envStr.c_str());
         }
      }
   }
#endif

   // use an ArgumentParser object to manage the program arguments.
   // this custom version detects if the last argument is a macro file
   // and modifies the argument list accordingly so the following code works ...
   CustomArgumentParser arguments( &argc, argv );

   // construct the viewer.
   CustomViewer viewer(arguments);

   osgViewer::ScreenCaptureHandler * cap_handler = new osgViewer::ScreenCaptureHandler;
   cap_handler->setKeyEventTakeScreenShot('O');
   viewer.addEventHandler(cap_handler); 

   // animation
   StateList statelist;
   bool playbackmode;
   bool recordingmode = false;
   bool savemovie = false;
   bool loop = false;
   std::string moviedir("screenshots");	// default screenshot directory

   if( arguments.isSWM() )
   {
	  playbackmode = true;
	  statelist.read( arguments.getFilename() );
	  if( arguments.read("-movie", moviedir) )	// choose a different folder
	  {
		 savemovie = true;  // flag to store frames and quit ...
	  }
	  if( arguments.read("-loop") ) 
	  {
		 loop = true;
	  }
   }
   else
   {
	  playbackmode = false;
	  savemovie = false;
	  loop = true;  // playback in none macro mode should loop (otherwise you don't get a chance to save)
   }

   // setup screenshot location
   osgDB::makeDirectory( moviedir );
   cap_handler->setCaptureOperation(new osgViewer::ScreenCaptureHandler::WriteToFile(moviedir+"/frame", "jpg", osgViewer::ScreenCaptureHandler::WriteToFile::SEQUENTIAL_NUMBER));

   // Compact viewer help — show our options and key bindings without OSG boilerplate
   if( arguments.read("-help") || arguments.read("--help") || arguments.read("-h") )
   {
      std::cout <<
         "Usage: anuga_viewer [options] file.sww\n"
         "       anuga_viewer [options] file.swm\n"
         "\n"
         "Options:\n"
         "  -texture <file>               Bedslope texture image (overrides auto tile fetch)\n"
         "  -maptiles osm|satellite|none  Map tile source when SWW has UTM zone (default: osm)\n"
         "  -scale <float>                Initial vertical exaggeration (default: 1.0)\n"
         "  -tps <float>                  Timesteps per second (default: 10)\n"
         "  -hmin <float>                 Depth below which water is transparent\n"
         "  -hmax <float>                 Depth above which water is fully opaque\n"
         "  -alphamin <float 0-1>         Transparency at hmin\n"
         "  -alphamax <float 0-1>         Maximum transparency\n"
         "  -speedmax <float>             Speed colour scale maximum (m/s)\n"
         "  -momentummax <float>          Momentum colour scale maximum (m^2/s)\n"
         "  -stagemin <float>             Stage colour scale minimum elevation (m)\n"
         "  -cullangle <float 0-90>       Cull triangles steeper than this angle\n"
         "  -lightpos x,y,z              Directional light position (default: 1,1,1)\n"
         "  -nosky                        Disable skybox\n"
         "  -movie <dir>                  Export frames to directory (use with .swm)\n"
         "  -loop                         Loop .swm playback\n"
         "  -version                      Print revision number\n"
         "  --osg-help                    Show full OSG options and environment variables\n"
         "\n"
         "Keyboard shortcuts:\n"
         "  Space          Pause / resume\n"
         "  v / V          Cycle colour mode forward / backward\n"
         "                   (stage / depth / speed / momentum /\n"
         "                    max depth / max speed / max momentum / max stage)\n"
         "  [ / ]          Decrease / increase colour scale maximum by 20%\n"
         "  z / Z          Decrease / increase vertical exaggeration by 50%\n"
         "  w              Cycle wireframe modes\n"
         "  g              Cycle grid / colorbar overlay\n"
         "  i              Toggle information HUD\n"
         "  l              Toggle lighting\n"
         "  t              Cycle view mode: landscape (map) → colour (osm) → colour\n"
         "  b              Toggle backface culling\n"
         "  c              Toggle steep-triangle culling\n"
         "  x              Reset camera\n"
         "  r              Reset animation to timestep 0\n"
         "  1              Start / stop recording camera path\n"
         "  2              Play / stop recorded macro\n"
         "  3              Save macro as movie.swm\n"
         "  O              Screenshot\n"
         "  Shift+arrows   Pan camera\n"
         "  Shift+LMB      Show timeseries for clicked point\n"
         "  Escape         Quit\n";
      return 0;
   }

   if( arguments.read("--osg-help") )
   {
      arguments.getApplicationUsage()->write(std::cout, osg::ApplicationUsage::HELP_ALL);
      return 0;
   }


   // same for version info
   if( arguments.read("-version") )
   {
	  std::cout << version() << std::endl;
	  return 1;
   }


   // last argument is either an sww file or an swm macro recording
   int lastarg = arguments.argc()-1;
   std::string swwfile = arguments.argv()[lastarg];
   arguments.remove(lastarg);
   if( osgDB::getLowerCaseFileExtension(swwfile) != std::string("sww") )
   {
	  std::cout << "Require last argument be an .sww/.swm file ... quitting" << std::endl;
	  return 1; 
   }
   SWWReader *sww = new SWWReader(swwfile);
   if (sww->isValid() == false)
   {
	  std::cout << "Unable to load " << swwfile << " ... is this really an .sww file?" << std::endl;
	  return 1;
   }


   // need swollen binary directory which contains resource images
   char *ptr = getenv( "SWOLLEN_BINDIR" );
   if (ptr)
	  sww->setSwollenDir( std::string(ptr) );
   else if( osgDB::getFilePath(argv[0]) == "" )
	  sww->setSwollenDir( std::string(".") );
   else
	  sww->setSwollenDir( osgDB::getFilePath(argv[0]) );
   std::cout << "SWOLLEN_BINDIR = " << sww->getSwollenDir() << std::endl;


   // default arguments and command line parameters
   float tmpfloat, tps, vscale;
   if( !arguments.read("-tps", tps) || tps <= 0.0 ) tps = DEF_TPS;
   if( !arguments.read("-scale", vscale) ) vscale = 1.0;
   if( arguments.read("-hmin",tmpfloat) ) sww->setHeightMin( tmpfloat );
   if( arguments.read("-hmax",tmpfloat) ) sww->setHeightMax( tmpfloat );
   if( arguments.read("-alphamin",tmpfloat) ) sww->setAlphaMin( tmpfloat );
   if( arguments.read("-alphamax",tmpfloat) ) sww->setAlphaMax( tmpfloat );
   if( arguments.read("-cullangle",tmpfloat) ) sww->setCullAngle( tmpfloat );
   if( arguments.read("-speedmax",tmpfloat) ) sww->setSpeedMax( tmpfloat );
   if( arguments.read("-momentummax",tmpfloat) ) sww->setMomentumMax( tmpfloat );
   if( arguments.read("-stagemin",tmpfloat) ) sww->setStageOffset( tmpfloat );

   std::string bedslopetexture;
   std::string maptiles = "osm";
   arguments.read("-maptiles", maptiles);

   if( arguments.read("-texture",bedslopetexture) )
   {
      sww->setBedslopeTexture( bedslopetexture );
   }
   else if ( maptiles != "none" && sww->getUTMZone() > 0 )
   {
      // Auto-fetch map tiles when SWW has a valid UTM zone and no -texture given.
      // Cache the GeoTIFF next to the SWW file for reuse on subsequent launches.
      MapTileSource tileSource = (maptiles == "satellite") ? MapTileSource::SATELLITE
                                                           : MapTileSource::OSM;
      std::string suffix = (tileSource == MapTileSource::SATELLITE) ? "_satellite.tif"
                                                                     : "_osm.tif";
      std::string swwDir = osgDB::getFilePath( sww->getSWWFilename() );
      if (swwDir.empty()) swwDir = ".";
      std::string tifPath = swwDir + "/" +
          osgDB::getStrippedName( sww->getSWWFilename() ) + suffix;
      std::string result = fetchMapTexture(sww, tifPath, tileSource);
      if (!result.empty())
      {
         sww->setBedslopeTexture(result);
         bedslopetexture = result;
      }
   }

   // HUD labels for the three texture modes cycled by the 't' key.
   // landscapeLabel  (mode 0): OSM/texture on, water hidden — pure map view
   // colourOsmLabel  (mode 1): OSM/texture on, water coloured — flood analysis with map background
   // "colour"        (mode 2): no texture, water coloured
   std::string landscapeLabel;   // mode 0
   std::string colourOsmLabel;   // mode 1
   if (!bedslopetexture.empty())
   {
      if (maptiles == "satellite")
      {
         landscapeLabel = "landscape (satellite)";
         colourOsmLabel = "colour (satellite)";
      }
      else if (maptiles == "osm")
      {
         landscapeLabel = "landscape (osm)";
         colourOsmLabel = "colour (osm)";
      }
      else  // user-supplied -texture file
      {
         landscapeLabel = "landscape";
         colourOsmLabel = "landscape";
      }
   }

   // root node
   osg::Group* rootnode = new osg::Group;

   // transform
   osg::PositionAttitudeTransform* model = new osg::PositionAttitudeTransform;
   model->setName("position_attitude_transform");

   // enscapsulates OpenGL state
   osg::StateSet* rootStateSet = new osg::StateSet;

   // Bedslope geometry
   BedSlope* bedslope = new BedSlope(sww);

   // Water geometry
   WaterSurface* water = new WaterSurface(sww);

   // Heads Up Display (text overlay)
   g_hud = new AnugaHUD();
   g_hud->setTitle(S_VIEWER_TITLE);

   // --- initial HUD status line values
	g_hud->setStatus("recorder", arguments.isSWM() ? "playback" : "paused");
	g_hud->setStatus("filename", swwfile);
	g_hud->setStatus("culling", water->getCulling() ? "on" : "off");
	g_hud->setStatus("wireframe", "off");
	g_hud->setStatus("color", "stage (max 1.00 m)");
	// texMode: 0=landscape (water hidden, map visible), 1=colour+osm, 2=colour (no texture)
	int texMode = bedslopetexture.empty() ? 2 : 0;
	g_hud->setStatus("mode", bedslopetexture.empty() ? "colour" : landscapeLabel);
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "%.2fx", vscale);
		g_hud->setStatus("vscale", std::string(buf));
	}

   // Lighting
   DirectionalLight* light = new DirectionalLight(rootStateSet);
   light->setPosition( osg::Vec3(1,1,1) );  // z is up

   std::string lightposstr;
   while (arguments.read("-lightpos",lightposstr))
   {
	  float x, y, z;
	  int count = sscanf( lightposstr.c_str(), "%f,%f,%f", &x, &y, &z );
	  if( count == 3 ) light->setPosition( osg::Vec3(x,y,z) );  // z is up
	  else osg::notify(osg::WARN) << "Invalid bedslope light position \"" << lightposstr << "\"" << std::endl;
   }

   // scenegraph hierarchy
   rootnode->setStateSet(rootStateSet);
   rootnode->addChild( g_hud->get() );
   rootnode->addChild( light->get() );
   rootnode->addChild(model);
   model->addChild( bedslope->get() );
   model->addChild( water->get() );

   // Start in landscape mode (mode 0): show OSM map without water overlay
   if (texMode == 0)
      water->get()->setNodeMask(0);

	// Load the initial frame so we can get grid extents
	sww->loadBedslopeVertexArray(0);
	bedslope->update();

	osg::Switch * grid_switch = new osg::Switch();
	grid_switch->addChild(Axes_Create(bedslope->getBound()));
	grid_switch->setAllChildrenOff();
	model->addChild(grid_switch);

   // allow vertical scaling from command line parameter
   model->setScale( osg::Vec3(1.0, 1.0, vscale) );
   // GL_NORMALIZE renormalizes normals after the inverse-transpose of the scale transform
   model->getOrCreateStateSet()->setMode(GL_NORMALIZE, osg::StateAttribute::ON);

	// Allow us to switch the sky on and off if we don't want photo-realism
	osg::Switch * sky_switch = new osg::Switch();	

	if( !arguments.read("-nosky") )
	{
		std::string sky_tex_path = sww->getSwollenDir() + std::string("/images/sky_small.jpg");
		std::cout << "sky texture path: " << sky_tex_path << std::endl;
		
		// surrounding sky sphere
		sky_switch->addChild(Skybox_Create(10.0, sky_tex_path ));
		sky_switch->setAllChildrenOn();
		rootnode->addChild(sky_switch);
	}

   // add model to viewer.
   viewer.setSceneData(rootnode);
 
   // any option left unread are converted into errors to write out later.
   arguments.reportRemainingOptionsAsUnrecognized();
 
   // report any errors if they have occured when parsing the program aguments.
   if (arguments.errors())
   {
	  arguments.writeErrorMessages(std::cout);
	  return 1;
   }

	// set up the camera manipulators.

	//osgGA::MatrixManipulator * mman = new osgGA::FlightManipulator();
	TerrainManipulatorNoHomeOnResize * mman = new TerrainManipulatorNoHomeOnResize();
	mman->setNode(model);
	mman->setAutoComputeHomePosition( false );
	mman->setHomePosition(
	  osg::Vec3d(0,-3,3),    // camera location
	  osg::Vec3d(0,0,0),     // camera target
	  osg::Vec3d(0,1,1) );   // camera up vector
	mman->setMinimumDistance(0.000001);
	viewer.setCameraManipulator(mman);
	viewer.home();

	// add the state manipulator
	osgGA::StateSetManipulator * ssm = new osgGA::StateSetManipulator(viewer.getCamera()->getOrCreateStateSet());
	ssm->setKeyEventToggleBackfaceCulling('b');
	ssm->setKeyEventToggleLighting('l');
	ssm->setKeyEventToggleTexturing('\0');  // handled by KeyboardEventHandler
	ssm->setKeyEventCyclePolygonMode('\0');
	viewer.addEventHandler( ssm );

	// register additional event handler
	KeyboardEventHandler* event_handler = new KeyboardEventHandler(sww->getNumberOfTimesteps(), tps);
	viewer.addEventHandler(event_handler);

	// add the help handler
	viewer.addEventHandler(new AnugaHelpHandler);

	// add the window size toggle handler (provides fullscreen toggle on 'f')
	viewer.addEventHandler(new osgViewer::WindowSizeHandler);

	// add the stats handler
	viewer.addEventHandler(new osgViewer::StatsHandler);

	// create the windows and run the threads.
	// setUpViewInWindow starts in a decorated window so the WM applies borders
	// immediately.  setUpViewOnSingleScreen fills the whole screen at the X11
	// level, which some WMs don't decorate until the user toggles fullscreen.
	// The user can still press 'f' to go fullscreen on screen 0.
	viewer.setUpViewInWindow(50, 50, 1024, 768, 0);
	// Single-threaded: draw happens on the main thread in sync with the geometry
	// update.  Multi-threaded mode causes the draw thread to read vertex arrays
	// while the main thread is writing them (different timestep), producing the
	// "jumping between timeslices" flicker seen in fullscreen on Windows.
	viewer.setThreadingModel(osgViewer::Viewer::SingleThreaded);
	viewer.realize();

	unsigned int timestep = 0;

	while( !viewer.done() )
	{
		if( !playbackmode )
		{
			 // current time in seconds
			 double time = viewer.getFrameStamp()->getReferenceTime();

			 event_handler->setTime( time );
			 timestep = event_handler->getTimestep();
			 water->setTimeStep(timestep);
			 bedslope->setTimeStep(timestep);
			 g_hud->setTime( sww->getTime(timestep) );

			// these methods do their own dirty checking
			water->setWireframe((event_handler->getWireframeMode() & WF_WATER) > 0);
			bedslope->setWireframe((event_handler->getWireframeMode() & WF_BED) > 0);

			if( event_handler->toggleCulling() )
			{
				bool culling = water->getCulling();
				water->setCulling(!culling);
				g_hud->setStatus("culling", culling ? "on" : "off");
			}

			GridMode ge = event_handler->getGridMode();
			viewer.setGrid(grid_switch, ge);

			static ColorMode colorMode_last = CM_STAGE;
			ColorMode colorMode = event_handler->getColorMode();
			bool colorChanged = (colorMode != colorMode_last);
			colorMode_last = colorMode;
			sww->setColorMode(colorMode);
			if (colorChanged)
				water->forceRefresh();

			// [ / ] nudge the colour scale max for the active mode by ±20%
			int nudge = event_handler->scaleNudge();
			if (nudge != 0 || colorChanged)
			{
				const float factor = (nudge > 0) ? 1.2f : (nudge < 0 ? 1.0f/1.2f : 1.0f);
				char buf[64];
				if (colorMode == CM_SPEED || colorMode == CM_MAX_SPEED)
				{
					sww->setSpeedMax(sww->getSpeedMax() * factor);
					const char* label = (colorMode == CM_MAX_SPEED) ? "max speed" : "speed";
					snprintf(buf, sizeof(buf), "%s (max %.2f m/s)", label, sww->getSpeedMax());
				}
				else if (colorMode == CM_MOMENTUM || colorMode == CM_MAX_MOMENTUM)
				{
					sww->setMomentumMax(sww->getMomentumMax() * factor);
					const char* label = (colorMode == CM_MAX_MOMENTUM) ? "max momentum" : "momentum";
					snprintf(buf, sizeof(buf), "%s (max %.2f m^2/s)", label, sww->getMomentumMax());
				}
				else
				{
					sww->setHeightMax(sww->getHeightMax() * factor);
					const char* label = (colorMode == CM_MAX_DEPTH)  ? "max depth" :
					                    (colorMode == CM_MAX_STAGE)  ? "max stage" :
					                    (colorMode == CM_STAGE)      ? "stage" : "depth";
					snprintf(buf, sizeof(buf), "%s (max %.2f m)", label, sww->getHeightMax());
				}
				g_hud->setStatus("color", std::string(buf));
			}

			int zNudge = event_handler->zScaleNudge();
			if (zNudge != 0)
			{
				vscale *= (zNudge > 0) ? 1.5f : (1.0f / 1.5f);
				if (vscale < 0.01f) vscale = 0.01f;
				model->setScale(osg::Vec3(1.0, 1.0, vscale));
				sww->setVScale(vscale);
				water->forceRefresh();
				bedslope->forceRefresh();
				char buf[32];
				snprintf(buf, sizeof(buf), "%.2fx", vscale);
				g_hud->setStatus("vscale", std::string(buf));
			}

			{
				int panX = event_handler->getPanX();
				int panY = event_handler->getPanY();
				if (panX != 0 || panY != 0)
				{
					osgGA::CameraManipulator* manip = viewer.getCameraManipulator();
					osg::Matrixd m = manip->getMatrix();
					// Extract right and up vectors from camera matrix (column 0 and 1)
					osg::Vec3d right(m(0,0), m(0,1), m(0,2));
					osg::Vec3d up(m(1,0), m(1,1), m(1,2));
					// Pan step: 3% of eye distance from origin
					osg::Vec3d eye(m(3,0), m(3,1), m(3,2));
					double step = eye.length() * 0.03;
					osg::Vec3d delta = right * (-panX * step) + up * (-panY * step);
					m(3,0) += delta.x();
					m(3,1) += delta.y();
					m(3,2) += delta.z();
					manip->setByMatrix(m);
				}
			}

			if (event_handler->toggleTexture())
			{
				if (bedslopetexture.empty())
				{
					// No texture: no-op (nothing to toggle)
				}
				else
				{
					// Cycle: landscape (osm) → colour (osm) → colour → landscape (osm)
					texMode = (texMode + 1) % 3;
					if (texMode == 0)       // landscape: map visible, water hidden
					{
						ssm->setTextureEnabled(true);
						water->get()->setNodeMask(0);
						g_hud->setStatus("mode", landscapeLabel);
					}
					else if (texMode == 1)  // colour (osm): water visible, map on terrain
					{
						ssm->setTextureEnabled(true);
						water->get()->setNodeMask(~0u);
						g_hud->setStatus("mode", colourOsmLabel);
					}
					else                    // colour: no texture, water visible
					{
						ssm->setTextureEnabled(false);
						water->get()->setNodeMask(~0u);
						g_hud->setStatus("mode", "colour");
					}
				}
			}

			bool mouseClicked = event_handler->checkMouseClicked();
			if (mouseClicked || colorChanged)
			{
				int sp = event_handler->getSelectedPoly();

				// Map colour mode to timeseries quantity
				SWWReader::TimeSeriesType tsType;
				std::string tsTitle, tsUnit;
				switch (colorMode)
				{
					case CM_DEPTH: case CM_MAX_DEPTH:
						tsType = SWWReader::TSTYPE_DEPTH;
						tsTitle = "Depth"; tsUnit = "m"; break;
					case CM_SPEED: case CM_MAX_SPEED:
						tsType = SWWReader::TSTYPE_SPEED;
						tsTitle = "Speed"; tsUnit = "m/s"; break;
					case CM_MOMENTUM: case CM_MAX_MOMENTUM:
						tsType = SWWReader::TSTYPE_MOMENTUM_MAGNITUDE;
						tsTitle = "Momentum"; tsUnit = "m^2/s"; break;
					default: // CM_STAGE, CM_MAX_STAGE
						tsType = SWWReader::TSTYPE_STAGE;
						tsTitle = "Stage"; tsUnit = "m"; break;
				}

				osg::ref_ptr<osg::FloatArray> time_series = new osg::FloatArray;
				if (sp >= 0)
					sww->getTimeSeries(sp, tsType, time_series);

				g_hud->setTimeSeriesData(time_series, sww->getTime(sww->getNumberOfTimesteps()-1), tsTitle, tsUnit);
			}
			

			if( event_handler->checkReturnOrigin() )
			{
				viewer.getCameraManipulator()->home(0.0);
			}
		

			// '1' key starts/stops recording of view/position/setting info
			if( event_handler->toggleRecording() )
			{
				switch( recordingmode )
				{
				   case false : 
					  recordingmode = true; 		  
					  g_hud->setStatus("recorder", "recording");
					  break;
				   case true : 
					  recordingmode = false; 
					  g_hud->setStatus("recorder", "paused");
					  break;
				}
			}


			// '2' key starts playback of recorded frames
			if( event_handler->togglePlayback() && statelist.size() > 0 )
			{
				recordingmode = false;
				playbackmode = true; 
				g_hud->setStatus("recorder", "playback");
				event_handler->setPaused( true );
				playback_index = 0;
			}

			if( recordingmode )
			{
				State state = State();
				state.setTimestep( event_handler->getTimestep() );
				state.setCulling( sww->getCulling() );
				state.setWireframe( event_handler->getWireframeMode() );
				state.setGrid( event_handler->getGridMode() );
				state.setShowHUD( g_hud->isVisible());
				state.setShowTexture(ssm->getTextureEnabled());
				state.setLighting(ssm->getLightingEnabled());
				state.setMatrix(viewer.getCameraManipulator()->getMatrix());
				statelist.push_back( state );
			}
		}
		else
		{
			// in playback mode
			State state = statelist.at( playback_index );
			water->setTimeStep( state.getTimestep() );
			bedslope->setTimeStep( state.getTimestep() );
			water->setWireframe((state.getWireframe() & WF_WATER) > 0);
			bedslope->setWireframe((state.getWireframe() & WF_BED) > 0);
			water->setCulling( state.getCulling() );
			viewer.setGrid(grid_switch, state.getGrid());
			ssm->setTextureEnabled(state.getShowTexture());
			ssm->setLightingEnabled(state.getLighting());

			// update HUD
			g_hud->setTime( sww->getTime(state.getTimestep()) );
			g_hud->setVisible(state.getShowHUD());
			g_hud->setStatus("culling", state.getCulling() ? "on" : "off");
			g_hud->setWireframe((WireframeMode)state.getWireframe());
			g_hud->setStatus("mode", state.getShowTexture() ? landscapeLabel : "colour");

			// loop playback
			playback_index ++;
			if( playback_index == statelist.size() )
			{
				if( loop )
				{
					playback_index = 0;
				}
				else
				{
					viewer.setDone(true);
				}
			}

			// '2' key stops playback of recorded frames
			if( event_handler->togglePlayback() )
			{
				playbackmode = false; 
				g_hud->setStatus("recorder", "paused");
				event_handler->setPaused( true );
			}

			viewer.getCameraManipulator()->setByMatrix(state.getMatrix());
	  }


	  // '3' key causes compiled animation to be saved to disk
	  if( event_handler->toggleSave() )
	  {
		 std::fstream f;
		 f.open( "movie.swm", std::fstream::out );
		 if( f.is_open() )
		 {
			f << "# SWM 1.0 START" << std::endl;
			arguments.write( f );
			statelist.write( f );
			f << "# SWM END" << std::endl;
			f.close();
		 }
		 std::cout << "Wrote macro file movie.swm" << std::endl;

		g_hud->setStatus("filename", "saved as movie.swm");

	  }

		if (savemovie)
		{
			cap_handler->captureNextFrame(viewer);
		}

		// Toggle sky and update bed texture if we have toggled texturing
		bool tex_enabled = ssm->getTextureEnabled();
		static bool tex_enabled_last = tex_enabled;
		if (tex_enabled != tex_enabled_last)
		{
			if (tex_enabled)
			{
				sky_switch->setAllChildrenOn();
				bedslope->onRefreshTextured(true);
			}
			else
			{
				sky_switch->setAllChildrenOff();
				bedslope->onRefreshTextured(false);
			}
		}
		tex_enabled_last = tex_enabled;

		// scene-graph updates
		water->update();
		bedslope->update();
		g_hud->update();

		// fire off the cull and draw traversals of the scene.
		viewer.frame();
	}

#ifdef _WIN32
	// TerminateProcess kills the process immediately without calling DLL detach
	// handlers or static destructors.  ExitProcess and exit() both trigger DLL
	// teardown, which hangs in the MinGW/OSG runtime on Windows.  We only read
	// SWW files so abandoning open handles is safe.
	TerminateProcess(GetCurrentProcess(), 0);
#else
	return 0;
#endif
}
