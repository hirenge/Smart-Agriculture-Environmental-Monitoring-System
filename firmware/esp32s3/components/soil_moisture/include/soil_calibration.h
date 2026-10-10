#pragma once

#include <stdbool.h>
#include <math.h>

/* Relative wet/dry index, not volumetric water content. The 100-count span
 * is a project noise guard, not a universal sensor calibration standard. */
static inline bool soil_calibration_is_valid(int dry, int wet)
{
    int span = dry - wet;
    return dry >= 0 && dry <= 4095 && wet >= 0 && wet <= 4095 &&
           (span >= 100 || span <= -100);
}

static inline float soil_calibration_percent(int raw, int dry, int wet)
{
    if (!soil_calibration_is_valid(dry, wet) || raw < 0 || raw > 4095) {
        return NAN;
    }
    float percent = ((float)dry - (float)raw) * 100.0f / ((float)dry - (float)wet);
    return percent < 0.0f ? 0.0f : (percent > 100.0f ? 100.0f : percent);
}
