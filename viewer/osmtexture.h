#ifndef OSMTEXTURE_H
#define OSMTEXTURE_H

#include <string>

class SWWReader;

enum class MapTileSource { OSM, SATELLITE };

/**
 * Fetch map tiles for the terrain extent and warp them into a georeferenced
 * GeoTIFF in the terrain's UTM coordinate system.
 *
 * source == OSM:       OpenStreetMap street map (tile.openstreetmap.org)
 * source == SATELLITE: ESRI World Imagery satellite
 *
 * If outputPath already exists it is reused without re-downloading.
 * Returns outputPath on success, empty string on failure.
 *
 * Attribution:
 *   OSM:       map data © OpenStreetMap contributors (openstreetmap.org/copyright)
 *   Satellite: imagery © Esri, Maxar, Earthstar Geographics, and GIS User Community
 */
std::string fetchMapTexture(SWWReader* sww, const std::string& outputPath,
                            MapTileSource source = MapTileSource::OSM);

#endif // OSMTEXTURE_H
