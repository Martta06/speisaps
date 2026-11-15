/*
 * params.h — Structs de parámetros para la función de coste marciana.
 *
 * No contiene lógica: solo tipos de datos compartidos entre módulos.
 * Los pesos son modificables en tiempo de ejecución; los límites dependen
 * del modo de desplazamiento (rover vs. EVA a pie).
 */

#ifndef PARAMS_H
#define PARAMS_H

#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* Modo de desplazamiento                                              */
/* ------------------------------------------------------------------ */

typedef enum {
    MODE_FOOT_EVA = 0,
    MODE_ROVER    = 1
} TraversalMode;

/* ------------------------------------------------------------------ */
/* Umbrales físicos (dependen del modo, no del usuario)               */
/* ------------------------------------------------------------------ */

typedef struct {
    double max_slope_deg;       /* bloqueador de pendiente (°)         */
    double max_roughness;       /* rugosidad máxima tolerable [0, 1]   */
    bool   sep_alert_active;    /* alerta SEP global → bloqueo total   */
} TraversalLimits;

/* ------------------------------------------------------------------ */
/* Pesos de los factores de coste (modificables por el usuario)       */
/* ------------------------------------------------------------------ */

typedef struct {
    double w_dist;       /* longitud base del segmento               */
    double w_slope;      /* penalización de pendiente                */
    double w_rough;      /* rugosidad superficial (calculada del DEM)*/
    double w_elev_up;    /* coste de ascenso                         */
    double w_elev_down;  /* coste de descenso                        */
    double w_rad;        /* dosis de radiación acumulada (EVA)       */
} CostWeights;

/* ------------------------------------------------------------------ */
/* Parámetros de una arista del grafo (derivados del terreno y tiempo)*/
/* ------------------------------------------------------------------ */

typedef struct {
    double delta_s_m;          /* longitud del segmento (m)                  */
    double delta_z_m;          /* cambio de elevación, signed (m)            */
    double slope_deg;          /* pendiente media del segmento (°)           */
    double roughness;          /* micro-rugosidad calculada del DEM [0, 1]  */
    double elapsed_time_s;     /* segundos transcurridos desde inicio        */
    double dose_rate_msv_day;  /* tasa de dosis GCR instantánea (mSv/día)    */
    double speed_ms;           /* velocidad nominal de traversal (m/s)       */
} EdgeParams;

/* ------------------------------------------------------------------ */
/* Perfiles predeterminados                                            */
/* ------------------------------------------------------------------ */

/* Devuelve límites conservadores para EVA a pie */
TraversalLimits limits_default_foot_eva(void);

/* Devuelve límites para rover tipo Curiosity */
TraversalLimits limits_default_rover(void);

/* Pesos neutros: todos los factores con peso 1.0 */
CostWeights weights_uniform(void);

/* Pesos sugeridos para EVA a pie */
CostWeights weights_default_foot_eva(void);

/* Pesos sugeridos para rover */
CostWeights weights_default_rover(void);

#endif /* PARAMS_H */
