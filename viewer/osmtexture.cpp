
// osmtexture.cpp — libcurl-based OSM/satellite tile fetching (no GDAL required)
//
// Projects each output UTM pixel → lat/lon via inverse Transverse Mercator,
// then samples the corresponding slippy-map tile.  Writes output as JPEG
// plus a .georef sidecar that swwreader uses for UV mapping.

#include "osmtexture.h"
#include "swwreader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#  include <direct.h>
#  define MAKE_DIR(p) _mkdir(p)
#  define DIR_SEP "\\"
#else
#  define MAKE_DIR(p) mkdir((p), 0755)
#  define DIR_SEP "/"
#endif

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

#ifdef HAVE_CURL

#include <curl/curl.h>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osg/Image>

// ---- WGS84 / UTM constants ----
static const double WA   = 6378137.0;
static const double WF   = 1.0 / 298.257223563;
static const double WE2  = 2*WF - WF*WF;
static const double WEP2 = WE2 / (1.0 - WE2);
static const double WK0  = 0.9996;
static const double WFE  = 500000.0;
static const double WFN  = 10000000.0;

// Inverse UTM: (easting, northing, zone, south) → (lat_rad, lon_rad)
static void utmToLatLon(double e, double n, int zone, bool south,
                        double& lat, double& lon)
{
    double x = e - WFE;
    double y = south ? n - WFN : n;

    double lon0 = (zone * 6.0 - 183.0) * M_PI / 180.0;
    double e1   = (1.0 - sqrt(1.0 - WE2)) / (1.0 + sqrt(1.0 - WE2));
    double M    = y / WK0;
    double mu   = M / (WA * (1.0 - WE2/4.0 - 3.0*WE2*WE2/64.0 - 5.0*WE2*WE2*WE2/256.0));

    double phi1 = mu
        + (3.0*e1/2.0        - 27.0*e1*e1*e1/32.0)        * sin(2.0*mu)
        + (21.0*e1*e1/16.0   - 55.0*e1*e1*e1*e1/32.0)     * sin(4.0*mu)
        + (151.0*e1*e1*e1/96.0)                            * sin(6.0*mu)
        + (1097.0*e1*e1*e1*e1/512.0)                       * sin(8.0*mu);

    double sp = sin(phi1), cp = cos(phi1), tp = tan(phi1);
    double N1 = WA / sqrt(1.0 - WE2*sp*sp);
    double T1 = tp*tp;
    double C1 = WEP2*cp*cp;
    double R1 = WA*(1.0-WE2) / pow(1.0 - WE2*sp*sp, 1.5);
    double D  = x / (N1*WK0);

    lat = phi1 - (N1*tp/R1) * (
          D*D/2.0
        - (5.0 + 3.0*T1 + 10.0*C1 - 4.0*C1*C1 - 9.0*WEP2)                * D*D*D*D/24.0
        + (61.0 + 90.0*T1 + 298.0*C1 + 45.0*T1*T1 - 252.0*WEP2 - 3.0*C1*C1) * D*D*D*D*D*D/720.0);

    lon = lon0 + (
          D
        - (1.0 + 2.0*T1 + C1)                                              * D*D*D/6.0
        + (5.0 - 2.0*C1 + 28.0*T1 - 3.0*C1*C1 + 8.0*WEP2 + 24.0*T1*T1)  * D*D*D*D*D/120.0)
        / cp;
}

// lat/lon (radians) → slippy-map tile index + sub-tile pixel offset [0, 256)
static void latLonToTile(double lat, double lon, int zoom,
                         int& tx, int& ty, double& fx, double& fy)
{
    int    n   = 1 << zoom;
    double xf  = (lon/M_PI + 1.0) * 0.5 * n;
    double sinL = sin(lat);
    double yf  = (1.0 - log((1.0 + sinL)/(1.0 - sinL)) / (2.0*M_PI)) * 0.5 * n;
    tx = (int)floor(xf); if (tx < 0) tx = 0; if (tx >= n) tx = n-1;
    ty = (int)floor(yf); if (ty < 0) ty = 0; if (ty >= n) ty = n-1;
    fx = (xf - tx) * 256.0;
    fy = (yf - ty) * 256.0;
}

// Target ~8 tiles wide; clamp to [5, maxZ].
static int chooseZoom(double lonRangeDeg, double latRangeDeg, int maxZ)
{
    double r = std::max(lonRangeDeg, latRangeDeg);
    if (r <= 0.0) return 12;
    int z = (int)floor(log2(8.0 * 360.0 / r));
    if (z <  5) z =  5;
    if (z > 18) z = 18;
    if (z > maxZ) z = maxZ;
    return z;
}

static bool fileExists(const std::string& p) { struct stat s; return stat(p.c_str(), &s) == 0; }

// Create directory and all parents (mkdir -p).
static void makeDirs(const std::string& path)
{
    for (size_t i = 1; i <= path.size(); i++) {
        if (i == path.size() || path[i] == '/' || path[i] == '\\') {
            MAKE_DIR(path.substr(0, i).c_str());
        }
    }
}

// Shared tile cache: ~/.cache/anuga-viewer/tiles/<source>/
// (Windows: %LOCALAPPDATA%\anuga-viewer\tiles\<source>\)
static std::string getTileCacheDir(const char* sourceLabel)
{
    std::string sub = sourceLabel;
    for (char& c : sub) c = (char)tolower((unsigned char)c);
#ifdef _WIN32
    const char* base = getenv("LOCALAPPDATA");
    if (!base || !base[0]) base = getenv("APPDATA");
    if (!base || !base[0]) return "." DIR_SEP "anuga-viewer-tiles" DIR_SEP + sub;
    return std::string(base) + "\\anuga-viewer\\tiles\\" + sub;
#else
    const char* home = getenv("HOME");
    if (!home || !home[0]) return "./.anuga-viewer-tiles/" + sub;
    return std::string(home) + "/.cache/anuga-viewer/tiles/" + sub;
#endif
}

static size_t curlWriteCB(void* ptr, size_t sz, size_t n, void* fp)
{
    return fwrite(ptr, sz, n, (FILE*)fp);
}

static bool downloadTile(const std::string& url, const std::string& path)
{
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    CURL* c = curl_easy_init();
    if (!c) { fclose(f); return false; }
    curl_easy_setopt(c, CURLOPT_URL,            url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION,  curlWriteCB);
    curl_easy_setopt(c, CURLOPT_WRITEDATA,      f);
    curl_easy_setopt(c, CURLOPT_USERAGENT,
        "anuga-viewer/0.5 (https://github.com/anuga-community/anuga-viewer)");
    curl_easy_setopt(c, CURLOPT_TIMEOUT,        30L);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    const char* ca = getenv("CURL_CA_BUNDLE");
    if (ca && ca[0]) curl_easy_setopt(c, CURLOPT_CAINFO, ca);
    CURLcode res = curl_easy_perform(c);
    long httpCode = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(c);
    fclose(f);
    if (res != CURLE_OK || httpCode != 200) {
        remove(path.c_str());
        if (res != CURLE_OK)
            std::cerr << "[tiles] curl error: " << curl_easy_strerror(res) << "\n";
        return false;
    }
    return true;
}

// Download tile and save with extension matching the actual content (PNG or JPEG).
// Returns the final path (possibly with .jpg instead of .png), or "" on failure.
static std::string fetchTile(const std::string& cacheDir, const std::string& url,
                             int zoom, int tx, int ty)
{
    char pngName[80], jpgName[80];
    snprintf(pngName, sizeof(pngName), "/%d_%d_%d.png", zoom, tx, ty);
    snprintf(jpgName, sizeof(jpgName), "/%d_%d_%d.jpg", zoom, tx, ty);
    std::string pngPath = cacheDir + pngName;
    std::string jpgPath = cacheDir + jpgName;

    if (fileExists(pngPath)) return pngPath;
    if (fileExists(jpgPath)) return jpgPath;

    // Download to a tmp path, then rename based on detected format
    char tmpName[80];
    snprintf(tmpName, sizeof(tmpName), "/%d_%d_%d.tmp", zoom, tx, ty);
    std::string tmpPath = cacheDir + tmpName;

    if (!downloadTile(url, tmpPath)) return "";

    // Detect JPEG vs PNG by magic bytes
    unsigned char hdr[4] = {};
    FILE* f = fopen(tmpPath.c_str(), "rb");
    if (f) { size_t nr = fread(hdr, 1, 4, f); (void)nr; fclose(f); }

    std::string finalPath = (hdr[0] == 0xFF && hdr[1] == 0xD8) ? jpgPath : pngPath;
    if (rename(tmpPath.c_str(), finalPath.c_str()) != 0) {
        remove(tmpPath.c_str());
        return "";
    }
    return finalPath;
}

struct TileSrc {
    const char* label;
    const char* urlTmpl;   // {z}/{x}/{y} or {z}/{y}/{x}
    const char* attribution;
    int         maxZoom;
};
static const TileSrc kSources[] = {
    {
        "OSM",
        "https://tile.openstreetmap.org/{z}/{x}/{y}.png",
        "Map data (c) OpenStreetMap contributors — openstreetmap.org/copyright",
        19
    },
    {
        "Satellite",
        "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}",
        "Imagery (c) Esri, Maxar, Earthstar Geographics, and GIS User Community",
        18
    },
};

static std::string buildUrl(const char* tmpl, int z, int x, int y)
{
    std::string s = tmpl;
    auto sub = [&](const char* tok, int v) {
        std::string vs = std::to_string(v);
        size_t pos;
        while ((pos = s.find(tok)) != std::string::npos) s.replace(pos, strlen(tok), vs);
    };
    sub("{z}", z); sub("{x}", x); sub("{y}", y);
    return s;
}

// Sample tile at fractional pixel (fx, fy measured from tile top-left).
// OSG loads PNGs with row 0 at the bottom, so we flip the row index.
static bool sampleTile(const std::string& cacheDir, const TileSrc& src,
                       int zoom, int tx, int ty, double fx, double fy,
                       std::map<std::pair<int,int>, osg::ref_ptr<osg::Image>>& cache,
                       unsigned char& r, unsigned char& g, unsigned char& b)
{
    auto key = std::make_pair(tx, ty);
    auto it  = cache.find(key);
    if (it == cache.end())
    {
        std::string url  = buildUrl(src.urlTmpl, zoom, tx, ty);
        std::string path = fetchTile(cacheDir, url, zoom, tx, ty);
        if (path.empty()) { cache[key] = nullptr; goto miss; }
        cache[key] = osgDB::readImageFile(path);
        it = cache.find(key);
    }
    {
        osg::Image* img = it->second.get();
        if (!img) goto miss;
        int px = (int)fx; if (px < 0) px = 0; if (px >= img->s()) px = img->s()-1;
        int py = (int)fy; if (py < 0) py = 0; if (py >= img->t()) py = img->t()-1;
        // fy=0 is tile top; OSG row 0 is bottom → flip
        int row  = img->t() - 1 - py;
        int nch  = img->getPixelSizeInBits() / 8;
        const unsigned char* p = img->data(px, row);
        r = p[0];
        g = (nch >= 2) ? p[1] : p[0];
        b = (nch >= 3) ? p[2] : p[0];
        return true;
    }
miss:
    r = g = b = 128; return false;
}

std::string fetchMapTexture(SWWReader* sww, const std::string& outputPath, MapTileSource source)
{
    static bool curlReady = false;
    if (!curlReady) { curl_global_init(CURL_GLOBAL_DEFAULT); curlReady = true; }

    const TileSrc& src = kSources[static_cast<int>(source)];

    int zone = sww->getUTMZone();
    if (zone <= 0) {
        std::cout << "[" << src.label << "] No UTM zone in SWW — skipping tile fetch.\n";
        return "";
    }

    bool south = sww->isSouthernHemisphere();

    double xmin, xmax, ymin, ymax;
    sww->getTerrainBoundsUTM(xmin, xmax, ymin, ymax);

    // 5% margin in UTM
    double mg    = std::max(xmax-xmin, ymax-ymin) * 0.05;
    double oxmin = xmin-mg, oxmax = xmax+mg;
    double oymin = ymin-mg, oymax = ymax+mg;
    double oW = oxmax-oxmin, oH = oymax-oymin;

    // UTM corners → lat/lon bbox
    double lats[4], lons[4];
    double cxs[4] = {oxmin,oxmax,oxmin,oxmax};
    double cys[4] = {oymin,oymin,oymax,oymax};
    for (int i = 0; i < 4; i++)
        utmToLatLon(cxs[i], cys[i], zone, south, lats[i], lons[i]);

    double latMin = *std::min_element(lats,lats+4);
    double latMax = *std::max_element(lats,lats+4);
    double lonMin = *std::min_element(lons,lons+4);
    double lonMax = *std::max_element(lons,lons+4);

    int zoom = chooseZoom((lonMax-lonMin)*180/M_PI, (latMax-latMin)*180/M_PI, src.maxZoom);

    // Tile index range covering bbox (latMax → smaller ty since tiles go top-down)
    int txMin, tyMin, txMax, tyMax; double dummy;
    latLonToTile(latMax, lonMin, zoom, txMin, tyMin, dummy, dummy);
    latLonToTile(latMin, lonMax, zoom, txMax, tyMax, dummy, dummy);
    if (txMin > txMax) std::swap(txMin, txMax);
    if (tyMin > tyMax) std::swap(tyMin, tyMax);
    int n = 1 << zoom;
    if (txMin < 0)  txMin = 0;
    if (txMax >= n) txMax = n-1;
    if (tyMin < 0)  tyMin = 0;
    if (tyMax >= n) tyMax = n-1;

    int tileCount = (txMax-txMin+1) * (tyMax-tyMin+1);
    if (tileCount > 400) {
        std::cerr << "[" << src.label << "] Too many tiles (" << tileCount << ") — aborting.\n";
        return "";
    }

    std::string cacheDir = getTileCacheDir(src.label);
    makeDirs(cacheDir);

    // Return early only when the stitched output exists AND every expected tile is
    // in the cache.  A partial download leaves the output looking blank in places;
    // falling through re-fetches only the missing tiles then re-stitches.
    if (fileExists(outputPath)) {
        bool complete = true;
        char tname[80];
        for (int ty = tyMin; ty <= tyMax && complete; ty++)
            for (int tx = txMin; tx <= txMax && complete; tx++) {
                snprintf(tname, sizeof(tname), "/%d_%d_%d", zoom, tx, ty);
                if (!fileExists(cacheDir + tname + ".png") &&
                    !fileExists(cacheDir + tname + ".jpg"))
                    complete = false;
            }
        if (complete) {
            std::cout << "[" << src.label << "] Using cached texture: " << outputPath << "\n";
            return outputPath;
        }
        std::cout << "[" << src.label << "] Incomplete tile cache — re-fetching missing tiles.\n";
    }

    std::cout << "[" << src.label << "] UTM zone " << zone << (south ? "S" : "N") << "\n";
    std::cout << "[" << src.label << "] bbox ["
              << lonMin*180/M_PI << ", " << latMin*180/M_PI << ", "
              << lonMax*180/M_PI << ", " << latMax*180/M_PI << "] deg\n";
    std::cout << "[" << src.label << "] " << tileCount
              << " tiles (" << (txMax-txMin+1) << "x" << (tyMax-tyMin+1)
              << ")  zoom=" << zoom << "\n";
    std::cout << "[" << src.label << "] tile cache: " << cacheDir << "\n";

    // Pre-fetch all tiles with per-tile progress, then stitch.
    std::map<std::pair<int,int>, osg::ref_ptr<osg::Image>> tileCache;
    int done = 0, failed = 0;
    for (int ty = tyMin; ty <= tyMax; ty++)
    {
        for (int tx = txMin; tx <= txMax; tx++)
        {
            ++done;
            std::cout << "\r[" << src.label << "] tile "
                      << std::setw(3) << done << "/" << tileCount
                      << "  " << std::flush;
            std::string url  = buildUrl(src.urlTmpl, zoom, tx, ty);
            std::string path = fetchTile(cacheDir, url, zoom, tx, ty);
            osg::ref_ptr<osg::Image> img;
            if (!path.empty()) img = osgDB::readImageFile(path);
            if (!img) failed++;
            tileCache[std::make_pair(tx, ty)] = img;
        }
    }
    std::cout << "\r[" << src.label << "] tiles done (" << (done - failed) << "/" << done << " ok)"
              << "  \n";

    if (failed == done) {
        std::cerr << "[" << src.label << "] All tile fetches failed — aborting.\n";
        return "";
    }

    // Output image: 2048 px wide
    int pixW = 2048;
    int pixH = std::max(1, (int)(pixW * oH / oW + 0.5));
    double pixelW = oW / pixW;
    double pixelH = oH / pixH;   // positive step downward in UTM

    std::cout << "[" << src.label << "] stitching " << pixW << "x" << pixH << " px...\n" << std::flush;

    osg::ref_ptr<osg::Image> outImg = new osg::Image();
    outImg->allocateImage(pixW, pixH, 1, GL_RGB, GL_UNSIGNED_BYTE);

    int misses = 0;
    for (int j = 0; j < pixH; j++)
    {
        // j=0 → north (oymax), increasing j → south
        double utm_y = oymax - (j + 0.5) * pixelH;
        for (int i = 0; i < pixW; i++)
        {
            double utm_x = oxmin + (i + 0.5) * pixelW;
            double lat, lon;
            utmToLatLon(utm_x, utm_y, zone, south, lat, lon);
            int tx, ty; double fx, fy;
            latLonToTile(lat, lon, zoom, tx, ty, fx, fy);

            // Store north (j=0) at OSG row pixH-1 so JPEG writer flips it to file row 0,
            // which OSG JPEG reader then maps back to OSG row pixH-1 (= OpenGL v=1 = north).
            unsigned char* p = outImg->data(i, pixH-1-j);
            unsigned char r, g, b;
            if (!sampleTile(cacheDir, src, zoom, tx, ty, fx, fy, tileCache, r, g, b))
                misses++;
            p[0] = r; p[1] = g; p[2] = b;
        }
    }

    if (misses > pixW * pixH / 4)
        std::cerr << "[" << src.label << "] Warning: many tile misses — texture may be incomplete.\n";

    if (!osgDB::writeImageFile(*outImg, outputPath)) {
        std::cerr << "[" << src.label << "] Failed to write: " << outputPath << "\n";
        return "";
    }

    // Sidecar georef for swwreader::setBedslopeTexture UV mapping.
    // Format: xorigin yorigin xpixel ypixel xresolution yresolution
    // Matches the GDAL GeoTransform convention (ypixel negative = downward).
    FILE* gf = fopen((outputPath + ".georef").c_str(), "w");
    if (gf) {
        fprintf(gf, "%.6f %.6f %.6f %.6f %d %d\n",
                oxmin, oymax, pixelW, -pixelH, pixW, pixH);
        fclose(gf);
    }

    std::cout << "[" << src.label << "] Saved " << pixW << "x" << pixH
              << " texture: " << outputPath << "\n";
    std::cout << "[" << src.label << "] " << src.attribution << "\n";
    return outputPath;
}

#else // !HAVE_CURL

std::string fetchMapTexture(SWWReader*, const std::string&, MapTileSource)
{
    std::cout << "[tiles] libcurl not available — map tile fetch disabled.\n";
    return "";
}

#endif // HAVE_CURL
