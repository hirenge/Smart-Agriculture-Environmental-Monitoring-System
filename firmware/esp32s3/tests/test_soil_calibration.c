#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "soil_calibration.h"

int main(void)
{
    assert(soil_calibration_is_valid(3000, 1200));
    assert(soil_calibration_is_valid(1200, 3000));
    assert(!soil_calibration_is_valid(3000, 3000));
    assert(!soil_calibration_is_valid(2000, 2050));
    assert(!soil_calibration_is_valid(-1, 1000));
    assert(!soil_calibration_is_valid(4096, 1000));
    assert(soil_calibration_percent(3000, 3000, 1200) == 0.0f);
    assert(soil_calibration_percent(1200, 3000, 1200) == 100.0f);
    assert(fabsf(soil_calibration_percent(2100, 3000, 1200) - 50.0f) < 0.001f);
    assert(soil_calibration_percent(4095, 3000, 1200) == 0.0f);
    assert(soil_calibration_percent(0, 3000, 1200) == 100.0f);
    assert(soil_calibration_percent(1200, 1200, 3000) == 0.0f);
    assert(soil_calibration_percent(3000, 1200, 3000) == 100.0f);
    assert(isnan(soil_calibration_percent(2000, 2000, 2000)));
    assert(isnan(soil_calibration_percent(-1, 3000, 1200)));
    puts("Soil calibration: 15 checks PASS (test endpoints are examples, not measured data)");
    return 0;
}
