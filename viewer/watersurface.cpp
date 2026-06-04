/*
  WaterSurface class

  An OpenSceneGraph viewer for ANUGA .sww files.
  Copyright (C) 2004, 2009 Geoscience Australia
*/


#include <watersurface.h>
#include <osg/AlphaFunc>
#include <osg/BlendFunc>
#include <osg/ShadeModel>
#include <osg/ShapeDrawable>
#include <osg/Texture2D>
#include <osg/TexEnv>
#include <osg/TexGen>

#include <osgDB/ReadFile>

#define DEF_ALPHA_THRESHOLD 0.05


// constructor
WaterSurface::WaterSurface(SWWReader* sww)
	: MeshObject("watersurface")
{
   // persistent
   _sww = sww;

   // environment map
   osg::Texture2D* texture = new osg::Texture2D;
   texture->setDataVariance(osg::Object::DYNAMIC);
   texture->setBorderColor(osg::Vec4(1.0f,1.0f,1.0f,0.5f));
   texture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
   std::string* envmap = new std::string( _sww->getSwollenDir() + std::string("/images/envmap.jpg") );
   texture->setImage(osgDB::readImageFile( envmap->c_str() ));
   _stateset->setTextureAttributeAndModes( 1, texture, osg::StateAttribute::ON );
   _stateset->setMode( GL_LIGHTING, osg::StateAttribute::ON );

   // surface transparency
   osg::BlendFunc* osgBlendFunc = new osg::BlendFunc();
   osgBlendFunc->setFunction(osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE_MINUS_SRC_ALPHA);
   _stateset->setAttribute(osgBlendFunc);
   _stateset->setMode(GL_BLEND, osg::StateAttribute::ON);
   _stateset->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);

   // discard pixels with an alpha value below threshold
   osg::AlphaFunc* alphaFunc = new osg::AlphaFunc;
   alphaFunc->setFunction( osg::AlphaFunc::GREATER, DEF_ALPHA_THRESHOLD );
   _stateset->setAttributeAndModes( alphaFunc, osg::StateAttribute::ON );

   // automatically generate texture coords
   osg::TexGen* texgen = new osg::TexGen;
   texgen->setMode( osg::TexGen::SPHERE_MAP );

   osg::TexEnv* texenv = new osg::TexEnv;
   texenv->setMode( osg::TexEnv::DECAL );
   //texenv->setMode( osg::TexEnv::BLEND );
   texenv->setColor( osg::Vec4(0.6f,0.6f,0.6f,0.2f) );
   _stateset->setTextureAttributeAndModes( 1, texgen, osg::StateAttribute::ON );
   _stateset->setTextureAttribute( 1, texenv );

}



WaterSurface::~WaterSurface()
{
}


void WaterSurface::setEnvMapEnabled(bool enabled)
{
    _stateset->setTextureMode(1, GL_TEXTURE_2D,
        enabled ? osg::StateAttribute::ON
                : (osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE));
}


void WaterSurface::onRefreshData()
{
	// delete if exists
	if( _geom->getNumPrimitiveSets() )
	{
		_geom->removePrimitiveSet(0);  // reference counting does actual delete
	}

	// refresh data if file on disk has changed
	if ((_sww->refresh() == false) || (_sww->loadStageVertexArray(_timestep) == false))
	{
		// error: could not reload file
		return;
	}

	if (_sww->getDataMode() == SWWReader::DM_CENTROID_FACETED)
	{
		// Deindexed geometry: give each triangle its own 3 vertices so every corner
		// of a face carries the exact same centroid colour — no boundary bleed, no
		// interpolation.  Per-face normals give fully faceted lighting too.
		osg::ref_ptr<osg::Vec4Array>        facetedColors = _sww->getFacetedColorArray();
		osg::ref_ptr<osg::DrawElementsUInt> idx           = _sww->getBedslopeIndexArray();
		osg::ref_ptr<osg::Vec3Array>        srcVerts      = _sww->getStageVertexArray();

		if (facetedColors.valid() && idx.valid() && srcVerts.valid())
		{
			size_t ntri = idx->size() / 3;
			osg::ref_ptr<osg::Vec3Array> fv = new osg::Vec3Array(3 * ntri);
			osg::ref_ptr<osg::Vec3Array> fn = new osg::Vec3Array(3 * ntri);
			osg::ref_ptr<osg::Vec4Array> fc = new osg::Vec4Array(3 * ntri);

			for (size_t it = 0; it < ntri; it++)
			{
				unsigned int i0 = (*idx)[3*it+0];
				unsigned int i1 = (*idx)[3*it+1];
				unsigned int i2 = (*idx)[3*it+2];

				osg::Vec3 v0 = (*srcVerts)[i0];
				osg::Vec3 v1 = (*srcVerts)[i1];
				osg::Vec3 v2 = (*srcVerts)[i2];

				osg::Vec3 n = (v1 - v0) ^ (v2 - v0);
				n.normalize();

				(*fv)[3*it+0] = v0;  (*fv)[3*it+1] = v1;  (*fv)[3*it+2] = v2;
				(*fn)[3*it+0] = n;   (*fn)[3*it+1] = n;   (*fn)[3*it+2] = n;

				osg::Vec4 c = (*facetedColors)[it];
				(*fc)[3*it+0] = c;   (*fc)[3*it+1] = c;   (*fc)[3*it+2] = c;
			}

			_geom->setVertexArray(fv.get());
			_geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLES, 0, 3*ntri));
			_geom->setColorArray(fc.get());
			_geom->setColorBinding(osg::Geometry::BIND_PER_VERTEX);
			_geom->setNormalArray(fn.get());
			_geom->setNormalBinding(osg::Geometry::BIND_PER_VERTEX);
		}

		osg::ShadeModel* sm = new osg::ShadeModel;
		sm->setMode(osg::ShadeModel::SMOOTH);
		_stateset->setAttribute(sm);
	}
	else
	{
		// Shared-vertex indexed geometry — default path
		osg::ref_ptr<osg::Vec3Array> vertices      = _sww->getStageVertexArray();
		osg::ref_ptr<osg::Vec3Array> vertexnormals = _sww->getStageVertexNormalArray();
		osg::ref_ptr<osg::Vec4Array> colors        = _sww->getStageColorArray();

		_geom->setVertexArray(vertices.get());
		_geom->addPrimitiveSet(_sww->getBedslopeIndexArray().get());
		_geom->setColorArray(colors.get());
		_geom->setColorBinding(osg::Geometry::BIND_PER_VERTEX);
		_geom->setNormalArray(vertexnormals.get());
		_geom->setNormalBinding(osg::Geometry::BIND_PER_VERTEX);

		osg::ShadeModel* sm = new osg::ShadeModel;
		sm->setMode(osg::ShadeModel::SMOOTH);
		_stateset->setAttribute(sm);
	}
}
