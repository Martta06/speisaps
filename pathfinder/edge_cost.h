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
 * Fórmula limpia y puramente física:
 *   C = delta_s × [
 *         w_dist
 *       + w_slope  × slope_cost(θ, θ_max)
 *       + w_rough  × roughness_cost(roughness)
 *       + w_rad    × radiation_cost_per_meter(dose, speed)
 *   ]
 *   + elevation_cost(dz, w_up, w_down)
 */
double edge_cost(const EdgeParams      *ep,
                 const CostWeights     *w,
                 const TraversalLimits *lim);

#define COST_TERM_COUNT 5

#define COST_IDX_DIST  0
#define COST_IDX_SLOPE 1
#define COST_IDX_ROUGH 2
#define COST_IDX_RAD   3
#define COST_IDX_ELEV  4

double edge_cost_breakdown(const EdgeParams      *ep,
                           const CostWeights     *w,
                           const TraversalLimits *lim,
                           double                 out_terms[COST_TERM_COUNT]);

#endif /* EDGE_COST_H */
