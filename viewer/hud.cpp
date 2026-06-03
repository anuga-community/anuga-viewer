
/*
  HeadsUpDisplay class for orthographic foreground display of text

  An OpenSceneGraph viewer for ANUGA .sww files.
  Copyright (C) 2004, 2009 Geoscience Australia
*/

#include <iostream>
#include <stdio.h>
#include <sstream>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/MatrixTransform>
#include <osg/StateSet>
#include <osgViewer/Viewer>

#include "linegraph.h"
#include "project.h"
#include "hud.h"

#define DEF_HUD_COLOUR   0.8, 0.8, 0.8, 1.0
osg::Vec4 COLORBAR_TEXT_COL(0.8, 0.8, 0.8, 1.0);


// constructor
HeadsUpDisplay::HeadsUpDisplay()
	: _linegraph(NULL),
	_font(NULL),
	_hudVisibility(HUD_FULL),
	_visibility_dirty(false)
{
   // a heads-up display requires an orthographic projection
   _projection = new osg::Projection;
   _projection->setMatrix( osg::Matrix::ortho2D(0,ORTHO2D_WIDTH,0,ORTHO2D_HEIGHT) );

   // font
   std::string SWOLLEN_BINDIR;
   std::string FONT_PATH;
   char *ptr = getenv( "SWOLLEN_BINDIR" );

   if (ptr)
      SWOLLEN_BINDIR = std::string(ptr);
   else
      SWOLLEN_BINDIR = std::string( "." );

   FONT_PATH = SWOLLEN_BINDIR + std::string( "/fonts/arial.ttf" );
   std::cout << "FONT_PATH = " << FONT_PATH << std::endl;

   _font = osgText::readFontFile(FONT_PATH);

   // title text (top-left) — offset enough below canvas edge for ascenders
   _titletext = addText(osg::Vec3(20, ORTHO2D_HEIGHT-40, 0), *_font);
   _dirtytime = false;

   // timer text (top-right, right-aligned, large) — same baseline
   _timetext = addText(osg::Vec3(ORTHO2D_WIDTH-20, ORTHO2D_HEIGHT-40, 0), *_font, true, true);
   _timevalue = 0.0;
   _dirtytime = true;

   _dirtytimeseries = false;

   // state
   osg::StateSet *state = _projection->getOrCreateStateSet();
   state->setRenderBinDetails(12, "RenderBin");
   state->setMode(GL_DEPTH_TEST,osg::StateAttribute::OFF);
   state->setMode( GL_LIGHTING, osg::StateAttribute::OFF );

   // scenegraph
   _xfm = new osg::MatrixTransform;
   _xfm->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
   _xfm->setMatrix(osg::Matrix::identity());

   // title + time: shown in FULL and MINIMAL
   osg::Geode* textnode = new osg::Geode;
   textnode->addDrawable( _titletext );
   textnode->addDrawable( _timetext );
   _text_switch = new osg::Switch;
   _text_switch->addChild(textnode);
   _text_switch->setAllChildrenOn();

   // status lines: shown in FULL only (populated by AnugaHUD)
   _status_switch = new osg::Switch;
   _status_switch->setAllChildrenOn();

   osg::Geode* intensity_scale_node = new osg::Geode;
   _intensity_scale_switch = new osg::Switch;

   {
		osgText::Text* intensityscale = addText(osg::Vec3(48,300,0), *_font);
		intensityscale->setText("> 2.0");
		intensityscale->setColor(COLORBAR_TEXT_COL);
		intensity_scale_node->addDrawable(intensityscale);
   }

   {
		osgText::Text* intensityscale = addText(osg::Vec3(48,300-256,0), *_font);
		intensityscale->setText("0.0");
		intensityscale->setColor(COLORBAR_TEXT_COL);
		intensity_scale_node->addDrawable(intensityscale);
   }
	intensity_scale_node->addDrawable(IntensityBar_Create(osg::Vec3(30,300,0), 16, 256));
	_intensity_scale_switch->addChild(intensity_scale_node);
   _intensity_scale_switch->setAllChildrenOff();

   _xfm->addChild( _text_switch );
   _xfm->addChild( _status_switch );
   _xfm->addChild( _intensity_scale_switch );
   _projection->addChild( _xfm );
}


HeadsUpDisplay::~HeadsUpDisplay()
{
	if (_linegraph)
	{
		delete _linegraph;
		_linegraph = NULL;
	}
}


osgText::Text * HeadsUpDisplay::addText(const osg::Vec3 & aPos, osgText::Font & aFont,
                                        bool aLarge, bool rightAlign)
{
	osgText::Text * text = new osgText::Text;

	text->setFont(&aFont);
	text->setColor(osg::Vec4(DEF_HUD_COLOUR));
	text->setCharacterSize(aLarge ? 30 : 20);
	text->setPosition(aPos);
	text->setFontResolution(40, 40);
	text->setDataVariance(osg::Object::DYNAMIC);
	if (rightAlign)
		text->setAlignment(osgText::Text::RIGHT_BASE_LINE);

	return text;
}


void HeadsUpDisplay::setTime( float t )
{
   if( t != _timevalue )
   {
      _timevalue = t;
      _dirtytime = true;
   }
}


void HeadsUpDisplay::setTitle( const std::string & s )
{
   if( s != _titlestring )
   {
      _titlestring = s;
      _dirtytitle = true;
   }
}


void HeadsUpDisplay::setStatus(const std::string & aField, const std::string & aStatus)
{
	_statusmap[aField]._label = aStatus;
    _dirtystatus = true;
}


void HeadsUpDisplay::cycleVisibility()
{
	switch (_hudVisibility)
	{
		case HUD_FULL:    _hudVisibility = HUD_MINIMAL; break;
		case HUD_MINIMAL: _hudVisibility = HUD_NONE;    break;
		case HUD_NONE:    _hudVisibility = HUD_FULL;    break;
	}
	_visibility_dirty = true;
}


void HeadsUpDisplay::setVisible(bool aVisible)
{
	HudVisibility v = aVisible ? HUD_FULL : HUD_NONE;
	if (v != _hudVisibility)
	{
		_hudVisibility = v;
		_visibility_dirty = true;
	}
}


void HeadsUpDisplay::applyVisibility()
{
	switch (_hudVisibility)
	{
		case HUD_FULL:
			_text_switch->setAllChildrenOn();
			_status_switch->setAllChildrenOn();
			break;
		case HUD_MINIMAL:
			_text_switch->setAllChildrenOn();
			_status_switch->setAllChildrenOff();
			break;
		case HUD_NONE:
			_text_switch->setAllChildrenOff();
			_status_switch->setAllChildrenOff();
			break;
	}
}


void HeadsUpDisplay::update()
{
   if( _dirtytime )
   {
      char timestr[256];
      sprintf(timestr, "t = %-7.2f", _timevalue);
      _timetext->setText(timestr);
      _dirtytime = false;
   }


   if( _dirtystatus )
   {
		for(TStatusMap::iterator i = _statusmap.begin(); i!=_statusmap.end(); ++i)
		{
			std::ostringstream text;
			text << i->first << ": " << i->second._label;
			i->second._drawable->setText(text.str());
		}
      _dirtystatus = false;
   }


   if( _dirtytitle )
   {
      _titletext->setText(_titlestring);
      _dirtytitle = false;
   }

   if (_dirtytimeseries)
   {
		if (_linegraph)
		{
			_xfm->removeChild(_linegraph->getGeode());
			delete(_linegraph);
			_linegraph = NULL;
		}

		osg::FloatArray * fa = _graphdata._data.get();
		if (fa && fa->size()>1)
		{
			_linegraph = new LineGraph;
			_xfm->addChild(_linegraph->setUpScene(_graphdata._title, _graphdata._unit, fa, _graphdata._timelength, osg::Vec3(16.0f, ORTHO2D_HEIGHT*0.66f-24.0f, 0), osg::Vec2(ORTHO2D_WIDTH - 400, 400)));
		}

		_dirtytimeseries = false;
   }

	if (_visibility_dirty)
	{
		_visibility_dirty = false;
		applyVisibility();
	}
}

void HeadsUpDisplay::setTimeSeriesData(osg::ref_ptr<osg::FloatArray> aData, float aTimeLength, std::string aTitle, std::string aUnit)
{
	_dirtytimeseries = true;
	_graphdata._data = aData;
	_graphdata._timelength = aTimeLength;
	_graphdata._title = aTitle;
	_graphdata._unit = aUnit;
}


void HeadsUpDisplay::addStatusLine(const std::string & aLabel, osg::Geode* aParentGeode,
                                   const osg::Vec3& aPos, bool rightAlign)
{
	StatusData data;
	data._drawable = addText(aPos, *_font, false, rightAlign);
	_statusmap.insert(TStatusPair(aLabel, data));
	aParentGeode->addDrawable( data._drawable );
	_dirtystatus = true;
}
