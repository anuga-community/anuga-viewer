
#include <sstream>
#include <iomanip>
#include <stdio.h>

#include <osg/io_utils>
#include <osg/Export>
#include <osg/BlendFunc>
#include <osg/LineWidth>
#include <osg/MatrixTransform>
#include <osgViewer/Renderer>
#include <osgViewer/Viewer>

#include <osg/PolygonMode>
#include <osg/Geometry>
#include <osgText/Text>

#include "project.h"
#include "hud.h"
#include "linegraph.h"

static const char * FONT_NAME = "fonts/arial.ttf";

// Colours — semi-transparent dark theme
static const osg::Vec4 COL_BACKGROUND(0.15f, 0.20f, 0.35f, 0.82f);
static const osg::Vec4 COL_TEXT(0.95f, 0.95f, 0.95f, 1.0f);
static const osg::Vec4 COL_GRID(0.40f, 0.43f, 0.52f, 1.0f);
static const osg::Vec4 COL_LINE(0.25f, 0.80f, 1.0f, 1.0f);

LineGraph::LineGraph():
	_geode(NULL)
{
}


osg::Geometry* LineGraph::createBackgroundRectangle(const osg::Vec3& pos, const float width, const float height, osg::Vec4& color)
{
    osg::Geometry* geometry = new osg::Geometry;
    geometry->setUseDisplayList(true);

    osg::Vec3Array* vertices = new osg::Vec3Array;
    geometry->setVertexArray(vertices);
    vertices->push_back(osg::Vec3(pos.x(),       pos.y(),        0));
    vertices->push_back(osg::Vec3(pos.x(),       pos.y()-height, 0));
    vertices->push_back(osg::Vec3(pos.x()+width, pos.y()-height, 0));
    vertices->push_back(osg::Vec3(pos.x()+width, pos.y(),        0));

    osg::Vec4Array* colors = new osg::Vec4Array;
    colors->push_back(color);
    geometry->setColorArray(colors);
    geometry->setColorBinding(osg::Geometry::BIND_OVERALL);

    osg::DrawElementsUInt* base = new osg::DrawElementsUInt(osg::PrimitiveSet::QUADS, 0);
    base->push_back(0); base->push_back(1); base->push_back(2); base->push_back(3);
    geometry->addPrimitiveSet(base);

    // Enable blending. Do NOT use TRANSPARENT_BIN — that would move the background
    // to a later render pass and paint it over the grid/data lines that follow it
    // in the geode. With depth test off (HUD), geode draw order is what matters.
    osg::StateSet* ss = geometry->getOrCreateStateSet();
    ss->setMode(GL_BLEND, osg::StateAttribute::ON);
    ss->setAttribute(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    return geometry;
}


osg::Geode * LineGraph::setUpScene(const std::string & aLabel, const std::string & aUnit,
	const osg::FloatArray * aData, float aTimeLength,
	const osg::Vec3 & aPos, const osg::Vec2 & aSize)
{
	assert(!_geode);

    const float charTitle  = 20.0f;
    const float charAxis   = 15.0f;
    const float margin     = 14.0f;

    osg::FloatArray* normalised_data = NULL;
    if (aData) {
        normalised_data = new osg::FloatArray(*aData, osg::CopyOp::DEEP_COPY_ALL);
    }

	_geode = new osg::Geode;

	osg::Vec3 pos = aPos;

    // --- background panel (drawn first so text renders on top)
    {
        float panelH = aSize.y() + charTitle * 3.5f + margin * 2;
        float panelW = aSize.x() + margin;
        osg::Vec4 bgCol = COL_BACKGROUND;
        _geode->addDrawable(createBackgroundRectangle(
            pos + osg::Vec3(-margin, charTitle * 2.2f + margin, 0),
            panelW, panelH, bgCol));
    }

    // --- title label
    {
        osg::ref_ptr<osgText::Text> t = new osgText::Text;
        _geode->addDrawable(t.get());
        t->setColor(COL_TEXT);
        t->setFont(FONT_NAME);
        t->setCharacterSize(charTitle);
        t->setPosition(pos);
        t->setText(aLabel);
        pos.y() -= charTitle * 1.6f;
    }

    // Compute data range
    float max_y = 0.0f, min_y = 0.0f;
    if (aData && !aData->empty())
    {
        min_y = (*aData)[0]; max_y = (*aData)[0];
        for (unsigned int i = 1; i < aData->size(); ++i)
        {
            if ((*aData)[i] > max_y) max_y = (*aData)[i];
            if ((*aData)[i] < min_y) min_y = (*aData)[i];
        }
        // Round to 2 d.p.
        max_y = ceil(max_y  * 100.0f) / 100.0f;
        min_y = floor(min_y * 100.0f) / 100.0f;

        float range_y = osg::maximum(max_y - min_y, 0.0001f);
        for (unsigned int i = 0; i < aData->size(); ++i)
            normalised_data->at(i) = ((*aData)[i] - min_y) / range_y;
    }

    const float graphW = aSize.x() - margin * 2;
    const float graphH = aSize.y() - margin * 2;

    // --- grid (border + 4 horizontal reference lines)
    _geode->addDrawable(createGraphGrid(pos, graphW, graphH, const_cast<osg::Vec4&>(COL_GRID), 4));

    // --- y-axis max label (top-left of graph)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.2f %s", max_y, aUnit.c_str());
        osg::ref_ptr<osgText::Text> t = new osgText::Text;
        _geode->addDrawable(t.get());
        t->setColor(COL_TEXT);
        t->setFont(FONT_NAME);
        t->setCharacterSize(charAxis);
        t->setPosition(pos + osg::Vec3(0, charAxis * 0.4f, 0));
        t->setText(buf);
    }

    // --- y-axis min label (bottom-left)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.2f %s", min_y, aUnit.c_str());
        osg::ref_ptr<osgText::Text> t = new osgText::Text;
        _geode->addDrawable(t.get());
        t->setColor(COL_TEXT);
        t->setFont(FONT_NAME);
        t->setCharacterSize(charAxis);
        t->setPosition(pos + osg::Vec3(0, -graphH - charAxis * 1.2f, 0));
        t->setText(buf);
    }

    // --- x-axis time label (bottom-right)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f s", aTimeLength);
        osg::ref_ptr<osgText::Text> t = new osgText::Text;
        _geode->addDrawable(t.get());
        t->setColor(COL_TEXT);
        t->setFont(FONT_NAME);
        t->setCharacterSize(charAxis);
        t->setAlignment(osgText::Text::RIGHT_BASE_LINE);
        t->setPosition(pos + osg::Vec3(graphW, -graphH - charAxis * 1.2f, 0));
        t->setText(buf);
    }

    // --- data line
    if (aData && normalised_data && aData->size() > 1)
    {
        _geode->addDrawable(createGraphGeometry(pos, graphW, graphH,
            COL_LINE, normalised_data));
    }

    _geode->setName("linegraph");
    return _geode;
}


osg::Geometry* LineGraph::createGraphGrid(const osg::Vec3& pos, const float width, const float height, osg::Vec4& color, int numHLines)
{
    osg::Geometry* geometry = new osg::Geometry;
    osg::Vec3Array* vertices = new osg::Vec3Array;

    // Border
    vertices->push_back(pos + osg::Vec3(0,     0,       0));
    vertices->push_back(pos + osg::Vec3(width, 0,       0));
    vertices->push_back(pos + osg::Vec3(width, 0,       0));
    vertices->push_back(pos + osg::Vec3(width, -height, 0));
    vertices->push_back(pos + osg::Vec3(width, -height, 0));
    vertices->push_back(pos + osg::Vec3(0,     -height, 0));
    vertices->push_back(pos + osg::Vec3(0,     -height, 0));
    vertices->push_back(pos + osg::Vec3(0,     0,       0));

    // Horizontal reference lines at 25%, 50%, 75%
    for (int i = 1; i < numHLines; ++i)
    {
        float y = -(float(i) / float(numHLines)) * height;
        vertices->push_back(pos + osg::Vec3(0,     y, 0));
        vertices->push_back(pos + osg::Vec3(width, y, 0));
    }

    osg::Vec4Array* colors = new osg::Vec4Array;
    colors->push_back(color);
    geometry->setColorArray(colors);
    geometry->setColorBinding(osg::Geometry::BIND_OVERALL);
    geometry->setVertexArray(vertices);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_LINES, 0, vertices->size()));

    osg::StateSet* ss = geometry->getOrCreateStateSet();
    ss->setAttribute(new osg::LineWidth(1.5f));

    return geometry;
}


osg::Geometry* LineGraph::createGraphGeometry(const osg::Vec3& pos, float width, float height,
    const osg::Vec4& colour, const osg::FloatArray* aData)
{
	assert(aData);

	osg::Geometry* geometry = new osg::Geometry;
    osg::Vec3Array* vertices = new osg::Vec3Array;
    geometry->setVertexArray(vertices);
    vertices->reserve(aData->size());

	float step = width / float(aData->size() - 1);
	for (unsigned int i = 0; i < aData->size(); ++i)
        vertices->push_back(pos + osg::Vec3(step * float(i), aData->at(i) * height - height, 0.0f));

    osg::Vec4Array* colours = new osg::Vec4Array;
    colours->push_back(colour);
    geometry->setColorArray(colours);
    geometry->setColorBinding(osg::Geometry::BIND_OVERALL);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_LINE_STRIP, 0, aData->size()));

    osg::StateSet* ss = geometry->getOrCreateStateSet();
    ss->setAttribute(new osg::LineWidth(2.5f));

    return geometry;
}


osg::Node * Axes_Create(const osg::BoundingBox & aBB)
{
	osg::Group * gr = new osg::Group;
	osg::ref_ptr<osg::MatrixTransform> mt = new osg::MatrixTransform;
	osg::MatrixTransform * mt_top = new osg::MatrixTransform;
	LineGraph lg;
	LineGraph lg2;

	assert(aBB.valid());

	osg::Matrix mat, mat2;
	mat.postMultScale(osg::Vec3(0.001, 0.001, 0.001));

	osg::Vec3 scale = aBB._max - aBB._min;
	scale._v[2] = osg::maximum(scale._v[2], 0.1f);

	mat.postMultScale(scale);
	mat.postMultTranslate(aBB.corner(2));
	mt_top->setMatrix(mat);

	mat = mat.rotate(osg::PI_2, 1,0,0);
	mat.postMultTranslate(osg::Vec3d(0, 0, scale._v[2]*1025));
	mt->setMatrix(mat);
	mt->addChild(lg2.setUpScene("Elevation", "m", NULL, 0, osg::Vec3(0,0,0), osg::Vec2(1000,1000)));

	gr->addChild(mt);
	gr->addChild(lg.setUpScene("Position", "m", NULL, 0, osg::Vec3(0,0,0), osg::Vec2(1000,1000)));

	mt_top->addChild(gr);

	osg::StateSet* stateset = new osg::StateSet;
	stateset->setMode( GL_LIGHTING, osg::StateAttribute::OFF );
	stateset->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
	mt_top->setStateSet( stateset );

	return mt_top;
}


osg::Geometry* IntensityBar_Create(const osg::Vec3& pos, const float width, const float height)
{
    osg::Geometry* geometry = new osg::Geometry;
    geometry->setUseDisplayList(true);

    osg::Vec3Array* vertices = new osg::Vec3Array;
    geometry->setVertexArray(vertices);

    vertices->push_back(osg::Vec3(pos.x(),       pos.y(),          0));
    vertices->push_back(osg::Vec3(pos.x(),       pos.y()-height/2, 0));
    vertices->push_back(osg::Vec3(pos.x()+width, pos.y()-height/2, 0));
    vertices->push_back(osg::Vec3(pos.x()+width, pos.y(),          0));
    vertices->push_back(osg::Vec3(pos.x(),       pos.y()-height/2, 0));
    vertices->push_back(osg::Vec3(pos.x(),       pos.y()-height,   0));
    vertices->push_back(osg::Vec3(pos.x()+width, pos.y()-height,   0));
    vertices->push_back(osg::Vec3(pos.x()+width, pos.y()-height/2, 0));

	osg::Vec4 r(1,0,0,1), g(0,1,0,1), b(0,0,1,1);
    osg::Vec4Array* colors = new osg::Vec4Array;
    colors->push_back(b); colors->push_back(g);
    colors->push_back(g); colors->push_back(b);
    colors->push_back(g); colors->push_back(r);
    colors->push_back(r); colors->push_back(g);
    geometry->setColorArray(colors);
	geometry->setColorBinding(osg::Geometry::BIND_PER_VERTEX);

    osg::DrawElementsUInt* base = new osg::DrawElementsUInt(osg::PrimitiveSet::QUADS, 0);
	for (int i = 0; i < 8; i++) base->push_back(i);
    geometry->addPrimitiveSet(base);

    return geometry;
}
