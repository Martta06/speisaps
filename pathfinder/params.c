/*
 * params.c — Implementación de los perfiles predeterminados.
 */

#include "params.h"

#include <math.h>

TraversalLimits limits_default_foot_eva(void) {
    return (TraversalLimits){
        .max_slope_deg      = 20.0,
        .max_roughness      = 0.80,
        .min_temp_c         = -80.0, /* límite del sistema térmico del traje */
        .max_temp_c         = 20.0
    };
}

TraversalLimits limits_default_rover(void) {
    return (TraversalLimits){
        .max_slope_deg      = 27.0,
        .max_roughness      = 0.90,
        .min_temp_c         = -100.0, /* calefactores RTG / actuadores */
        .max_temp_c         = 35.0
    };
}

CostWeights weights_uniform(void) {
    return (CostWeights){
        .w_dist      = 1.0,
        .w_slope     = 1.0,
        .w_rough     = 1.0,
        .w_elev_up   = 1.0,
        .w_elev_down = 1.0,
        .w_rad       = 1.0,
        .w_temp      = 1.0
    };
}

CostWeights weights_default_foot_eva(void) {
    return (CostWeights){
        .w_dist      = 1.0,
        .w_slope     = 3.0,   /* pendiente muy penalizada: traje rígido      */
        .w_rough     = 2.5,   /* rugosidad: riesgo de caída                  */
        .w_elev_up   = 4.0,   /* ascenso: metabolismo alto en traje          */
        .w_elev_down = 1.5,
        .w_rad       = 1.0,   /* dosis biológica (f_rad normalizado, ≈1 en calma) */
        .w_temp      = 1.5    /* penalizar frío extremo (consumo refrigeración/calor) */
    };
}

CostWeights weights_default_rover(void) {
    return (CostWeights){
        .w_dist      = 1.0,
        .w_slope     = 2.0,
        .w_rough     = 1.5,
        .w_elev_up   = 2.0,   /* energía + slip                              */
        .w_elev_down = 1.0,
        .w_rad       = 0.5,   /* tripulación dentro del rover: la dosis cuenta,
                                 pero el blindaje ya la reduce (×0.85)        */
        .w_temp      = 0.5    /* rover: calentadores automáticos             */
    };
}

/* ------------------------------------------------------------------ */

DynamicEnvironmentState env_state_calm(TraversalMode mode) {
    return (DynamicEnvironmentState){
        .spe_active             = false,
        .spe_storm_rate_uSv_h   = 0.0,
        .spe_onset_s            = 0.0,
        .spe_end_s              = INFINITY,
        .vehicle_shielding      = (mode == MODE_ROVER) ? SHIELDING_ROVER : SHIELDING_FOOT_EVA,
        .dust_storm_active      = 0,
        .storm_center_lon       = 0.0,
        .storm_center_lat       = 0.0,
        .storm_vel_east_ms      = 0.0,
        .storm_vel_north_ms     = 0.0,
        .storm_radius_m         = 0.0,
        .storm_severity_penalty = 25.0f
    };
}

double env_spe_rate_at(const DynamicEnvironmentState *env, double elapsed_s) {
    if (!env || !env->spe_active || env->spe_storm_rate_uSv_h <= 0.0) return 0.0;
    if (elapsed_s < env->spe_onset_s) return 0.0;
    if (elapsed_s >= env->spe_end_s)  return 0.0;
    return env->spe_storm_rate_uSv_h;
}
