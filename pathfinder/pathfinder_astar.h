/*
 * pathfinder_astar.h — Motor de búsqueda A* sobre la malla ráster DEM.
 */

#ifndef PATHFINDER_ASTAR_H
#define PATHFINDER_ASTAR_H

#include "dem_reader.h"
#include "params.h"
#include "geojson_export.h"

/*
 * Ejecuta búsqueda A* espacio-temporal entre las coordenadas geográficas de inicio y fin.
 *
 * start_sol_hour: Hora solar local marciana decimal en el momento de partida [0.0, 24.0).
 * step_meters:    Paso de muestreo espacial en metros (ej. 1.0 para resolución métrica completa,
 *                 o 10.0 para acelerar la búsqueda ~50x-100x en travesías largas de 10-30 km).
 * env:            Estado ambiental dinámico (SPE + blindaje del vehículo). Si es NULL se
 *                 asume calma y sin blindaje (1.0, conservador).
 *                 En cada arista se evalúa la tasa SPE vigente en el instante de llegada
 *                 (env_spe_rate_at), así una SPE prevista solo penaliza los tramos que se
 *                 recorrerían durante la tormenta.
 */
bool astar_find_path(const DemContext              *ctx,
                     double                         start_lon,
                     double                         start_lat,
                     double                         goal_lon,
                     double                         goal_lat,
                     double                         start_sol_hour,
                     int                            step_meters,
                     const CostWeights             *weights,
                     const TraversalLimits         *limits,
                     const DynamicEnvironmentState *env,
                     PathResult                    *out_path);

/* Libera la memoria del PathResult */
void free_path_result(PathResult *path);

#endif /* PATHFINDER_ASTAR_H */
