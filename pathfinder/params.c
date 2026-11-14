/*
 * params.c — Implementación de los perfiles predeterminados.
 */

#include "params.h"

TraversalLimits limits_default_foot_eva(void) {
    return (TraversalLimits){
        .max_slope_deg      = 20.0,
        .max_rock_height_m  = 0.30,
        .max_wind_ms        = 20.0,
        .max_dust_tau       = 5.0,
        .ti_safe            = 150.0,
        .exclusion_buffer_m = 15.0,
        .hazard_decay_m     = 30.0,
        .sep_alert_active   = false
    };
}

TraversalLimits limits_default_rover(void) {
    return (TraversalLimits){
        .max_slope_deg      = 27.0,
        .max_rock_height_m  = 0.50,
        .max_wind_ms        = 30.0,
        .max_dust_tau       = 1e9,   /* rover RTG: sin límite por polvo      */
        .ti_safe            = 100.0,
        .exclusion_buffer_m = 10.0,
        .hazard_decay_m     = 20.0,
        .sep_alert_active   = false
    };
}

CostWeights weights_uniform(void) {
    return (CostWeights){
        .w_dist      = 1.0,
        .w_slope     = 1.0,
        .w_rough     = 1.0,
        .w_soft      = 1.0,
        .w_rock      = 1.0,
        .w_elev_up   = 1.0,
        .w_elev_down = 1.0,
        .w_prox      = 1.0,
        .w_unk       = 1.0,
        .w_rad       = 1.0,
        .w_wind      = 1.0
    };
}

CostWeights weights_default_foot_eva(void) {
    return (CostWeights){
        .w_dist      = 1.0,
        .w_slope     = 3.0,   /* pendiente muy penalizada: traje rígido      */
        .w_rough     = 2.5,   /* rugosidad: riesgo de caída                  */
        .w_soft      = 2.0,   /* trampa blanda: difícil recuperarse en traje */
        .w_rock      = 1.5,
        .w_elev_up   = 4.0,   /* ascenso: metabolismo alto en traje          */
        .w_elev_down = 1.5,
        .w_prox      = 2.0,
        .w_unk       = 2.0,   /* conservadores: celda desconocida peligrosa  */
        .w_rad       = 1.0,   /* activo: acumulación de dosis biológica       */
        .w_wind      = 1.5
    };
}

CostWeights weights_default_rover(void) {
    return (CostWeights){
        .w_dist      = 1.0,
        .w_slope     = 2.0,
        .w_rough     = 1.5,
        .w_soft      = 2.5,   /* rover: atasco catastrófico (Spirit)         */
        .w_rock      = 2.0,   /* desgaste de ruedas                          */
        .w_elev_up   = 2.0,   /* energía + slip                              */
        .w_elev_down = 1.0,
        .w_prox      = 1.5,
        .w_unk       = 1.5,
        .w_rad       = 0.0,   /* rover: sin límite biológico                 */
        .w_wind      = 0.5
    };
}
