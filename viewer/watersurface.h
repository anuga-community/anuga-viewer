
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


};


#endif  // WATERSURFACE_H

