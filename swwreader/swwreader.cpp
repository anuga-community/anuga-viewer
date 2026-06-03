/*
  SWWReader

  Reader of SWW files from within OpenSceneGraph.

  copyright (C) 2004-2005 Geoscience Australia
*/

#include <swwreader.h>
#include <cstdlib>
#include <string>
#include <fstream>
#include <vector>
#include <netcdf.h>
#include <osg/Notify>

#include <osgDB/Registry>
#include <osgDB/ReadFile>
#include <osgDB/FileNameUtils>

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_GDAL
#include <gdal_priv.h>
#endif

#define SAFE_DELETE_ARRAY(x) {if (x) { delete[](x); x=NULL;	}}

// Define this to use a fast square root algorithm - actual speed increases may depend on architecture
#define USE_FAST_SQRT

// compile time defaults
#define DEFAULT_CULLANGLE 85.0
#define DEFAULT_ALPHAMIN 0.8
#define DEFAULT_ALPHAMAX 1.0
#define DEFAULT_HEIGHTMIN 0.0
#define DEFAULT_HEIGHTMAX 1.0
#define DEFAULT_WETDEPTH 0.0
#define DEFAULT_BEDSLOPEOFFSET 0.0
#define DEFAULT_CULLONSTART false

#ifndef min
#define min(x, y) ((x<y) ? x:y)
#endif

#ifndef max
#define max(x, y) ((x>y) ? x:y)
#endif

#define getRange(aMin, aMax, x) { aMin = min(aMin, x);	aMax = max(aMax, x);	} 

#if 0
#include <windows.h>
#define PROFILE_BEGIN				\
	LARGE_INTEGER ticksPerSecond;	\
	LARGE_INTEGER tick_start;		\
	LARGE_INTEGER tick_end;			\
	QueryPerformanceFrequency(&ticksPerSecond);	\
	QueryPerformanceCounter(&tick_start);

#define PROFILE_END		\
	QueryPerformanceCounter(&tick_end);	\
	tick_end.QuadPart -= tick_start.QuadPart;	\
	tick_end.QuadPart /= ticksPerSecond.QuadPart/1000;	\
	printf("PROFILE(prepareTimestepImpl) %dms\n", tick_end.LowPart);
#else
#define PROFILE_BEGIN
#define PROFILE_END
#endif


#ifdef USE_FAST_SQRT
inline float Math_InvSqrtFast(float x)
{
	 float xhalf = 0.5f*x;
	 int i = *(int*)&x;
	 i = 0x5f3759df - (i>>1); // "magic number" that allows us to shift the exponent.
	 x = *(float*)&i;
	 return x*(1.5f - xhalf*x*x); // Newton method for closer approximation
}

inline void Math_NormalizeFast(osg::Vec3 & aVec)
{
	aVec *= Math_InvSqrtFast(aVec*aVec);
}
#endif


// only constructor, requires netcdf file
SWWReader::SWWReader(const std::string& filename) :
	_valid(false),
	_px(NULL),
	_py(NULL),
	_pz(NULL),
	_ptime(NULL),
	_xoffset(0),
	_yoffset(0),
	_zoffset(0),
	_pmaxdepth(NULL),
	_pmaxstage(NULL),
	_pmaxspeed(NULL),
	_pmaxmomentum(NULL),
	_elevationAnimated(false),
	_bedslopeLoaded(false),
	_vscale(1.0f),
	_zone(-1),
	_south(false)
{
PROFILE_BEGIN

	_fileChanged.watch(filename);

	// state initialization
	_state.bedslopetexturefilename = NULL;
	_state.stageoffset = 0.0f;
	_state.stageheightmax = 1.0f;

	// netcdf filename
	_state.swwfilename = new std::string(filename);

	if (load())
	{
		_valid = true;
	}

	PROFILE_END
}



SWWReader::~SWWReader()
{
	_status.push_back( nc_close(_ncid) );
	clear();
}



void SWWReader::setBedslopeTexture( std::string filename )
{
	osg::notify(osg::INFO) << "[SWWReader::setBedslopetexture] filename: " << filename <<  std::endl;
	_state.bedslopetexturefilename = new std::string(filename);
	_bedslopegeodata.hasData = false;

#ifdef HAVE_GDAL
	// GDAL Geospatial Image Library
	GDALDataset *pGDAL;
	GDALAllRegister();
	pGDAL = (GDALDataset *) GDALOpen( filename.c_str(), GA_ReadOnly );
	if( pGDAL == NULL )
		osg::notify(osg::INFO) << "[SWWReader::setBedslopetexture] GDAL unable to open file: " << filename <<  std::endl;
	else
	{
		osg::notify(osg::INFO) << "[SWWReader::setBedslopetexture] GDAL Driver: "
				  << pGDAL->GetDriver()->GetDescription() << "/"
				  << pGDAL->GetDriver()->GetMetadataItem( GDAL_DMD_LONGNAME )
				  << std::endl;

		_bedslopegeodata.xresolution = pGDAL->GetRasterXSize();
		_bedslopegeodata.yresolution = pGDAL->GetRasterYSize();
		osg::notify(osg::INFO) << "[SWWReader::setBedslopetexture] xresolution: " << _bedslopegeodata.xresolution <<  std::endl;
		osg::notify(osg::INFO) << "[SWWReader::setBedslopetexture] yresolution: " << _bedslopegeodata.yresolution <<  std::endl;

		// check if image contains geodata
		double geodata[6];
		if( pGDAL->GetGeoTransform( geodata ) == CE_None )
		{
			_bedslopegeodata.xorigin = geodata[0];
			_bedslopegeodata.yorigin = geodata[3];
			_bedslopegeodata.rotation = geodata[2];
			_bedslopegeodata.xpixel = geodata[1];
			_bedslopegeodata.ypixel = geodata[5];
			_bedslopegeodata.hasData = true;

			osg::notify(osg::INFO) << "[SWWReader::setBedslopetexture] xorigin: " << _bedslopegeodata.xorigin <<  std::endl;
			osg::notify(osg::INFO) << "[SWWReader::setBedslopetexture] yorigin: " << _bedslopegeodata.yorigin <<  std::endl;
			osg::notify(osg::INFO) << "[SWWReader::setBedslopetexture] xpixel: " << _bedslopegeodata.xpixel <<  std::endl;
			osg::notify(osg::INFO) << "[SWWReader::setBedslopetexture] ypixel: " << _bedslopegeodata.ypixel <<  std::endl;
			osg::notify(osg::INFO) << "[SWWReader::setBedslopetexture] rotation: " << _bedslopegeodata.rotation <<  std::endl;
		}
	}
#endif

	// Sidecar fallback: read a .georef file written by fetchMapTexture when GDAL
	// is unavailable or when the image format (e.g. JPEG) carries no geotransform.
	if (!_bedslopegeodata.hasData)
	{
		FILE* f = fopen((filename + ".georef").c_str(), "r");
		if (f)
		{
			float xo, yo, xp, yp;
			int   xr, yr;
			if (fscanf(f, "%f %f %f %f %d %d", &xo, &yo, &xp, &yp, &xr, &yr) == 6)
			{
				_bedslopegeodata.xorigin     = xo;
				_bedslopegeodata.yorigin     = yo;
				_bedslopegeodata.xpixel      = xp;
				_bedslopegeodata.ypixel      = yp;
				_bedslopegeodata.xresolution = xr;
				_bedslopegeodata.yresolution = yr;
				_bedslopegeodata.rotation    = 0.0f;
				_bedslopegeodata.hasData     = true;
				osg::notify(osg::INFO) << "[SWWReader::setBedslopetexture] loaded .georef sidecar\n";
			}
			fclose(f);
		}
	}
}



void SWWReader::getWetStageRange(float& outMin, float& outMax)
{
	outMin =  1e30f;
	outMax = -1e30f;

	if (!_pz || _ntimesteps == 0 || _npoints == 0)
		goto fallback;

	{
		std::vector<float> buf(_npoints);

		int ncid;
		if (nc_open(_state.swwfilename->c_str(), NC_NOWRITE, &ncid) != NC_NOERR)
			goto fallback;

		// Sample up to 20 evenly-spaced timesteps across the full simulation
		int nSamples = (int)_ntimesteps < 20 ? (int)_ntimesteps : 20;
		int step     = (int)_ntimesteps / nSamples;
		if (step < 1) step = 1;

		for (int ts = 0; ts < (int)_ntimesteps; ts += step)
		{
			size_t    start[2]  = {(size_t)ts, 0};
			size_t    count[2]  = {1, _npoints};
			ptrdiff_t stride[2] = {1, 1};
			if (nc_get_vars_float(ncid, _stageid, start, count, stride, buf.data()) != NC_NOERR)
				continue;

			for (size_t iv = 0; iv < _npoints; iv++)
			{
				if (buf[iv] - _pz[iv] > 0.001f)
				{
					if (buf[iv] < outMin) outMin = buf[iv];
					if (buf[iv] > outMax) outMax = buf[iv];
				}
			}
		}

		nc_close(ncid);
	}

	if (outMin <= outMax)
		return;

fallback:
	outMin = _state.stageoffset;
	outMax = _state.stageoffset + _state.stageheightmax;
}


osg::Image* SWWReader::getBedslopeTexture()
{
	return osgDB::readImageFile( _state.bedslopetexturefilename->c_str() );
}



osg::ref_ptr<osg::Vec2Array> SWWReader::getBedslopeTextureCoords()
{
	_bedslopetexcoords = new osg::Vec2Array;
	if( _bedslopegeodata.hasData )  // georeferenced bedslope texture
	{
		float xorigin = _bedslopegeodata.xorigin;
		float yorigin = _bedslopegeodata.yorigin;
		float xrange = _bedslopegeodata.xpixel * _bedslopegeodata.xresolution;
		float yrange = _bedslopegeodata.ypixel * _bedslopegeodata.yresolution;
		for( unsigned int iv=0; iv < _npoints; iv++ )
			_bedslopetexcoords->push_back( osg::Vec2( (_px[iv] + _xllcorner - xorigin)/xrange, 
																	1.0 - (_py[iv] + _yllcorner - yorigin)/yrange ) );
	}
	else
	{
		// decaled using (x,y) locations scaled by extents into range [0,1]
		for( unsigned int iv=0; iv < _npoints; iv++ )
			_bedslopetexcoords->push_back( osg::Vec2( (_px[iv]-_xoffset)*_xscale, (_py[iv]-_yoffset)*_yscale ) );
	}

	return _bedslopetexcoords;
}


void SWWReader::getTerrainBoundsUTM(double& xmin, double& xmax, double& ymin, double& ymax) const
{
	xmin = xmax = _px[0] + _xllcorner;
	ymin = ymax = _py[0] + _yllcorner;
	for (size_t i = 1; i < _npoints; ++i)
	{
		double x = _px[i] + _xllcorner;
		double y = _py[i] + _yllcorner;
		if (x < xmin) xmin = x;
		if (x > xmax) xmax = x;
		if (y < ymin) ymin = y;
		if (y > ymax) ymax = y;
	}
}


float SWWReader::getActualMaxDepth()
{
	if (!_pmaxdepth || _npoints == 0) return 0.0f;
	float v = 0.0f;
	for (size_t i = 0; i < _npoints; i++) if (_pmaxdepth[i] > v) v = _pmaxdepth[i];
	return v;
}

float SWWReader::getActualMaxSpeed()
{
	if (!_pmaxspeed || _npoints == 0) return 0.0f;
	float v = 0.0f;
	for (size_t i = 0; i < _npoints; i++) if (_pmaxspeed[i] > v) v = _pmaxspeed[i];
	return v;
}

float SWWReader::getActualMaxMomentum()
{
	if (!_pmaxmomentum || _npoints == 0) return 0.0f;
	float v = 0.0f;
	for (size_t i = 0; i < _npoints; i++) if (_pmaxmomentum[i] > v) v = _pmaxmomentum[i];
	return v;
}

bool SWWReader::loadBedslopeVertexArray(unsigned int aIndex)
{
	// netcdf open
	_status.push_back( nc_open(_state.swwfilename->c_str(), NC_NOWRITE, &_ncid) );
	if (this->_statusHasError())
	{
		return false;
	}

	float * pz = loadBedslopeZ(aIndex);
	assert(pz);

	getBedslopeBoundingVolume(pz);

	// bedslope vertex array, shifting and scaling vertices to unit cube
	// centred about the origin
	_bedslopevertices = new osg::Vec3Array;
	_bedslopevertices->reserve(_npoints);

	for (unsigned int iv=0; iv < _npoints; iv++)
	{
		_bedslopevertices->push_back( osg::Vec3( (_px[iv]-_xoffset)*_scale - _xcenter, 
															  (_py[iv]-_yoffset)*_scale - _ycenter, 
															  (pz[iv]-_zoffset)*_scale - _zcenter - DEFAULT_BEDSLOPEOFFSET) );
	}

	// calculate bedslope primitive normal and centroid arrays
	_bedslopenormals = new osg::Vec3Array;
	_bedslopecentroids = new osg::Vec3Array;

	_bedslopenormals->reserve(_nvolumes);
	_bedslopecentroids->reserve(_nvolumes);

	osg::Vec3 v1, v2, v3, side1, side2, nrm;
	for (unsigned int iv=0; iv < _nvolumes; iv++)
	{
		unsigned int v1index = _pvolumes[3*iv+0];
		unsigned int v2index = _pvolumes[3*iv+1];
		unsigned int v3index = _pvolumes[3*iv+2];

		if ((v1index >= _npoints) || (v2index >= _npoints) || (v3index >= _npoints))
		{
			// data out of bounds
			_status.push_back( nc_close(_ncid) );
			return false;
		}

		v1 = _bedslopevertices->at(v1index);
		v2 = _bedslopevertices->at(v2index);
		v3 = _bedslopevertices->at(v3index);

		side1 = v2 - v1;
		side2 = v3 - v2;
		side1.z() *= _vscale;
		side2.z() *= _vscale;
		nrm = side1^side2;
		nrm.normalize();
		nrm.z() *= _vscale;  // pre-correct for GL inverse-transpose of scale(1,1,vscale)

		_bedslopenormals->push_back( nrm );
		_bedslopecentroids->push_back( (v1+v2+v3)/3.0 );
	}

	_status.push_back( nc_close(_ncid) );

	if (_statusHasError())
	{
		return false;
	}


	return true;
}


bool SWWReader::loadStageVertexArray(unsigned int index)
{
	PROFILE_BEGIN

	assert(_bedslopevertices);

	size_t start[2], count[2], iv;
	const ptrdiff_t stride[2] = {1,1};
	start[0] = index;
	start[1] = 0;
	count[0] = 1;
	count[1] = _npoints;

	// netcdf open
	_status.push_back( nc_open(_state.swwfilename->c_str(), NC_NOWRITE, &_ncid) );
	if (this->_statusHasError())
	{
		return false;
	}

	// --- Check that the stage data hasn't shrunk
	size_t npoints = 0;
	_status.push_back( nc_inq_dimlen(_ncid, _npointsid, &npoints) );
	if (_statusHasError() || (npoints != _npoints))
	{
		// Our indices will not be out of bounds
		osg::notify(osg::FATAL) << "File changes have made it invalid! Please wait." <<  std::endl;
		return false;
	}

		// empty array for storing list of steep triangles
		osg::ref_ptr<osg::IntArray> steeptri = new osg::IntArray;

	// stage heights from netcdf file (x and y are same as bedslope)
	_status.push_back(nc_get_vars_float (_ncid, _stageid, start, count, stride, _pstage));

	if (_pxmomentum && _pymomentum)
	{
		// stage momentum from netcdf file (x and y are same as bedslope)
		_status.push_back(nc_get_vars_float (_ncid, _xmomentumid, start, count, stride, _pxmomentum));
		_status.push_back(nc_get_vars_float (_ncid, _ymomentumid, start, count, stride, _pymomentum));
	}

	_status.push_back( nc_close(_ncid) );

	// load stage vertex array, scaling and shifting vertices to lie in the unit cube
	_stagevertices = new osg::Vec3Array;
	_stagevertices->reserve(_npoints);

	ColorMode cm0 = _state.colorMode;
	bool isMaxMode = (cm0 == CM_MAX_DEPTH || cm0 == CM_MAX_SPEED ||
	                  cm0 == CM_MAX_MOMENTUM || cm0 == CM_MAX_STAGE) && _pmaxstage;
	for (iv=0; iv < _npoints; iv++)
	{
		float stage_z = isMaxMode ? _pmaxstage[iv] : _pstage[iv];
		_stagevertices->push_back( osg::Vec3( (_px[iv]-_xoffset)*_scale - _xcenter,
														  (_py[iv]-_yoffset)*_scale - _ycenter,
														  (stage_z-_zoffset)*_scale - _zcenter) );
	}

	// stage index, per primitive normal and centroid arrays
	_stageprimitivenormals = new osg::Vec3Array;
	_stageprimitivenormals->reserve(_nvolumes);
	osg::Vec3 v1b, v2b, v3b;
	osg::Vec3 v1s, v2s, v3s;
	osg::Vec3 side1, side2, nrm;
	unsigned int v1index, v2index, v3index;

	// cullangle given in degrees, test is against dot product
	float cullthreshold = cos(osg::DegreesToRadians(_state.cullangle));

	// over all stage triangles
	for (iv=0; iv < _nvolumes; iv++)
	{
		v1index = _pvolumes[3*iv+0];
		v2index = _pvolumes[3*iv+1];
		v3index = _pvolumes[3*iv+2];

		v1s = _stagevertices->at(v1index);
		v2s = _stagevertices->at(v2index);
		v3s = _stagevertices->at(v3index);

		// current triangle primitive normal in world-space (z scaled for correct direction)
		side1 = v2s - v1s;
		side2 = v3s - v2s;
		side1.z() *= _vscale;
		side2.z() *= _vscale;
		nrm = side1^side2;
		nrm.normalize();

		// store world-space normal (z pre-correction applied at per-vertex stage)
		_stageprimitivenormals->push_back( nrm );

		// identify steep triangles, store index
		if( fabs(nrm * osg::Vec3f(0,0,1)) < cullthreshold )
			steeptri->push_back( iv );
	}

	// Alpha: either wetdepth ramp (real metres) or legacy height ramp (model units).
	// wetdepth > 0: alpha ramps linearly from 0 at dry to alphamax at wetdepth metres,
	//               then stays at alphamax above that depth.
	// wetdepth == 0: alpha = alphamin + (alphamax-alphamin)*(height-hmin)/(hmax-hmin),
	//                clamped to [0, alphamax], where height is in model units.
	float alpha, height, alphascale;
	alphascale = (_state.alphamax - _state.alphamin) / (_state.heightmax - _state.heightmin);
	_stagecolors = new osg::Vec4Array;
	_stagecolors->reserve(_npoints);
	for (iv=0; iv < _npoints; iv++)
	{
		float depth_m = _pstage[iv] - _pz[iv];

		if (_state.wetdepth > 0.0f)
		{
			if (depth_m <= 0.0f)
				alpha = 0.0f;
			else if (depth_m < _state.wetdepth)
				alpha = (depth_m / _state.wetdepth) * _state.alphamax;
			else
				alpha = _state.alphamax;
		}
		else
		{
			height = _stagevertices->at(iv).z() - _bedslopevertices->at(iv).z();
			if (height < _state.heightmin)
				alpha = 0.0f;
			else
			{
				alpha = alphascale * (height - _state.heightmin) + _state.alphamin;
				if (alpha > _state.alphamax)
					alpha = _state.alphamax;
			}
		}

	  {
		ColorMode cm = _state.colorMode;

		if (cm == CM_BLUE)
		{
			// flat steel-blue: natural water look, no rainbow gradient
			_stagecolors->push_back( osg::Vec4( 0.40f, 0.52f, 0.70f, alpha ) );
		}
		else
		{
			float intens = 0.0f;
			if (cm == CM_MAX_DEPTH || cm == CM_MAX_SPEED || cm == CM_MAX_MOMENTUM || cm == CM_MAX_STAGE)
			{
				if (cm == CM_MAX_DEPTH)
					intens = _pmaxdepth ? min(1.0f, _pmaxdepth[iv] / _state.heightmax) : 0.0f;
				else if (cm == CM_MAX_SPEED)
					intens = _pmaxspeed ? min(1.0f, _pmaxspeed[iv] / _state.speedmax) : 0.0f;
				else if (cm == CM_MAX_MOMENTUM)
					intens = _pmaxmomentum ? min(1.0f, _pmaxmomentum[iv] / _state.momentummax) : 0.0f;
				else  // CM_MAX_STAGE: absolute elevation mapped to [stageoffset, stageoffset+stageheightmax]
					intens = _pmaxstage ? min(1.0f, max(0.0f, (_pmaxstage[iv] - _state.stageoffset) / _state.stageheightmax)) : 0.0f;
			}
			else if (cm == CM_DEPTH)
			{
				intens = (depth_m > 0.001f) ? min(1.0f, depth_m / _state.heightmax) : 0.0f;
			}
			else if (cm == CM_STAGE)
			{
				intens = (depth_m > 0.001f) ? min(1.0f, max(0.0f, (_pstage[iv] - _state.stageoffset) / _state.stageheightmax)) : 0.0f;
			}
			else if (_pxmomentum && _pymomentum)
			{
				if (cm == CM_SPEED)
				{
					if (depth_m > 0.001f)
						intens = min(1.0f, sqrt(_pxmomentum[iv]*_pxmomentum[iv]+_pymomentum[iv]*_pymomentum[iv]) / depth_m / _state.speedmax);
				}
				else  // CM_MOMENTUM
				{
					intens = min(1.0f, sqrt(_pxmomentum[iv]*_pxmomentum[iv]+_pymomentum[iv]*_pymomentum[iv]) / _state.momentummax);
				}
			}
			_stagecolors->push_back( osg::Vec4( intens, (0.5f-fabs(intens-0.5f))*2, 1.0f-intens, alpha ) );
		}
	  }
	}

	// steep triangle vertices should have alpha=0, overwrite such vertex colours
	if( _state.culling )
	{
		//std::cout << "culling on step: " << index << "  steeptris: " << steeptri->size() << std::endl;
		for (iv=0; iv < steeptri->size() ; iv++)
		{
			int tri = steeptri->at(iv);
			v1index = _pvolumes[3*tri+0];
			v2index = _pvolumes[3*tri+1];
			v3index = _pvolumes[3*tri+2];

			_stagecolors->at(v1index) = osg::Vec4( 1, 1, 1, 0 );
			_stagecolors->at(v2index) = osg::Vec4( 1, 1, 1, 0 );
			_stagecolors->at(v3index) = osg::Vec4( 1, 1, 1, 0 );
		}
	}

	// per-vertex normals calculated as average of primitive normals
	// from contributing triangles
	_stagevertexnormals = new osg::Vec3Array;
	_stagevertexnormals->reserve(_npoints);

	int num_shared_triangles, triangle_index;
	for (iv=0; iv < _npoints; iv++)
	{
		nrm.set(0,0,0);
		num_shared_triangles = _connectivity.at(iv).size();	// There may be 2-7 triangles sharing a vertex

		for (int i=0; i<num_shared_triangles; i++ )
		{
			triangle_index = _connectivity.at(iv).at(i);
			nrm += _stageprimitivenormals->at(triangle_index);
		}

		nrm = nrm / num_shared_triangles;  // average
		nrm.normalize();
		nrm.z() *= _vscale;  // pre-correct for GL inverse-transpose of scale(1,1,vscale)

		_stagevertexnormals->push_back(nrm);
	}

	PROFILE_END

	return true;
}


bool SWWReader::getTimeSeries(unsigned int aPolyIndex, TimeSeriesType aPlotType, osg::ref_ptr<osg::FloatArray> aData)
{
	PROFILE_BEGIN

	// we could get an average of 3 plots here, but it's probably overkill
	int stage_index = _pvolumes[aPolyIndex*3];


	size_t start[2], count[2];
	const ptrdiff_t stride[2] = {1,1};
	start[0] = 0;
	start[1] = stage_index;
	count[0] = _ntimesteps;
	count[1] = 1;

	// netcdf open
	_status.push_back( nc_open(_state.swwfilename->c_str(), NC_NOWRITE, &_ncid) );
		if (this->_statusHasError()) return false;

	aData->resize(count[0]);

	switch (aPlotType)
	{
		case TSTYPE_MOMENTUM_MAGNITUDE:
		{
			if (!_pxmomentum || !_pymomentum)
			{
				break;
			}

			osg::ref_ptr<osg::FloatArray> xmom = new osg::FloatArray;
			osg::ref_ptr<osg::FloatArray> ymom = new osg::FloatArray;

			xmom->resize(count[0]);
			ymom->resize(count[0]);

			// momentum from netcdf file (x and y are same as bedslope)
			_status.push_back(nc_get_vars_float (_ncid, _xmomentumid, start, count, stride, (float*)xmom->getDataPointer()));
			_status.push_back(nc_get_vars_float (_ncid, _ymomentumid, start, count, stride, (float*)ymom->getDataPointer()));

			for (int i=0; i<(int)aData->size(); i++)
			{
				aData->at(i) = sqrt(xmom->at(i)*xmom->at(i)+ymom->at(i)*ymom->at(i));
			}
			

			break;
		}

		case TSTYPE_STAGE:
		default:
		{
			_status.push_back(nc_get_vars_float (_ncid, _stageid, start, count, stride, (float*)aData->getDataPointer()));
			break;
		}

		case TSTYPE_DEPTH:
		{
			_status.push_back(nc_get_vars_float (_ncid, _stageid, start, count, stride, (float*)aData->getDataPointer()));
			float bed_z = _pz[stage_index];
			for (int i = 0; i < (int)aData->size(); i++)
				aData->at(i) = osg::maximum(0.0f, aData->at(i) - bed_z);
			break;
		}

		case TSTYPE_SPEED:
		{
			if (!_pxmomentum || !_pymomentum) break;

			osg::ref_ptr<osg::FloatArray> xmom = new osg::FloatArray;
			osg::ref_ptr<osg::FloatArray> ymom = new osg::FloatArray;
			xmom->resize(count[0]);
			ymom->resize(count[0]);

			_status.push_back(nc_get_vars_float (_ncid, _stageid,     start, count, stride, (float*)aData->getDataPointer()));
			_status.push_back(nc_get_vars_float (_ncid, _xmomentumid, start, count, stride, (float*)xmom->getDataPointer()));
			_status.push_back(nc_get_vars_float (_ncid, _ymomentumid, start, count, stride, (float*)ymom->getDataPointer()));

			float bed_z = _pz[stage_index];
			for (int i = 0; i < (int)aData->size(); i++)
			{
				float depth = aData->at(i) - bed_z;
				if (depth > 0.001f)
				{
					float mom = sqrt(xmom->at(i)*xmom->at(i) + ymom->at(i)*ymom->at(i));
					aData->at(i) = mom / depth;
				}
				else
					aData->at(i) = 0.0f;
			}
			break;
		}
	}

	_status.push_back( nc_close(_ncid) );

	if (_statusHasError())
	{
		return false;
	}

	PROFILE_END

	return true;
}



bool SWWReader::_statusHasError()
{
	bool haserror = false;  // assume success, trap failure

	std::vector<int>::iterator iter;
	for (iter=_status.begin(); iter != _status.end(); iter++)
	{
		if (*iter != NC_NOERR)
		{
			osg::notify(osg::FATAL) << "Error: " << nc_strerror(*iter) <<  std::endl;
			haserror = true;
			nc_close(_ncid);
			break;
		}
	}

	// on return start gathering result values afresh
	_status.clear();

	return haserror;
}


bool SWWReader::refresh()
{
	if (_fileChanged.isChanged())
	{
		// Preserve user-adjusted colour range across reload; load() calls
		// loadBedslopeVertexArray(0) with _bedslopeLoaded=false which would
		// otherwise overwrite stageoffset/stageheightmax with raw terrain bounds.
		float savedOffset    = _state.stageoffset;
		float savedHeightMax = _state.stageheightmax;
		clear();
		bool result = load();
		if (result)
		{
			_state.stageoffset    = savedOffset;
			_state.stageheightmax = savedHeightMax;
		}
		return result;
	}

	return true;
}


void SWWReader::clear()
{
	_valid = false;
	_bedslopeLoaded = false;

	SAFE_DELETE_ARRAY(_pxmomentum);
	SAFE_DELETE_ARRAY(_pymomentum);
	SAFE_DELETE_ARRAY(_px);
	SAFE_DELETE_ARRAY(_py);
	SAFE_DELETE_ARRAY(_pz);
	SAFE_DELETE_ARRAY(_ptime);
	SAFE_DELETE_ARRAY(_pvolumes);
	SAFE_DELETE_ARRAY(_pstage);
	SAFE_DELETE_ARRAY(_pmaxdepth);
	SAFE_DELETE_ARRAY(_pmaxstage);
	SAFE_DELETE_ARRAY(_pmaxspeed);
	SAFE_DELETE_ARRAY(_pmaxmomentum);
}


bool SWWReader::load()
{
	if (!_state.swwfilename)
	{
		return false;
	}

	// netcdf open
	_status.push_back( nc_open(_state.swwfilename->c_str(), NC_NOWRITE, &_ncid) );
	if (this->_statusHasError())
	{
		return false;
	}

	// dimension ids
	_status.push_back( nc_inq_dimid(_ncid, "number_of_volumes", &_nvolumesid) );
	_status.push_back( nc_inq_dimid(_ncid, "number_of_vertices", &_nverticesid) );
	_status.push_back( nc_inq_dimid(_ncid, "number_of_points", &_npointsid) );
	_status.push_back( nc_inq_dimid(_ncid, "number_of_timesteps", &_ntimestepsid) );
	if (this->_statusHasError()) return false;

	// dimension values
	_status.push_back( nc_inq_dimlen(_ncid, _nvolumesid, &_nvolumes) );
	_status.push_back( nc_inq_dimlen(_ncid, _nverticesid, &_nvertices) );
	_status.push_back( nc_inq_dimlen(_ncid, _npointsid, &_npoints) );
	_status.push_back( nc_inq_dimlen(_ncid, _ntimestepsid, &_ntimesteps) );
	if (this->_statusHasError()) return false;

	// variable ids
	_status.push_back( nc_inq_varid (_ncid, "x", &_xid) );
	_status.push_back( nc_inq_varid (_ncid, "y", &_yid) );

	if (nc_inq_varid (_ncid, "elevation", &_zid) != NC_NOERR)
	{
		// If we failed to find the elevation var, this is likely to be an old-style file.
		osg::notify(osg::INFO) << "[SWWReader] Warning: variable 'elevation' not found. Trying old format 'z' instead." << std::endl;
		_status.push_back( nc_inq_varid (_ncid, "z", &_zid) );
	}
	else
	{
		// now we check for multiple bedslope frames
		int num_dims;
		_status.push_back(nc_inq_varndims(_ncid, _zid, &num_dims));
		if (num_dims == 2)
		{
			osg::notify(osg::INFO) << "[SWWReader] Animated elevation data found. " << std::endl;
			_elevationAnimated = true;
		}
		else
		{
			osg::notify(osg::INFO) << "[SWWReader] Static elevation data only. " <<  std::endl;
			assert(num_dims == 1 && "Wrong num of dimensions for elevation data - unknown file format!");
		}

		_status.push_back( NC_NOERR );
	}

	_status.push_back( nc_inq_varid (_ncid, "volumes", &_volumesid) );
	_status.push_back( nc_inq_varid (_ncid, "time", &_timeid) );
	_status.push_back( nc_inq_varid (_ncid, "stage", &_stageid) );
	if (this->_statusHasError())
	{
		return false;
	}

	// --- Look for momentum data
	if ((nc_inq_varid (_ncid, "xmomentum", &_xmomentumid) != NC_NOERR) ||
		(nc_inq_varid (_ncid, "ymomentum", &_ymomentumid) != NC_NOERR)) 
	{
		osg::notify(osg::INFO) << "[SWWReader] No momentum data found." << std::endl;
		_pxmomentum = NULL;
		_pymomentum = NULL;
	}
	else
	{
		_pxmomentum = new float[_npoints];
		_pymomentum = new float[_npoints];
	}


	// allocation of variable arrays, destructor responsible for cleanup
	_px = new float[_npoints];
	_py = new float[_npoints];
	_pz = new float[_npoints];	// bedslope z

	_ptime = new float[_ntimesteps];
	_pvolumes = new unsigned int[_nvertices * _nvolumes];
	_pstage = new float[_npoints];

	// loading variables from netcdf file
	_status.push_back( nc_get_var_float (_ncid, _xid, _px) );  // x vertices
	_status.push_back( nc_get_var_float (_ncid, _yid, _py) );  // y vertices
	
	_status.push_back( nc_get_var_float (_ncid, _timeid, _ptime) );  // time array
	_status.push_back( nc_get_var_int (_ncid, _volumesid, (int *) _pvolumes) );  // triangle indices

	if (this->_statusHasError()) return false;


	osg::notify(osg::INFO) << "[SWWReader] number of volumes: " << _nvolumes <<  std::endl;
	osg::notify(osg::INFO) << "[SWWReader] number of vertices: " << _nvertices <<  std::endl;
	osg::notify(osg::INFO) << "[SWWReader] number of points: " << _npoints <<  std::endl;
	osg::notify(osg::INFO) << "[SWWReader] number of timesteps: " << _ntimesteps <<  std::endl;

	// Georeference offset and UTM zone — must be read before nc_close
	_xllcorner = 0.0;
	_yllcorner = 0.0;
	{
		float xll, yll;
		if (nc_get_att_float(_ncid, NC_GLOBAL, "xllcorner", &xll) == NC_NOERR &&
		    nc_get_att_float(_ncid, NC_GLOBAL, "yllcorner", &yll) == NC_NOERR)
		{
			_xllcorner = xll;
			_yllcorner = yll;
			osg::notify(osg::INFO) << "[SWWReader] xllcorner: " << _xllcorner << std::endl;
			osg::notify(osg::INFO) << "[SWWReader] yllcorner: " << _yllcorner << std::endl;
		}
	}

	_zone = -1;
	_south = false;
	{
		int zoneVal = -1;
		if (nc_get_att_int(_ncid, NC_GLOBAL, "zone", &zoneVal) == NC_NOERR && zoneVal > 0)
			_zone = zoneVal;

		size_t hemLen = 0;
		if (_zone > 0 && nc_inq_attlen(_ncid, NC_GLOBAL, "hemisphere", &hemLen) == NC_NOERR && hemLen > 0)
		{
			std::string hem(hemLen, '\0');
			nc_get_att_text(_ncid, NC_GLOBAL, "hemisphere", &hem[0]);
			_south = (hem[0] == 'S' || hem[0] == 's');
		}
		if (_zone > 0)
			osg::notify(osg::INFO) << "[SWWReader] UTM zone: " << _zone << (_south ? "S" : "N") << std::endl;
	}

	// --- close file, we have finished with it
	_status.push_back( nc_close(_ncid) );

	// sww file can optionally contain bedslope texture image filename
	size_t attlen; // length of text attribute (if it exists)
	if( nc_inq_attlen(_ncid, NC_GLOBAL, "texture", &attlen) != NC_ENOTATT )
	{
		std::string texfilename;
		int status;
		status = nc_get_att_text(_ncid, NC_GLOBAL, "texture", (char*)texfilename.c_str());
		if( status == NC_NOERR )
		{
			texfilename[attlen] = '\0';  // ensure string is terminated, not a requirement for netcdf attributes
			osg::notify(osg::INFO) << "[SWWReader] embedded image filename: " << texfilename <<  std::endl;

			// if sww isn't in current directory, need to prepend sww path to the bedslope texture
			if( osgDB::getFilePath(*_state.swwfilename) == "" )
			{
				setBedslopeTexture(texfilename);
			}
			else
			{
				setBedslopeTexture( osgDB::getFilePath(*_state.swwfilename) + std::string("/") + texfilename);
			}
		}
	}



	// alpha-scaling defaults, can be overridden after construction by command line parameters
	_state.alphamin = DEFAULT_ALPHAMIN;
	_state.alphamax = DEFAULT_ALPHAMAX;
	_state.heightmin = DEFAULT_HEIGHTMIN;
	_state.heightmax = DEFAULT_HEIGHTMAX;
	_state.wetdepth = DEFAULT_WETDEPTH;

	// steepness culling default, can be overridden after construction by command line parameter
	_state.cullangle = DEFAULT_CULLANGLE;
	_state.culling = DEFAULT_CULLONSTART;

	_state.colorMode = CM_MOMENTUM;
	_state.speedmax = 5.0f;    // m/s at which speed colormap saturates
	_state.momentummax = 2.0f; // m²/s at which momentum colormap saturates

	// loop index
	size_t iv;
	// vertex indices
	unsigned int v1index, v2index, v3index;

	// compute triangle connectivity, a list (indexed by vertex number) 
	// of lists (indices of triangles sharing this vertex)
	_connectivity = std::vector<triangle_list>(_npoints);
	for (iv=0; iv < _nvolumes; iv++)
	{
		v1index = _pvolumes[3*iv+0];
		v2index = _pvolumes[3*iv+1];
		v3index = _pvolumes[3*iv+2];

		if (v1index<_npoints)
		{
			_connectivity.at(v1index).push_back(iv);
		}

		if (v2index<_npoints)
		{
			_connectivity.at(v2index).push_back(iv);
		}

		if (v3index<_npoints)
		{
			_connectivity.at(v3index).push_back(iv);
		}
	}


	// bedslope index array, pvolumes array indexes into x, y and z
	_bedslopeindices = new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES, _nvolumes*_nvertices);
	for (iv=0; iv < _nvolumes*_nvertices; iv++)
		(*_bedslopeindices)[iv] = _pvolumes[iv];


	if (_statusHasError())
	{
		return false;
	}

	// Load initial frame
	loadBedslopeVertexArray(0);

	// Pre-compute per-vertex maxima — use precomputed centroid values from
	// the SWW file if available, otherwise scan all timesteps.
	if (!tryLoadCentroidMaxima())
		computeMaxima();

	return true;
}


bool SWWReader::tryLoadCentroidMaxima()
{
	int ncid;
	if (nc_open(_state.swwfilename->c_str(), NC_NOWRITE, &ncid) != NC_NOERR)
		return false;

	// All four centroid max variables must be present; bail if any is missing.
	int id_ms, id_md, id_msp, id_mu;
	if (nc_inq_varid(ncid, "max_stage_c", &id_ms)  != NC_NOERR ||
	    nc_inq_varid(ncid, "max_depth_c", &id_md)  != NC_NOERR ||
	    nc_inq_varid(ncid, "max_speed_c", &id_msp) != NC_NOERR ||
	    nc_inq_varid(ncid, "max_uh_c",    &id_mu)  != NC_NOERR)
	{
		nc_close(ncid);
		return false;
	}

	// Read one value per triangle (centroid quantities).
	std::vector<float> cstage(_nvolumes);
	std::vector<float> cdepth(_nvolumes);
	std::vector<float> cspeed(_nvolumes);
	std::vector<float> cuh(_nvolumes);

	if (nc_get_var_float(ncid, id_ms,  cstage.data()) != NC_NOERR ||
	    nc_get_var_float(ncid, id_md,  cdepth.data()) != NC_NOERR ||
	    nc_get_var_float(ncid, id_msp, cspeed.data()) != NC_NOERR ||
	    nc_get_var_float(ncid, id_mu,  cuh.data())    != NC_NOERR)
	{
		nc_close(ncid);
		return false;
	}
	nc_close(ncid);

	// Expand centroid values to per-vertex by averaging over the triangles
	// that share each vertex (uses _connectivity built just before this call).
	_pmaxstage    = new float[_npoints]();
	_pmaxdepth    = new float[_npoints]();
	_pmaxspeed    = new float[_npoints]();
	_pmaxmomentum = new float[_npoints]();

	std::vector<int> cnt(_npoints, 0);
	for (size_t tri = 0; tri < _nvolumes; tri++)
	{
		for (int k = 0; k < 3; k++)
		{
			unsigned int vi = _pvolumes[3*tri + k];
			if (vi < _npoints)
			{
				_pmaxstage[vi]    += cstage[tri];
				_pmaxdepth[vi]    += cdepth[tri];
				_pmaxspeed[vi]    += cspeed[tri];
				_pmaxmomentum[vi] += cuh[tri];
				cnt[vi]++;
			}
		}
	}

	for (size_t iv = 0; iv < _npoints; iv++)
	{
		if (cnt[iv] > 0)
		{
			float inv = 1.0f / cnt[iv];
			_pmaxstage[iv]    *= inv;
			_pmaxdepth[iv]    *= inv;
			_pmaxspeed[iv]    *= inv;
			_pmaxmomentum[iv] *= inv;
		}
		if (_pmaxdepth[iv] < 0.0f)
			_pmaxdepth[iv] = 0.0f;
	}

	osg::notify(osg::INFO) << "[SWWReader] loaded precomputed centroid maxima from SWW file" << std::endl;
	return true;
}


void SWWReader::computeMaxima()
{
	_pmaxdepth    = new float[_npoints]();
	_pmaxstage    = new float[_npoints];
	_pmaxspeed    = new float[_npoints]();
	_pmaxmomentum = new float[_npoints]();

	// Initialise max stage to a very negative value so any real stage wins
	for (size_t iv = 0; iv < _npoints; iv++)
		_pmaxstage[iv] = -1e30f;

	size_t start[2] = {0, 0};
	size_t count[2] = {1, _npoints};
	const ptrdiff_t stride[2] = {1, 1};

	int ncid;
	if (nc_open(_state.swwfilename->c_str(), NC_NOWRITE, &ncid) != NC_NOERR)
		return;

	for (size_t t = 0; t < _ntimesteps; t++)
	{
		start[0] = t;
		nc_get_vars_float(ncid, _stageid, start, count, stride, _pstage);

		bool hasMom = (_pxmomentum && _pymomentum);
		if (hasMom)
		{
			nc_get_vars_float(ncid, _xmomentumid, start, count, stride, _pxmomentum);
			nc_get_vars_float(ncid, _ymomentumid, start, count, stride, _pymomentum);
		}

		for (size_t iv = 0; iv < _npoints; iv++)
		{
			float depth = _pstage[iv] - _pz[iv];
			if (depth > _pmaxdepth[iv])    _pmaxdepth[iv] = depth;
			if (_pstage[iv] > _pmaxstage[iv]) _pmaxstage[iv] = _pstage[iv];

			if (hasMom)
			{
				float mom = sqrt(_pxmomentum[iv]*_pxmomentum[iv] + _pymomentum[iv]*_pymomentum[iv]);
				if (mom > _pmaxmomentum[iv]) _pmaxmomentum[iv] = mom;
				if (depth > 0.001f)
				{
					float spd = mom / depth;
					if (spd > _pmaxspeed[iv]) _pmaxspeed[iv] = spd;
				}
			}
		}
	}

	nc_close(ncid);

	// Floor negative max depths at zero (never-flooded cells)
	for (size_t iv = 0; iv < _npoints; iv++)
		if (_pmaxdepth[iv] < 0.0f) _pmaxdepth[iv] = 0.0f;

	osg::notify(osg::INFO) << "[SWWReader] computeMaxima done" << std::endl;
}


float SWWReader::getTime(unsigned int index)
{
	if ((!_ptime) || (index >= getNumberOfTimesteps()))
	{
		// invalid data
		return 0.0f;
	}

	return *(_ptime+index);
}


void SWWReader::getBedslopeBoundingVolume(const float * aZ)
{
	// bedslope range and resultant scale factors (note, these don't take into
	// account any xllcorner, yllcorner offsets - used during texture coord assignment)
	float xmin, xmax, xrange;
	float ymin, ymax, yrange;
	float zmin, zmax, zrange;
	float aspect_ratio;

	assert(aZ);

	xmin = xmax = _px[0];
	ymin = ymax = _py[0];
	zmin = zmax =  aZ[0];
	for(unsigned int iv=1; iv < _npoints; iv++ )
	{
		getRange(xmin, xmax, _px[iv]);
		getRange(ymin, ymax, _py[iv]);
		getRange(zmin, zmax, aZ[iv]);
	}

	xrange = xmax - xmin;
	yrange = ymax - ymin;
	zrange = zmax - zmin;
	aspect_ratio = xrange/yrange;
	_xscale = 1.0 / xrange;
	_yscale = 1.0 / yrange;
	 
	// handle case of a flat bed that doesn't necessarily pass through z=0
	_zscale = (zrange == 0.0) ? 1.0 : 1.0/zrange;

	_xoffset = xmin;
	_yoffset = ymin;
	_zoffset = zmin;
	if (!_bedslopeLoaded)
	{
		// Only set on first load; subsequent calls (e.g. bedslope texture change) must not
		// overwrite stageoffset/stageheightmax that the user has already adjusted.
		_state.stageoffset = (float)zmin;
		_state.stageheightmax = (zrange > 0.0f) ? zrange : 1.0f;
	}
	_bedslopeLoaded = true;
	_xcenter = 0.5;
	_ycenter = 0.5;
	_zcenter = 0.0;

	if( aspect_ratio > 1.0 )
	{
		_scale = 1.0/xrange;
		_ycenter /= aspect_ratio;
	}
	else
	{
		_scale = 1.0/yrange;
		_xcenter *= aspect_ratio;
	}

	osg::notify(osg::INFO) << "[SWWReader] xmin: " << xmin <<  std::endl;
	osg::notify(osg::INFO) << "[SWWReader] xmax: " << xmax <<  std::endl;
	osg::notify(osg::INFO) << "[SWWReader] xrange: " << xrange <<  std::endl;
	osg::notify(osg::INFO) << "[SWWReader] xscale: " << _xscale <<  std::endl;
	osg::notify(osg::INFO) << "[SWWReader] xcenter: " << _xcenter <<  std::endl;
	osg::notify(osg::INFO) <<  std::endl;

	osg::notify(osg::INFO) << "[SWWReader] ymin: " << ymin <<  std::endl;
	osg::notify(osg::INFO) << "[SWWReader] ymax: " << ymax <<  std::endl;
	osg::notify(osg::INFO) << "[SWWReader] yrange: " << yrange <<  std::endl;
	osg::notify(osg::INFO) << "[SWWReader] yscale: " << _yscale <<  std::endl;
	osg::notify(osg::INFO) << "[SWWReader] ycenter: " << _ycenter <<  std::endl;
	osg::notify(osg::INFO) <<  std::endl;

	osg::notify(osg::INFO) << "[SWWReader] zmin: " << zmin <<  std::endl;
	osg::notify(osg::INFO) << "[SWWReader] zmax: " << zmax <<  std::endl;
	osg::notify(osg::INFO) << "[SWWReader] zrange: " << zrange <<  std::endl;
	osg::notify(osg::INFO) << "[SWWReader] zscale: " << _zscale <<  std::endl;
	osg::notify(osg::INFO) << "[SWWReader] zcenter: " << _zcenter <<  std::endl;
	osg::notify(osg::INFO) <<  std::endl;
}


float * SWWReader::loadBedslopeZ(unsigned int aTimestep)
{
	assert(_pz);

	// --- Check that the stage data hasn't shrunk
	size_t npoints = 0;
	_status.push_back( nc_inq_dimlen(_ncid, _npointsid, &npoints) );
	if (_statusHasError() || (npoints != _npoints))
	{
		// Our indices will not be out of bounds
		osg::notify(osg::FATAL) << "File changes have made it invalid! Please wait." <<  std::endl;
		return NULL;
	}

	if (!_elevationAnimated)
	{
		// Static bedslope, never changes
		_status.push_back( nc_get_var_float (_ncid, _zid, _pz) );
		return _pz;
	}

	size_t start[2], count[2];
	const ptrdiff_t stride[2] = {1,1};
	start[0] = aTimestep;
	start[1] = 0;
	count[0] = 1;
	count[1] = _npoints;

	// bedslope elevation from netcdf file
	_status.push_back(nc_get_vars_float (_ncid, _zid, start, count, stride, _pz));

	if (_statusHasError())
	{
		// Our indices will not be out of bounds
		osg::notify(osg::FATAL) << "Could not load variable bed elevation - check file format." <<  std::endl;
		return NULL;
	}

	return _pz;
}
