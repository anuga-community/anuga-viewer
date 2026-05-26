

#include <bedslope.h>

#include <osg/Texture>
#include <osg/Texture2D>
#include <osg/PolygonMode>

#include <osgUtil/SmoothingVisitor>

// Bedslope colour when there is no texture
#define DEF_BEDSLOPE_COLOUR     (225.0f/255.0f), (190.0f/255.0f), (90.0f/255.0f), 1     // R, G, B, Alpha (brown)


// constructor
BedSlope::BedSlope(SWWReader* sww)
	: MeshObject("bedslope"),
	_tex2D(NULL),
	_loaded(false)
{
    // persistent
    _sww = sww;

    // bedslope starts untextured; caller uses setTextureImage() to apply a tile
	_texture = false;

	// material
	_material = new osg::Material();
	_material->setAmbient( osg::Material::FRONT_AND_BACK, osg::Vec4(0.2, 0.15, 0.06, 1.0) );
	_material->setDiffuse( osg::Material::FRONT_AND_BACK, osg::Vec4(0.6, 0.5, 0.2, 1.0) );
	_material->setSpecular( osg::Material::FRONT_AND_BACK, osg::Vec4(0.0, 0.0, 0.0, 1.0) );
	_material->setEmission( osg::Material::FRONT_AND_BACK, osg::Vec4(0.0, 0.0, 0.0, 1.0) );

	_material->setAmbient( osg::Material::FRONT_AND_BACK, osg::Vec4(0.0, 0.0, 0.0, 1.0) );

	_material->setSpecular( osg::Material::FRONT_AND_BACK, osg::Vec4(0.0, 0.0, 0.0, 1.0) );
	_material->setEmission( osg::Material::FRONT_AND_BACK, osg::Vec4(0.0, 0.0, 0.0, 1.0) );

	// state
	_stateset->setAttributeAndModes( _material, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE );
	_stateset->setMode( GL_BLEND, osg::StateAttribute::ON );
	_stateset->setMode( GL_LIGHTING, osg::StateAttribute::ON );
	onRefreshTextured(false);
}


void BedSlope::onRefreshData()
{
	if (!_sww->isElevationAnimated() && _loaded)
	{
		// it is already loaded and has a static mesh, so do nothing
		return;
	}

	// delete if exists
	if( _geom->getNumPrimitiveSets() )
	{
		_geom->removePrimitiveSet(0);  // reference counting does actual delete
	}

	// refresh data if file on disk has changed
	if ((_sww->refresh() == false) || (_sww->loadBedslopeVertexArray(_timestep) == false))
	{
		// error: could not reload file
		return;
	}

    // geometry from sww file
    osg::Vec3Array* vertices = _sww->getBedslopeVertexArray().get();
    _geom->setVertexArray( vertices );
    _geom->addPrimitiveSet( _sww->getBedslopeIndexArray().get() );

    osg::Vec4Array* color = new osg::Vec4Array(1);
    (*color)[0] = _texture ? osg::Vec4(1.0f, 1.0f, 1.0f, 1.0f)
                           : osg::Vec4(DEF_BEDSLOPE_COLOUR);
    _geom->setColorArray( color );
    _geom->setColorBinding( osg::Geometry::BIND_OVERALL );

    // Calculate per-vertex normals
    osgUtil::SmoothingVisitor* visitor = new osgUtil::SmoothingVisitor();
    visitor->smooth( *_geom );

	_loaded = true;

    // Re-apply texture state after geometry refresh (restores tex coords and material)
    onRefreshTextured(_texture);
}

void BedSlope::onRefreshTextured(bool aIsTextured)
{
	if (aIsTextured && _texture)
	{
		// Textured mesh has a different state set
		_material->setDiffuse( osg::Material::FRONT_AND_BACK, osg::Vec4(1.0, 1.0, 1.0, 1.0) );
		_geom->setTexCoordArray( 0, _sww->getBedslopeTextureCoords().get() );
	}
	else
	{
		_material->setDiffuse( osg::Material::FRONT_AND_BACK, osg::Vec4(DEF_BEDSLOPE_COLOUR) );
	}
}

void BedSlope::setTextureImage(osg::Image* img)
{
	if (img)
	{
		if (!_tex2D)
		{
			_tex2D = new osg::Texture2D;
			_tex2D->setDataVariance(osg::Object::DYNAMIC);
			_tex2D->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP);
			_tex2D->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP);
			// NPOT_HINT_PLACEHOLDER
		}
		_tex2D->setImage(img);
		_tex2D->dirtyTextureObject();
		_stateset->setTextureAttributeAndModes(0, _tex2D, osg::StateAttribute::ON);
		_texture = true;
	}
	else
	{
		if (_tex2D)
			_stateset->setTextureAttributeAndModes(0, _tex2D, osg::StateAttribute::OFF);
		_texture = false;
	}
	_loaded = false;   // force onRefreshData to rebuild geometry with correct tex coords / vertex colour
	forceRefresh();
}