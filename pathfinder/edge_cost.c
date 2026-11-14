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
    if (lim->sep_alert_active)              return INFINITY;
    if (ep->cell_class == CELL_UNSAFE)      return INFINITY;
    if (ep->max_rock_height_m > lim->max_rock_height_m) return INFINITY;

    /* ---- Factores individuales (pueden ser INFINITY) -------------- */

    double f_slope = slope_cost(ep->slope_deg, lim->max_slope_deg);
    if (!isfinite(f_slope)) return INFINITY;

    double f_prox = hazard_proximity_cost(ep->hazard_distance_m,
                                          lim->exclusion_buffer_m,
                                          lim->hazard_decay_m);
    if (!isfinite(f_prox)) return INFINITY;

    double f_unk = unknown_cell_cost((int)ep->cell_class);
    if (!isfinite(f_unk)) return INFINITY;

    /* Viento: necesita el factor lateral (calculado externamente y almacenado
     * en wind_multiplier como componente lateral ya combinada, o se pasa
     * directamente desde mars_wind_crosstrack_factor).
     * Aquí usamos wind_multiplier como velocidad de viento efectiva (m/s). */
    double f_wind = wind_cost(ep->wind_multiplier * lim->max_wind_ms,
                              0.5,   /* crosstrack neutro si no se calcula */
                              lim->max_wind_ms);
    if (!isfinite(f_wind)) return INFINITY;

    /* ---- Factores suaves ----------------------------------------- */
    double f_rough = roughness_cost(ep->roughness_vrm);
    double f_soft  = trafficability_cost(ep->thermal_inertia, lim->ti_safe);
    double f_rock  = rock_abundance_cost(ep->rock_abundance, RA_MAX_DEFAULT);
    double f_rad   = radiation_cost_per_meter(ep->dose_rate_msv_day, ep->speed_ms);

    /* ---- Coste horizontal (multiplicado por delta_s) -------------- */
    double horiz = ep->delta_s_m * (
          w->w_dist
        + w->w_slope * f_slope
        + w->w_rough * f_rough
        + w->w_soft  * f_soft
        + w->w_rock  * f_rock
        + w->w_prox  * f_prox
        + w->w_unk   * f_unk
        + w->w_rad   * f_rad
        + w->w_wind  * f_wind
    );

    /* ---- Coste vertical (trabajo gravitacional, no por metro) ----- */
    double vert = elevation_cost(ep->delta_z_m, w->w_elev_up, w->w_elev_down);

    return horiz + vert;
}

/* ------------------------------------------------------------------ */

double edge_cost_breakdown(const EdgeParams      *ep,
                           const CostWeights     *w,
                           const TraversalLimits *lim,
                           double                 out_terms[COST_TERM_COUNT])
{
    /* Inicializar a 0 */
    for (int i = 0; i < COST_TERM_COUNT; i++) out_terms[i] = 0.0;

    /* Hard blockers */
    if (lim->sep_alert_active)              return INFINITY;
    if (ep->cell_class == CELL_UNSAFE)      return INFINITY;
    if (ep->max_rock_height_m > lim->max_rock_height_m) return INFINITY;

    double f_slope = slope_cost(ep->slope_deg, lim->max_slope_deg);
    if (!isfinite(f_slope)) return INFINITY;

    double f_prox = hazard_proximity_cost(ep->hazard_distance_m,
                                          lim->exclusion_buffer_m,
                                          lim->hazard_decay_m);
    if (!isfinite(f_prox)) return INFINITY;

    double f_unk = unknown_cell_cost((int)ep->cell_class);
    if (!isfinite(f_unk)) return INFINITY;

    double f_wind = wind_cost(ep->wind_multiplier * lim->max_wind_ms,
                              0.5, lim->max_wind_ms);
    if (!isfinite(f_wind)) return INFINITY;

    double f_rough = roughness_cost(ep->roughness_vrm);
    double f_soft  = trafficability_cost(ep->thermal_inertia, lim->ti_safe);
    double f_rock  = rock_abundance_cost(ep->rock_abundance, RA_MAX_DEFAULT);
    double f_rad   = radiation_cost_per_meter(ep->dose_rate_msv_day, ep->speed_ms);

    double s = ep->delta_s_m;

    out_terms[COST_IDX_DIST]  = w->w_dist  * s;
    out_terms[COST_IDX_SLOPE] = w->w_slope * f_slope * s;
    out_terms[COST_IDX_ROUGH] = w->w_rough * f_rough * s;
    out_terms[COST_IDX_SOFT]  = w->w_soft  * f_soft  * s;
    out_terms[COST_IDX_ROCK]  = w->w_rock  * f_rock  * s;
    out_terms[COST_IDX_PROX]  = w->w_prox  * f_prox  * s;
    out_terms[COST_IDX_UNK]   = w->w_unk   * f_unk   * s;
    out_terms[COST_IDX_RAD]   = w->w_rad   * f_rad   * s;
    out_terms[COST_IDX_WIND]  = w->w_wind  * f_wind  * s;
    out_terms[COST_IDX_ELEV]  = elevation_cost(ep->delta_z_m, w->w_elev_up, w->w_elev_down);

    double total = 0.0;
    for (int i = 0; i < COST_TERM_COUNT; i++) total += out_terms[i];
    return total;
}
