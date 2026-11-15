/*
 * terrain_cost.c — Implementación de los factores de coste individuales.
 */

#include "terrain_cost.h"
#include "params.h"   /* CellClass */

#include <math.h>

/* ---- utilidades internas ----------------------------------------- */

static inline double clamp01(double x)
{
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

/* ------------------------------------------------------------------ */

double slope_cost(double theta_deg, double theta_max_deg)
{
    if (theta_deg >= theta_max_deg) return INFINITY;
    double t = theta_deg / theta_max_deg;
    return t * t;
}

/* ------------------------------------------------------------------ */

double roughness_cost(double vrm)
{
    return clamp01(vrm);
}

/* ------------------------------------------------------------------ */

double elevation_cost(double dz_m, double w_up, double w_down)
{
    if (dz_m > 0.0) return w_up   *  dz_m;
    if (dz_m < 0.0) return w_down * (-dz_m);
    return 0.0;
}

/* ------------------------------------------------------------------ */

double radiation_cost_per_meter(double dose_rate_msv_day, double speed_ms)
{
    if (speed_ms <= 0.0) return 0.0;
    double dose_rate_msv_s = dose_rate_msv_day / 86400.0;
    return dose_rate_msv_s / speed_ms;
}

double radiation_cost_normalized(double dose_rate_msv_day, double speed_ms)
{
    if (speed_ms <= 0.0 || dose_rate_msv_day <= 0.0) return 0.0;
    return (dose_rate_msv_day / RAD_REF_DOSE_RATE_MSV_DAY) * (1.0 / speed_ms);
}

/* ------------------------------------------------------------------ */

double temperature_stress_cost(double temp_c, double min_temp_c, double max_temp_c)
{
    if (temp_c < min_temp_c || temp_c > max_temp_c) return INFINITY;

    /* Rango confortable o neutro: -20°C a 0°C */
    const double comfort_low = -20.0;
    const double comfort_high = 0.0;

    if (temp_c >= comfort_low && temp_c <= comfort_high) {
        return 0.0;
    }

    if (temp_c < comfort_low) {
        double delta = comfort_low - temp_c;
        double max_delta = comfort_low - min_temp_c;
        if (max_delta <= 0.0) return 0.0;
        double ratio = delta / max_delta;
        return ratio * ratio; /* cuadrático [0, 1] */
    } else {
        double delta = temp_c - comfort_high;
        double max_delta = max_temp_c - comfort_high;
        if (max_delta <= 0.0) return 0.0;
        double ratio = delta / max_delta;
        return ratio * ratio;
    }
}
