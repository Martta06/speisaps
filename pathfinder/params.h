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
    double min_temp_c;          /* temperatura mínima tolerable (°C)   */
    double max_temp_c;          /* temperatura máxima tolerable (°C)   */
} TraversalLimits;

/* ------------------------------------------------------------------ */
/* Estado ambiental dinámico: Tormenta Solar (SPE)                    */
/* ------------------------------------------------------------------ */

/*
 * Tasa de dosis de referencia para normalizar el coste de radiación:
 * GCR en superficie de Gale medida por MSL/RAD, 0.64 ± 0.12 mSv/día
 * (Hassler et al., 2014, Science 343). Con esta referencia, un tramo en
 * llanura abierta en calma tiene f_rad ≈ 1.
 */
#define RAD_REF_DOSE_RATE_MSV_DAY 0.64

/* Factores de transmisión del vehículo (1.0 = sin blindaje adicional) */
#define SHIELDING_FOOT_EVA 1.00
#define SHIELDING_ROVER    0.85

/*
 * DynamicEnvironmentState — Estado del entorno que cambia en tiempo real
 * y que A* consulta en cada arista.
 *
 * La hora marciana NO va aquí: A* ya recibe la hora de partida y calcula
 * la hora de llegada a cada tramo (start_sol_hour + t_acumulado).
 *
 * La ventana [spe_onset_s, spe_end_s) se expresa en segundos desde la
 * partida, igual que elapsed_time_s, para poder planificar con una SPE
 * prevista (p. ej. alerta: "las partículas llegan en 40 min").
 */
typedef struct {
    bool   spe_active;            /* true = hay SPE en curso o prevista      */
    double spe_storm_rate_uSv_h;  /* tasa EXTRA a cielo abierto (uSv/h)      */
    double spe_onset_s;           /* inicio del SPE (s desde partida), 0=ya  */
    double spe_end_s;             /* fin del SPE (s desde partida), INFINITY */
    double vehicle_shielding;     /* transmisión: 1.0 EVA, 0.85 rover        */

    /* --- Campos para la Tormenta de Arena Dinámica --- */
    int    dust_storm_active;      /* 0 = No activa, 1 = Activa                         */
    double storm_center_lon;       /* Longitud inicial del centro de la tormenta (°E)   */
    double storm_center_lat;       /* Latitud inicial del centro de la tormenta (°N)    */
    double storm_vel_east_ms;      /* Velocidad de deriva en dirección Este (m/s)       */
    double storm_vel_north_ms;     /* Velocidad de deriva en dirección Norte (m/s)      */
    double storm_radius_m;         /* Radio de la tormenta de arena en metros           */
    float  storm_severity_penalty; /* Multiplicador de penalización de coste / bloqueo  */
} DynamicEnvironmentState;

/* Estado en calma (sin SPE) con el blindaje correspondiente al modo */
DynamicEnvironmentState env_state_calm(TraversalMode mode);

/*
 * Tasa SPE extra (uSv/h, cielo abierto, sin blindaje) vigente en el instante
 * elapsed_s. Devuelve 0.0 si no hay SPE o si elapsed_s cae fuera de la ventana.
 */
double env_spe_rate_at(const DynamicEnvironmentState *env, double elapsed_s);

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
    double w_temp;       /* estrés térmico / disconfort temperatura  */
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
    double dose_rate_msv_day;  /* tasa de dosis total GCR+SPE, ya blindada (mSv/día) */
    double temp_c;             /* temperatura atmosférica local (°C)         */
    double speed_ms;           /* velocidad nominal de traversal (m/s)       */
    double dust_storm_penalty; /* penalización de tormenta de arena (0, >0 o INFINITY) */
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
