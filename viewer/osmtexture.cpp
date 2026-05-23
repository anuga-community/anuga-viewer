
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

// Target ~2048 px across (8 tiles × 256 px). Clamped to zoom 5–18.
static int chooseZoomLevel(double lonRange, double latRange)
{
    double maxRange = std::max(lonRange, latRange);
    if (maxRange <= 0.0) return 12;
    int z = (int)std::floor(std::log2(8.0 * 360.0 / maxRange));
    if (z <  5) z =  5;
    if (z > 18) z = 18;
    return z;
}

static bool fileExists(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

struct TileSourceConfig
{
    const char* label;
    const char* serverUrl;   // GDAL WMS ServerUrl (may contain ${z}/${x}/${y} or ${z}/${y}/${x})
    const char* attribution;
    int         maxZoom;
};

static const TileSourceConfig kSources[] = {
    {
        "OSM",
        "https://tile.openstreetmap.org/${z}/${x}/${y}.png",
        "Map data © OpenStreetMap contributors — openstreetmap.org/copyright",
        15
    },
    {
        "Satellite",
        "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/${z}/${y}/${x}",
        "Imagery © Esri, Maxar, Earthstar Geographics, and GIS User Community",
        18
    },
};

std::string fetchMapTexture(SWWReader* sww, const std::string& outputPath, MapTileSource source)
{
    const TileSourceConfig& cfg = kSources[static_cast<int>(source)];

    int zone = sww->getUTMZone();
    if (zone <= 0)
    {
        std::cout << "[" << cfg.label << "] No valid UTM zone in SWW file — skipping tile fetch.\n";
        return "";
    }

    if (fileExists(outputPath))
    {
        std::cout << "[" << cfg.label << "] Using cached texture: " << outputPath << "\n";
        return outputPath;
    }

    bool south = sww->isSouthernHemisphere();
    int epsg   = south ? 32700 + zone : 32600 + zone;
    std::cout << "[" << cfg.label << "] UTM zone " << zone << (south ? "S" : "N")
              << "  EPSG:" << epsg << "\n";

    double xmin, xmax, ymin, ymax;
    sww->getTerrainBoundsUTM(xmin, xmax, ymin, ymax);

    OGRSpatialReference utmSRS, wgs84SRS;
    utmSRS.importFromEPSG(epsg);
    wgs84SRS.importFromEPSG(4326);
    wgs84SRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    OGRCoordinateTransformation* toWGS84 =
        OGRCreateCoordinateTransformation(&utmSRS, &wgs84SRS);
    if (!toWGS84)
    {
        std::cerr << "[" << cfg.label << "] Failed to create UTM→WGS84 transform.\n";
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
    if (zoom > cfg.maxZoom) zoom = cfg.maxZoom;
    std::cout << "[" << cfg.label << "] bbox WGS84: [" << lonMin << ", " << latMin
              << ", " << lonMax << ", " << latMax << "]  zoom=" << zoom << "\n";

    std::string tileCache = outputPath + "_tiles";
    VSIMkdir(tileCache.c_str(), 0755);

    char wmsXML[4096];
    snprintf(wmsXML, sizeof(wmsXML),
        "<GDAL_WMS>"
          "<Service name=\"TMS\">"
            "<ServerUrl>%s</ServerUrl>"
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
        cfg.serverUrl, zoom, tileCache.c_str());

    GDALAllRegister();
    GDALDataset* wmsDS = (GDALDataset*)GDALOpen(wmsXML, GA_ReadOnly);
    if (!wmsDS)
    {
        std::cerr << "[" << cfg.label << "] Failed to open WMS dataset — check network.\n";
        return "";
    }

    double utmW   = xmax - xmin;
    double utmH   = ymax - ymin;
    double margin = std::max(utmW, utmH) * 0.05;

    double outXMin    = xmin - margin;
    double outYMax    = ymax + margin;
    double outW_utm   = utmW + 2.0 * margin;
    double outH_utm   = utmH + 2.0 * margin;

    int pixW = 2048;
    int pixH = std::max(1, (int)(pixW * outH_utm / outW_utm + 0.5));

    double pixelW =  outW_utm / pixW;
    double pixelH = -outH_utm / pixH;
    double geoTransform[6] = { outXMin, pixelW, 0.0, outYMax, 0.0, pixelH };

    GDALDriver* gtiff = GetGDALDriverManager()->GetDriverByName("GTiff");
    GDALDataset* outDS = gtiff->Create(outputPath.c_str(), pixW, pixH, 3, GDT_Byte, nullptr);
    if (!outDS)
    {
        GDALClose(wmsDS);
        std::cerr << "[" << cfg.label << "] Failed to create output GeoTIFF: " << outputPath << "\n";
        return "";
    }

    outDS->SetGeoTransform(geoTransform);
    OGRSpatialReference outSRS;
    outSRS.importFromEPSG(epsg);
    char* wkt = nullptr;
    outSRS.exportToWkt(&wkt);
    outDS->SetProjection(wkt);
    CPLFree(wkt);

    GDALWarpOptions* opts = GDALCreateWarpOptions();
    opts->hSrcDS      = wmsDS;
    opts->hDstDS      = outDS;
    opts->nBandCount  = 3;
    opts->panSrcBands = (int*)CPLMalloc(3 * sizeof(int));
    opts->panDstBands = (int*)CPLMalloc(3 * sizeof(int));
    for (int i = 0; i < 3; ++i) { opts->panSrcBands[i] = i + 1; opts->panDstBands[i] = i + 1; }
    opts->pfnProgress     = GDALTermProgress;
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
        std::cerr << "[" << cfg.label << "] Warp failed.\n";
        return "";
    }

    std::cout << "[" << cfg.label << "] Saved " << pixW << "×" << pixH
              << " px texture to " << outputPath << "\n";
    std::cout << "[" << cfg.label << "] " << cfg.attribution << "\n";
    return outputPath;
}

#else // !HAVE_GDAL

std::string fetchMapTexture(SWWReader*, const std::string&, MapTileSource)
{
    std::cout << "[tiles] GDAL not available — map tile fetch disabled.\n";
    return "";
}

#endif // HAVE_GDAL
