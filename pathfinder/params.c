/*
 * params.c — Implementación de los perfiles predeterminados.
 */

#include "params.h"

TraversalLimits limits_default_foot_eva(void) {
    return (TraversalLimits){
        .max_slope_deg      = 20.0,
        .max_roughness      = 0.80,
        .min_temp_c         = -80.0, /* límite del sistema térmico del traje */
        .max_temp_c         = 20.0,
        .sep_alert_active   = false
    };
}

TraversalLimits limits_default_rover(void) {
    return (TraversalLimits){
        .max_slope_deg      = 27.0,
        .max_roughness      = 0.90,
        .min_temp_c         = -100.0, /* calefactores RTG / actuadores */
        .max_temp_c         = 35.0,
        .sep_alert_active   = false
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
        .w_rad       = 1.0,   /* acumulación de dosis biológica activa       */
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
        .w_rad       = 0.0,   /* rover: sin límite biológico                 */
        .w_temp      = 0.5    /* rover: calentadores automáticos             */
    };
}
