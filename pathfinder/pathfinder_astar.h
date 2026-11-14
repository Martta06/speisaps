/*
 * pathfinder_astar.h — Motor de búsqueda A* sobre la malla ráster DEM.
 */

#ifndef PATHFINDER_ASTAR_H
#define PATHFINDER_ASTAR_H

#include "dem_reader.h"
#include "params.h"
#include "geojson_export.h"

/*
 * Ejecuta búsqueda A* entre las coordenadas geográficas de inicio y fin.
 * Genera el camino óptimo minimizando el coste multidimensional de edge_cost().
 */
bool astar_find_path(const DemContext       *ctx,
                     double                  start_lon,
                     double                  start_lat,
                     double                  goal_lon,
                     double                  goal_lat,
                     const CostWeights      *weights,
                     const TraversalLimits  *limits,
                     PathResult             *out_path);

/* Libera la memoria del PathResult */
void free_path_result(PathResult *path);

#endif /* PATHFINDER_ASTAR_H */
