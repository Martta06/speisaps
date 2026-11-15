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

/*
 * radiation_cost_normalized — Factor de coste de radiación adimensional.
 *
 * Dosis por metro de este tramo dividida por la dosis por metro de referencia
 * (GCR a cielo abierto, RAD_REF_DOSE_RATE_MSV_DAY, a 1 m/s):
 *
 *   f_rad = (dose_rate / RAD_REF) × (1 m/s / speed)
 *
 *   ≈ 1.0   llanura abierta en calma (EVA)
 *   ≈ 0.5   fondo de cañón en calma
 *   ≈ 6.5   llanura abierta con SPE de +150 uSv/h
 *
 * A diferencia de radiation_cost_per_meter (mSv/m, ~1e-5), este valor está en
 * la misma escala que los demás factores, así que w_rad tiene efecto real.
 * No está acotado a [0, 1] a propósito: una SPE debe poder dominar el coste.
 */
double radiation_cost_normalized(double dose_rate_msv_day, double speed_ms);

/*
 * temperature_stress_cost — Factor de estrés térmico normalizado [0, 1].
 *
 * temp_c:     Temperatura ambiente (°C) calculada con el modelo Parton-Logan.
 * min_temp_c: Límite inferior seguro (ej. -80°C).
 * max_temp_c: Límite superior seguro (ej. +20°C).
 *
 * En la zona óptima (-20°C a 0°C) el estrés es 0. A medida que se acerca
 * a los extremos se incrementa cuadráticamente. Retorna INFINITY si excede límites.
 */
double temperature_stress_cost(double temp_c, double min_temp_c, double max_temp_c);

#endif /* TERRAIN_COST_H */
