#ifndef OSMTEXTURE_H
#define OSMTEXTURE_H

#include <string>

class SWWReader;

/**
 * Fetch OpenStreetMap tiles for the terrain extent and warp them into a
 * georeferenced GeoTIFF in the terrain's UTM coordinate system.
 *
 * If outputPath already exists it is reused without re-downloading.
 * Returns outputPath on success, empty string if the SWW has no valid UTM
 * zone or if the network/GDAL call fails.
 *
 * Attribution: map data © OpenStreetMap contributors
 * (https://www.openstreetmap.org/copyright)
 */
std::string fetchOSMTexture(SWWReader* sww, const std::string& outputPath);

#endif // OSMTEXTURE_H
