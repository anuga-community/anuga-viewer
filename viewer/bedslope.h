

/*
    BedSlope class

    An OpenSceneGraph viewer for ANUGA .sww files.
    copyright (C) 2004-2005 Geoscience Australia
*/


#ifndef BEDSLOPE_H
#define BEDSLOPE_H


#include <project.h>
#include <swwreader.h>
#include <osg/Geode>
#include <osg/Image>
#include <osg/Material>
#include <osg/StateAttribute>
#include <osg/Texture2D>

#include "meshobject.h"

/**
 * The static geometry in the scene.
 */
class BedSlope : public MeshObject
{

public:

    BedSlope(SWWReader *sww);
    osg::Geode* get(){ return _node; }
    osg::BoundingBox getBound(){ return _geom->Drawable::getBoundingBox(); }

	/**
	 * Called on mesh data refresh.
	 * Updates the mesh vertices with new data from disk.
	 */
	void onRefreshData();

	/**
	 * Called when the texture state is refreshed.
	 * Updates the texture state and vertex coloring.
	 * @param aIsTextured true if mesh is textured
	 */
	void onRefreshTextured(bool aIsTextured);

	/**
	 * Switch the bedslope texture at runtime.
	 * Pass nullptr to revert to the untextured brown appearance.
	 * UV coords are assumed identical for all tile textures (same UTM extent).
	 */
	void setTextureImage(osg::Image* img);

protected:

    osg::Material* _material;
    osg::Texture2D* _tex2D;

    virtual ~BedSlope(){;}
    bool _texture;
	bool _loaded;   // matches initializer order in constructor

};


#endif  // BEDSLOPE_H
