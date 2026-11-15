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
