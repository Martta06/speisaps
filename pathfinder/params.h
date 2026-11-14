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
/* Clasificación de celda del grafo                                    */
/* ------------------------------------------------------------------ */

typedef enum {
    CELL_SAFE    = 0,  /* traversada o bien caracterizada              */
    CELL_UNSAFE  = 1,  /* peligro confirmado → hard block              */
    CELL_UNKNOWN = 2   /* sin ground-truth → penalización conservadora */
} CellClass;

/* ------------------------------------------------------------------ */
/* Umbrales físicos (dependen del modo, no del usuario)               */
/* ------------------------------------------------------------------ */

typedef struct {
    double max_slope_deg;       /* bloqueador de pendiente (°)         */
    double max_rock_height_m;   /* altura máxima de obstáculo (m)      */
    double max_wind_ms;         /* viento máximo tolerable (m/s)       */
    double max_dust_tau;        /* opacidad de polvo máxima            */
    double ti_safe;             /* inercia térmica mínima segura       */
                                /*   (J m⁻² K⁻¹ s⁻½)                  */
    double exclusion_buffer_m;  /* radio de exclusión en escarpes (m)  */
    double hazard_decay_m;      /* escala de decaimiento proxy (m)     */
    bool   sep_alert_active;    /* alerta SEP global → bloqueo total   */
} TraversalLimits;

/* ------------------------------------------------------------------ */
/* Pesos de los factores de coste (modificables por el usuario)       */
/* ------------------------------------------------------------------ */

typedef struct {
    double w_dist;       /* longitud base del segmento               */
    double w_slope;      /* penalización de pendiente                */
    double w_rough;      /* rugosidad superficial                    */
    double w_soft;       /* trampa blanda (proxy inercia térmica)    */
    double w_rock;       /* densidad de bloques/rocas                */
    double w_elev_up;    /* coste de ascenso                         */
    double w_elev_down;  /* coste de descenso                        */
    double w_prox;       /* proximidad a escarpes/cráteres           */
    double w_unk;        /* penalización de celda desconocida        */
    double w_rad;        /* dosis de radiación acumulada (EVA)       */
    double w_wind;       /* multiplicador de condición de viento     */
} CostWeights;

/* ------------------------------------------------------------------ */
/* Parámetros de una arista del grafo (procedentes del GeoJSON/DEM)   */
/* ------------------------------------------------------------------ */

typedef struct {
    double delta_s_m;          /* longitud del segmento (m)                  */
    double delta_z_m;          /* cambio de elevación, signed (m)            */
    double slope_deg;          /* pendiente media del segmento (°)           */
    double roughness_vrm;      /* VRM o σ_z normalizado [0, 1]              */
    double rock_abundance;     /* fracción de cobertura de rocas [0, 1]     */
    double thermal_inertia;    /* TI en J m⁻² K⁻¹ s⁻½ (proxy traficat.)    */
    double hazard_distance_m;  /* distancia al peligro topográfico más     */
                               /*   cercano (escarpe, borde cráter) (m)    */
    double max_rock_height_m;  /* altura del obstáculo mayor en celda (m)  */
    CellClass cell_class;      /* clasificación de la celda                */
    double dose_rate_msv_day;  /* tasa de dosis GCR (mSv/día)             */
    double wind_multiplier;    /* factor de viento ≥ 1.0 (1.0 = nominal)  */
    double speed_ms;           /* velocidad nominal de traversal (m/s)     */
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
