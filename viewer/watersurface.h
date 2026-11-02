
/*
    WaterSurface class

    An OpenSceneGraph viewer for ANUGA .sww files.
    Copyright (C) 2004, 2009 Geoscience Australia
*/


#ifndef WATERSURFACE_H
#define WATERSURFACE_H


#include <project.h>
#include <swwreader.h>
#include <osg/Geode>
#include <osg/StateAttribute>

#include "meshobject.h"

/**
 * An animating water surface mesh.
 */
class WaterSurface : public MeshObject
{

public:

    WaterSurface(SWWReader *sww);

    // Enable or disable the sphere-map environment texture (unit 1).
    // Disable it in colour modes so per-vertex stage colours are visible.
    void setEnvMapEnabled(bool enabled);

protected:

    virtual ~WaterSurface();
	
	void onRefreshData();


private:

   // deindexed geometry for the faceted data mode, reused between frames
   osg::ref_ptr<osg::Vec3Array> _facetedVertices;
   osg::ref_ptr<osg::Vec3Array> _facetedNormals;
   osg::ref_ptr<osg::Vec4Array> _facetedColors;
};


#endif  // WATERSURFACE_H

