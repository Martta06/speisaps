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

double trafficability_cost(double thermal_inertia, double ti_safe)
{
    if (thermal_inertia <= 0.0) return 1.0;
    if (thermal_inertia >= ti_safe) return 0.0;
    return 1.0 - (thermal_inertia / ti_safe);
}

/* ------------------------------------------------------------------ */

double rock_abundance_cost(double ra, double ra_max)
{
    if (ra_max <= 0.0) return 0.0;
    return clamp01(ra / ra_max);
}

/* ------------------------------------------------------------------ */

double hazard_proximity_cost(double dist_m, double exclusion_m, double decay_m)
{
    if (dist_m <= exclusion_m) return INFINITY;
    double excess = dist_m - exclusion_m;
    return exp(-excess / decay_m);
}

/* ------------------------------------------------------------------ */

double elevation_cost(double dz_m, double w_up, double w_down)
{
    if (dz_m > 0.0) return w_up   *  dz_m;
    if (dz_m < 0.0) return w_down * (-dz_m);
    return 0.0;
}

/* ------------------------------------------------------------------ */

double unknown_cell_cost(int cls)
{
    switch ((CellClass)cls) {
        case CELL_SAFE:    return 0.0;
        case CELL_UNSAFE:  return INFINITY;
        case CELL_UNKNOWN: return 1.0;
        default:           return 1.0;
    }
}

/* ------------------------------------------------------------------ */

double radiation_cost_per_meter(double dose_rate_msv_day, double speed_ms)
{
    /*
     * dose_rate (mSv/día) → mSv/s: dividir por 86400.
     * tiempo por metro (s/m): 1 / speed_ms.
     * dosis por metro (mSv/m): dose_rate_msv_s / speed_ms.
     */
    if (speed_ms <= 0.0) return 0.0;
    double dose_rate_msv_s = dose_rate_msv_day / 86400.0;
    return dose_rate_msv_s / speed_ms;
}

/* ------------------------------------------------------------------ */

double wind_cost(double wind_ms, double crosstrack, double max_wind_ms)
{
    if (wind_ms >= max_wind_ms) return INFINITY;
    /*
     * Factor base: viento / límite ∈ [0, 1).
     * Modulado por componente lateral: viento de cara/espalda (crosstrack=0)
     * penaliza la mitad que el viento perpendicular (crosstrack=1).
     */
    double base = wind_ms / max_wind_ms;
    double lateral_weight = 0.5 + 0.5 * crosstrack;  /* ∈ [0.5, 1.0] */
    return base * lateral_weight;
}
