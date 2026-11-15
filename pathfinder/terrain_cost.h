/*
 * terrain_cost.h — Funciones de coste individuales por factor.
 *
 * Cada función devuelve un valor en [0, 1] (factor normalizado)
 * o INFINITY cuando el segmento está bloqueado (hard constraint).
 *
 * El coste compuesto final se calcula en edge_cost.h, que multiplica
 * cada factor por su peso y por la longitud del segmento.
 *
 * Todas las funciones son puras: sin estado global, sin I/O.
 */

#ifndef TERRAIN_COST_H
#define TERRAIN_COST_H

#include <math.h>   /* INFINITY */

/*
 * slope_cost — Factor de coste por pendiente.
 *
 * theta_deg:     pendiente del segmento (°), siempre positiva.
 * theta_max_deg: límite de bloqueo duro (del TraversalLimits).
 *
 * Retorna:
 *   INFINITY si theta_deg >= theta_max_deg (hard block).
 *   (theta_deg / theta_max_deg)² en [0, 1) en caso contrario.
 *
 * Cuadrático: duplicar la pendiente cuadruplica el coste,
 * reflejando el aumento no lineal de esfuerzo metabólico/energético.
 */
double slope_cost(double theta_deg, double theta_max_deg);

/*
 * roughness_cost — Factor de coste por rugosidad superficial.
 *
 * vrm: Medida de micro-rugosidad calculada en [0, 1].
 *
 * Retorna el valor directamente clampado a [0, 1].
 */
double roughness_cost(double vrm);

/*
 * elevation_cost — Coste de cambio de elevación (en metros, no normalizado).
 *
 * dz_m:    cambio de elevación signed (positivo = subida).
 * w_up:    peso del ascenso (desde CostWeights).
 * w_down:  peso del descenso (desde CostWeights).
 *
 * Retorna w_up × dz si subida, w_down × |dz| si bajada.
 */
double elevation_cost(double dz_m, double w_up, double w_down);

/*
 * radiation_cost_per_meter — Dosis acumulada por metro de trayecto.
 *
 * dose_rate_msv_day: tasa de dosis GCR (mSv/día), p. ej. 0.64.
 * speed_ms:          velocidad nominal de traversal (m/s), > 0.
 *
 * Retorna mSv por metro recorrido.
 */
double radiation_cost_per_meter(double dose_rate_msv_day, double speed_ms);

#endif /* TERRAIN_COST_H */
