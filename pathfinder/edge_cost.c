/*
 * edge_cost.c — Implementación de la función de coste compuesta.
 */

#include "edge_cost.h"
#include "terrain_cost.h"

#include <math.h>

/* Saturación de RA usada internamente (20 %) */
#define RA_MAX_DEFAULT 0.20

/* ------------------------------------------------------------------ */

double edge_cost(const EdgeParams      *ep,
                 const CostWeights     *w,
                 const TraversalLimits *lim)
{
    /* ---- Hard blockers globales ----------------------------------- */
    if (lim->sep_alert_active) return INFINITY;

    /* ---- Pendiente y Rugosidad ------------------------------------ */
    double f_slope = slope_cost(ep->slope_deg, lim->max_slope_deg);
    if (!isfinite(f_slope)) return INFINITY;

    if (ep->roughness > lim->max_roughness) return INFINITY;
    double f_rough = roughness_cost(ep->roughness);

    /* ---- Radiación y Temperatura --------------------------------- */
    double f_rad = radiation_cost_per_meter(ep->dose_rate_msv_day, ep->speed_ms);

    double f_temp = temperature_stress_cost(ep->temp_c, lim->min_temp_c, lim->max_temp_c);
    if (!isfinite(f_temp)) return INFINITY;

    /* ---- Coste horizontal (multiplicado por delta_s) -------------- */
    double horiz = ep->delta_s_m * (
          w->w_dist
        + w->w_slope * f_slope
        + w->w_rough * f_rough
        + w->w_rad   * f_rad
        + w->w_temp  * f_temp
    );

    /* ---- Coste vertical (trabajo gravitacional) ------------------- */
    double vert = elevation_cost(ep->delta_z_m, w->w_elev_up, w->w_elev_down);

    return horiz + vert;
}

/* ------------------------------------------------------------------ */

double edge_cost_breakdown(const EdgeParams      *ep,
                           const CostWeights     *w,
                           const TraversalLimits *lim,
                           double                 out_terms[COST_TERM_COUNT])
{
    for (int i = 0; i < COST_TERM_COUNT; i++) out_terms[i] = 0.0;

    if (lim->sep_alert_active) return INFINITY;

    double f_slope = slope_cost(ep->slope_deg, lim->max_slope_deg);
    if (!isfinite(f_slope)) return INFINITY;

    if (ep->roughness > lim->max_roughness) return INFINITY;
    double f_rough = roughness_cost(ep->roughness);

    double f_rad = radiation_cost_per_meter(ep->dose_rate_msv_day, ep->speed_ms);
    double f_temp = temperature_stress_cost(ep->temp_c, lim->min_temp_c, lim->max_temp_c);
    if (!isfinite(f_temp)) return INFINITY;

    double s = ep->delta_s_m;

    out_terms[COST_IDX_DIST]  = w->w_dist  * s;
    out_terms[COST_IDX_SLOPE] = w->w_slope * f_slope * s;
    out_terms[COST_IDX_ROUGH] = w->w_rough * f_rough * s;
    out_terms[COST_IDX_RAD]   = w->w_rad   * f_rad   * s;
    out_terms[COST_IDX_TEMP]  = w->w_temp  * f_temp  * s;
    out_terms[COST_IDX_ELEV]  = elevation_cost(ep->delta_z_m, w->w_elev_up, w->w_elev_down);

    double total = 0.0;
    for (int i = 0; i < COST_TERM_COUNT; i++) total += out_terms[i];
    return total;
}
