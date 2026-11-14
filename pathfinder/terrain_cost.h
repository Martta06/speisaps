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
 * vrm: Vector Ruggedness Measure o σ_z normalizado, en [0, 1].
 *
 * Retorna el valor directamente clampado a [0, 1].
 * Para EVA, el peso externo (w_rough) debe ser mayor que para rover.
 */
double roughness_cost(double vrm);

/*
 * trafficability_cost — Factor de coste por riesgo de trampa blanda.
 *
 * thermal_inertia: TI de la celda (J m⁻² K⁻¹ s⁻½).
 * ti_safe:         umbral mínimo para considerar el suelo consolidado
 *                  (del TraversalLimits: 100–150 J m⁻² K⁻¹ s⁻½).
 *
 * Retorna 0 si TI >= ti_safe (suelo firme),
 *         1 si TI <= 0 (peor caso, no debería ocurrir).
 * Función inversa lineal: a menor TI, mayor riesgo.
 */
double trafficability_cost(double thermal_inertia, double ti_safe);

/*
 * rock_abundance_cost — Factor de coste por densidad de bloques.
 *
 * ra: fracción de cobertura de rocas [0, 1] (p. ej. 0.10 = 10 %).
 * ra_max: valor de saturación (típico 0.20 = 20 %).
 *
 * Retorna ra / ra_max clampado a [0, 1].
 * Hard block externo si max_rock_height_m supera el límite del vehículo.
 */
double rock_abundance_cost(double ra, double ra_max);

/*
 * hazard_proximity_cost — Factor de coste por proximidad a peligros.
 *
 * dist_m:        distancia al peligro topográfico más cercano (m).
 * exclusion_m:   radio de exclusión duro (m); retorna INFINITY si dist ≤ exclusion_m.
 * decay_m:       escala de decaimiento de la penalización suave (m).
 *
 * Retorna:
 *   INFINITY si dist_m <= exclusion_m.
 *   exp(-(dist_m - exclusion_m) / decay_m) en [0, 1) si dist > exclusion_m.
 *
 * A distancia = exclusion_m + decay_m el factor vale ~0.37 (1/e).
 */
double hazard_proximity_cost(double dist_m, double exclusion_m, double decay_m);

/*
 * elevation_cost — Coste de cambio de elevación (en metros, no normalizado).
 *
 * dz_m:    cambio de elevación signed (positivo = subida).
 * w_up:    peso del ascenso (desde CostWeights).
 * w_down:  peso del descenso (desde CostWeights).
 *
 * Retorna w_up × dz si subida, w_down × |dz| si bajada.
 * Este término se suma al coste total sin multiplicar por delta_s_m,
 * ya que representa trabajo vertical, no coste por metro horizontal.
 */
double elevation_cost(double dz_m, double w_up, double w_down);

/*
 * unknown_cell_cost — Factor de penalización por celda sin datos.
 *
 * cls: clasificación de la celda (CellClass).
 *
 * Retorna:
 *   0.0      para CELL_SAFE.
 *   INFINITY para CELL_UNSAFE (hard block).
 *   1.0      para CELL_UNKNOWN (el peso w_unk escala la penalización).
 */
double unknown_cell_cost(int cls);  /* int para evitar dependencia circular */

/*
 * radiation_cost_per_meter — Dosis acumulada por metro de trayecto.
 *
 * dose_rate_msv_day: tasa de dosis GCR (mSv/día), p. ej. 0.64.
 * speed_ms:          velocidad nominal de traversal (m/s), > 0.
 *
 * Retorna mSv por metro recorrido.
 * Multiplicar por delta_s_m y w_rad para obtener el término de dosis.
 *
 * Solo relevante para EVA (w_rad = 0 para rover).
 */
double radiation_cost_per_meter(double dose_rate_msv_day, double speed_ms);

/*
 * wind_cost — Factor de coste por condición de viento.
 *
 * wind_ms:        velocidad del viento en el segmento (m/s).
 * crosstrack:     componente lateral normalizada [0, 1] (mars_wind_crosstrack_factor).
 * max_wind_ms:    límite de bloqueo duro (del TraversalLimits).
 *
 * Retorna:
 *   INFINITY si wind_ms >= max_wind_ms.
 *   (wind_ms / max_wind_ms) × (1 + crosstrack) / 2 en [0, 1) si no bloqueado.
 *
 * La componente lateral penaliza más el viento perpendicular a la marcha.
 */
double wind_cost(double wind_ms, double crosstrack, double max_wind_ms);

#endif /* TERRAIN_COST_H */
