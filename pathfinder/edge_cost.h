/*
 * edge_cost.h — Función de coste compuesta para una arista del grafo.
 *
 * Combina todos los factores individuales de terrain_cost.h con los
 * pesos de CostWeights y los umbrales de TraversalLimits.
 *
 * Esta es la única función que el algoritmo A-star/Dijkstra necesita llamar.
 */

#ifndef EDGE_COST_H
#define EDGE_COST_H

#include "params.h"

/*
 * edge_cost — Coste total de traversar el segmento descrito por ep.
 *
 * ep:  parámetros de la arista (del GeoJSON/DEM, rellenados externamente).
 * w:   pesos de los factores (modificables por el usuario).
 * lim: umbrales físicos del modo de desplazamiento.
 *
 * Retorna:
 *   INFINITY si cualquier hard constraint está activo.
 *   Coste ≥ 0 en caso contrario.
 *
 * Fórmula:
 *   C = delta_s × [
 *         w_dist
 *       + w_slope  × slope_cost(θ, θ_max)
 *       + w_rough  × roughness_cost(vrm)
 *       + w_soft   × trafficability_cost(TI, TI_safe)
 *       + w_rock   × rock_abundance_cost(RA, RA_MAX)
 *       + w_prox   × hazard_proximity_cost(d, excl, decay)
 *       + w_unk    × unknown_cell_cost(class)
 *       + w_rad    × radiation_cost_per_meter(dose, speed)
 *       + w_wind   × wind_cost(v_wind, crosstrack, v_max)
 *   ]
 *   + elevation_cost(dz, w_up, w_down)
 *
 * Nota: elevation_cost no se multiplica por delta_s porque representa
 * trabajo vertical (J ∝ m·g·Δz), no coste por metro horizontal.
 */
double edge_cost(const EdgeParams      *ep,
                 const CostWeights     *w,
                 const TraversalLimits *lim);

/*
 * edge_cost_breakdown — Igual que edge_cost pero desglosa cada término.
 *
 * Útil para depuración, heatmaps de factor dominante y UI de explicación.
 * out_terms debe apuntar a un array de al menos COST_TERM_COUNT doubles.
 *
 * Índices de out_terms (usar las constantes COST_IDX_*):
 *   0  dist
 *   1  slope
 *   2  rough
 *   3  soft (trafficability)
 *   4  rock
 *   5  prox (hazard proximity)
 *   6  unk  (unknown cell)
 *   7  rad  (radiation)
 *   8  wind
 *   9  elev (elevation, no multiplicado por delta_s)
 */
#define COST_TERM_COUNT 10

#define COST_IDX_DIST  0
#define COST_IDX_SLOPE 1
#define COST_IDX_ROUGH 2
#define COST_IDX_SOFT  3
#define COST_IDX_ROCK  4
#define COST_IDX_PROX  5
#define COST_IDX_UNK   6
#define COST_IDX_RAD   7
#define COST_IDX_WIND  8
#define COST_IDX_ELEV  9

double edge_cost_breakdown(const EdgeParams      *ep,
                           const CostWeights     *w,
                           const TraversalLimits *lim,
                           double                 out_terms[COST_TERM_COUNT]);

#endif /* EDGE_COST_H */
