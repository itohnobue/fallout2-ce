#include "light.h"

#include <algorithm>

#include "db.h"
#include "map_defs.h"
#include "object.h"
#include "perk.h"
#include "perk_tweak.h"
#include "tile.h"

namespace fallout {

// 0x51923C ambient_light
static int gAmbientIntensity = LIGHT_INTENSITY_MAX;

// light intensity per elevation per tile
// 0x59E994 tile_intensity
static int gTileIntensity[ELEVATION_COUNT][HEX_GRID_SIZE];

// 0x47A8F0 light_init
int lightInit()
{
    lightResetTileIntensity();
    return 0;
}

// 0x47A8F0 light_init
void lightReset()
{
    lightResetTileIntensity();
}

// 0x47A8F0 light_init
void lightExit()
{
    lightResetTileIntensity();
}

// 0x47A8F8 light_get_ambient
int lightGetAmbientIntensity()
{
    return gAmbientIntensity;
}

// 0x47A908 light_set_ambient
void lightSetAmbientIntensity(int intensity, bool shouldUpdateScreen)
{
    // PerksTweak NightVisionBonus: percent of max light per rank.
    // Default 20 == the legacy LIGHT_LEVEL_NIGHT_VISION_BONUS (65536/5).
    int adjustedIntensity = intensity + perkGetRank(gDude, PERK_NIGHT_VISION) * (LIGHT_INTENSITY_MAX * gPerkTweak.nightVisionBonus / 100);
    int normalizedIntensity = std::clamp(adjustedIntensity, LIGHT_INTENSITY_MIN, LIGHT_INTENSITY_MAX);

    int oldAmbientIntensity = gAmbientIntensity;
    gAmbientIntensity = normalizedIntensity;

    if (shouldUpdateScreen) {
        if (oldAmbientIntensity != normalizedIntensity) {
            tileWindowRefresh();
        }
    }
}

// 0x47A980 light_get_tile
int lightGetTileIntensity(int elevation, int tile)
{
    if (!elevationIsValid(elevation)) {
        return 0;
    }

    if (!hexGridTileIsValid(tile)) {
        return 0;
    }

    return std::min(gTileIntensity[elevation][tile], LIGHT_INTENSITY_MAX);
}

// 0x47A9C4 light_get_tile_true
int lightGetTrueTileIntensity(int elevation, int tile)
{
    if (!elevationIsValid(elevation)) {
        return 0;
    }

    if (!hexGridTileIsValid(tile)) {
        return 0;
    }

    return gTileIntensity[elevation][tile];
}

// 0x47A9EC light_set_tile
void lightSetTileIntensity(int elevation, int tile, int intensity)
{
    if (!elevationIsValid(elevation)) {
        return;
    }

    if (!hexGridTileIsValid(tile)) {
        return;
    }

    gTileIntensity[elevation][tile] = intensity;
}

// 0x47AA10 light_add_to_tile
void lightIncreaseTileIntensity(int elevation, int tile, int intensity)
{
    if (!elevationIsValid(elevation)) {
        return;
    }

    if (!hexGridTileIsValid(tile)) {
        return;
    }

    gTileIntensity[elevation][tile] += intensity;
}

// 0x47AA48 light_subtract_from_tile
void lightDecreaseTileIntensity(int elevation, int tile, int intensity)
{
    if (!elevationIsValid(elevation)) {
        return;
    }

    if (!hexGridTileIsValid(tile)) {
        return;
    }

    gTileIntensity[elevation][tile] -= intensity;
}

// 0x47AA84 light_reset_tiles
void lightResetTileIntensity()
{
    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        for (int tile = 0; tile < HEX_GRID_SIZE; tile++) {
            gTileIntensity[elevation][tile] = 655;
        }
    }
}

void lightDecreaseAmbient(int val)
{
    lightSetAmbientIntensity(gAmbientIntensity - val, true);
}

void lightIncreaseAmbient(int val)
{
    lightSetAmbientIntensity(gAmbientIntensity + val, true);
}

// 0x000000 light_save — persist ambient + tile light state to save file
int lightSave(File* stream)
{
    if (fileWriteInt32(stream, gAmbientIntensity) == -1) return -1;

    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        for (int tile = 0; tile < HEX_GRID_SIZE; tile++) {
            if (fileWriteInt32(stream, gTileIntensity[elevation][tile]) == -1) return -1;
        }
    }

    return 0;
}

// 0x000000 light_load — restore ambient + tile light state from save file
int lightLoad(File* stream)
{
    // P-03 (MEDIUM): Clamp the restored ambient intensity to the same range
    // the setter enforces (lightSetAmbientIntensity, light.cc:52). A crafted
    // save can carry any int32; an unclamped value (e.g. INT_MAX) flows into
    // intensityColorTable[intensity / 512] via objectGetLightIntensity /
    // tile render paths → ~4 MB OOB read. Legit max 65536 → index 128, safe.
    int ambientIntensity;
    if (fileReadInt32(stream, &ambientIntensity) == -1) return -1;
    gAmbientIntensity = std::clamp(ambientIntensity, LIGHT_INTENSITY_MIN, LIGHT_INTENSITY_MAX);

    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        for (int tile = 0; tile < HEX_GRID_SIZE; tile++) {
            // P-03 (MEDIUM): Clamp per-tile intensities to the same upper
            // bound used by lightGetTileIntensity (light.cc:75). Crafted-save
            // tile values above LIGHT_INTENSITY_MAX would also index the
            // intensityColorTable out of bounds via the tile render path.
            int tileIntensity;
            if (fileReadInt32(stream, &tileIntensity) == -1) return -1;
            gTileIntensity[elevation][tile] = std::clamp(tileIntensity, 0, LIGHT_INTENSITY_MAX);
        }
    }

    return 0;
}

} // namespace fallout
