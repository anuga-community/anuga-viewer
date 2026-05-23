
#include "osmtexture.h"
#include "swwreader.h"

#ifdef HAVE_GDAL

#include <gdal_priv.h>
#include <ogr_spatialref.h>
#include <gdalwarper.h>
#include <cpl_conv.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <sys/stat.h>

// Choose OSM zoom level so the terrain spans roughly 2048 px (8 tiles × 256).
static int chooseZoomLevel(double lonRange, double latRange)
{
    double maxRange = std::max(lonRange, latRange);
    if (maxRange <= 0.0) return 12;
    int z = (int)std::floor(std::log2(8.0 * 360.0 / maxRange));
    if (z <  5) z =  5;
    if (z > 15) z = 15;
    return z;
}

static bool fileExists(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

std::string fetchOSMTexture(SWWReader* sww, const std::string& outputPath)
{
    int zone = sww->getUTMZone();
    if (zone <= 0)
    {
        std::cout << "[OSM] No valid UTM zone in SWW file — skipping OSM fetch.\n";
        return "";
    }

    // Return cached file immediately if it already exists
    if (fileExists(outputPath))
    {
        std::cout << "[OSM] Using cached texture: " << outputPath << "\n";
        return outputPath;
    }

    bool south = sww->isSouthernHemisphere();
    int epsg   = south ? 32700 + zone : 32600 + zone;
    std::cout << "[OSM] UTM zone " << zone << (south ? "S" : "N")
              << "  EPSG:" << epsg << "\n";

    // Terrain bounding box in real-world UTM coords
    double xmin, xmax, ymin, ymax;
    sww->getTerrainBoundsUTM(xmin, xmax, ymin, ymax);

    // Transform all four UTM corners to WGS84 to pick tiles
    OGRSpatialReference utmSRS, wgs84SRS;
    utmSRS.importFromEPSG(epsg);
    wgs84SRS.importFromEPSG(4326);
    wgs84SRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    OGRCoordinateTransformation* toWGS84 =
        OGRCreateCoordinateTransformation(&utmSRS, &wgs84SRS);
    if (!toWGS84)
    {
        std::cerr << "[OSM] Failed to create UTM→WGS84 transform.\n";
        return "";
    }

    double xs[4] = { xmin, xmax, xmin, xmax };
    double ys[4] = { ymin, ymin, ymax, ymax };
    toWGS84->Transform(4, xs, ys);
    OGRCoordinateTransformation::DestroyCT(toWGS84);

    double lonMin = *std::min_element(xs, xs + 4);
    double lonMax = *std::max_element(xs, xs + 4);
    double latMin = *std::min_element(ys, ys + 4);
    double latMax = *std::max_element(ys, ys + 4);

    int zoom = chooseZoomLevel(lonMax - lonMin, latMax - latMin);
    std::cout << "[OSM] bbox WGS84: [" << lonMin << ", " << latMin
              << ", " << lonMax << ", " << latMax << "]  zoom=" << zoom << "\n";

    // Tile cache alongside the output GeoTIFF
    std::string tileCache = outputPath + "_tiles";
    VSIMkdir(tileCache.c_str(), 0755);

    // Build GDAL WMS XML for OSM TMS
    char wmsXML[4096];
    snprintf(wmsXML, sizeof(wmsXML),
        "<GDAL_WMS>"
          "<Service name=\"TMS\">"
            "<ServerUrl>https://tile.openstreetmap.org/${z}/${x}/${y}.png</ServerUrl>"
          "</Service>"
          "<DataWindow>"
            "<UpperLeftX>-20037508.34</UpperLeftX>"
            "<UpperLeftY>20037508.34</UpperLeftY>"
            "<LowerRightX>20037508.34</LowerRightX>"
            "<LowerRightY>-20037508.34</LowerRightY>"
            "<TileLevel>%d</TileLevel>"
            "<TileCountX>1</TileCountX>"
            "<TileCountY>1</TileCountY>"
            "<YOrigin>top</YOrigin>"
          "</DataWindow>"
          "<Projection>EPSG:3857</Projection>"
          "<BlockSizeX>256</BlockSizeX>"
          "<BlockSizeY>256</BlockSizeY>"
          "<BandsCount>3</BandsCount>"
          "<Cache><Path>%s</Path><Expires>604800</Expires></Cache>"
          "<UserAgent>anuga-viewer/0.4 (https://github.com/anuga-community/anuga-viewer)</UserAgent>"
          "<Timeout>30</Timeout>"
        "</GDAL_WMS>",
        zoom, tileCache.c_str());

    GDALAllRegister();
    GDALDataset* wmsDS = (GDALDataset*)GDALOpen(wmsXML, GA_ReadOnly);
    if (!wmsDS)
    {
        std::cerr << "[OSM] Failed to open WMS dataset — check network.\n";
        return "";
    }

    // Output GeoTIFF dimensions: ~2048 px wide, proportional height
    double utmW  = xmax - xmin;
    double utmH  = ymax - ymin;
    double margin = std::max(utmW, utmH) * 0.05; // 5% margin each side

    double outXMin = xmin - margin;
    double outYMax = ymax + margin;
    double outW_utm = utmW + 2.0 * margin;
    double outH_utm = utmH + 2.0 * margin;

    int pixW = 2048;
    int pixH = std::max(1, (int)(pixW * outH_utm / outW_utm + 0.5));

    double pixelW =  outW_utm / pixW;
    double pixelH = -outH_utm / pixH;   // negative = north-up

    double geoTransform[6] = { outXMin, pixelW, 0.0, outYMax, 0.0, pixelH };

    GDALDriver* gtiff = GetGDALDriverManager()->GetDriverByName("GTiff");
    GDALDataset* outDS = gtiff->Create(outputPath.c_str(), pixW, pixH, 3, GDT_Byte, nullptr);
    if (!outDS)
    {
        GDALClose(wmsDS);
        std::cerr << "[OSM] Failed to create output GeoTIFF: " << outputPath << "\n";
        return "";
    }

    outDS->SetGeoTransform(geoTransform);
    OGRSpatialReference outSRS;
    outSRS.importFromEPSG(epsg);
    char* wkt = nullptr;
    outSRS.exportToWkt(&wkt);
    outDS->SetProjection(wkt);
    CPLFree(wkt);

    // Warp OSM tiles (EPSG:3857) into the UTM output raster
    GDALWarpOptions* opts = GDALCreateWarpOptions();
    opts->hSrcDS      = wmsDS;
    opts->hDstDS      = outDS;
    opts->nBandCount  = 3;
    opts->panSrcBands = (int*)CPLMalloc(3 * sizeof(int));
    opts->panDstBands = (int*)CPLMalloc(3 * sizeof(int));
    for (int i = 0; i < 3; ++i) { opts->panSrcBands[i] = i + 1; opts->panDstBands[i] = i + 1; }
    opts->pfnProgress    = GDALTermProgress;
    opts->pTransformerArg = GDALCreateGenImgProjTransformer(
        wmsDS, nullptr, outDS, nullptr, FALSE, 0, 1);
    opts->pfnTransformer  = GDALGenImgProjTransform;

    GDALWarpOperation warp;
    warp.Initialize(opts);
    CPLErr err = warp.ChunkAndWarpImage(0, 0, pixW, pixH);

    GDALDestroyGenImgProjTransformer(opts->pTransformerArg);
    GDALDestroyWarpOptions(opts);
    GDALClose(outDS);
    GDALClose(wmsDS);

    if (err != CE_None)
    {
        std::cerr << "[OSM] Warp failed.\n";
        return "";
    }

    std::cout << "[OSM] Saved " << pixW << "×" << pixH
              << " px texture to " << outputPath << "\n";
    std::cout << "[OSM] Map data © OpenStreetMap contributors"
                 " — openstreetmap.org/copyright\n";
    return outputPath;
}

#else // !HAVE_GDAL

std::string fetchOSMTexture(SWWReader*, const std::string&)
{
    std::cout << "[OSM] GDAL not available — OSM fetch disabled.\n";
    return "";
}

#endif // HAVE_GDAL
