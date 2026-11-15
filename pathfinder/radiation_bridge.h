/*
 * radiation_bridge.h — Conexión e integración con el módulo mars_env de radiation_data.
 *
 * Utiliza get_radiation_dose_rate() de mars_env.h sobre la tabla gale_shielding_lut.bin
 * y la curva baseline de 24 horas rad_baseline_24h.bin (en uSv/h).
 * Proporciona una tasa normalizada y lista para el motor de pathfinding en mSv/día.
 */

#ifndef RADIATION_BRIDGE_H
#define RADIATION_BRIDGE_H

#include <stdbool.h>

/* Dimensiones del mapa de apantallamiento de Gale Crater (4010 x 3600 a 10m/px) */
#define RAD_LUT_ROWS 4010
#define RAD_LUT_COLS 3600

/* Inicializa el sistema de radiación cargando los archivos binarios de radiation_data/ */
bool radiation_init(const char *lut_bin_path, const char *baseline_24h_path);

/* Libera la memoria cargada */
void radiation_cleanup(void);

/* Comprueba si la LUT y la curva están cargadas */
bool radiation_is_ready(void);

/*
 * radiation_query_dose_rate_msv_day — Consulta la tasa de radiación en mSv/día.
 *
 * t_lmst:                 Hora solar local marciana decimal [0, 24).
 * dem_pixel_x, dem_pixel_y: Coordenadas de píxel en el DEM de 1 m (36000 x 40100).
 * is_rover:               true para aplicar el apantallamiento del vehículo (Curiosity).
 * sep_storm_uSv_h:        Tasa de tormenta solar de partículas en uSv/h (0.0 si no hay tormenta).
 *
 * Internamente mapea (dem_pixel_x, dem_pixel_y) al espacio (col, row) de la LUT
 * dividiendo por 10 (resolución 10 m/px) y llama a get_radiation_dose_rate() de mars_env.
 * Convierte el retorno de uSv/h a mSv/día (* 24.0 / 1000.0).
 */
double radiation_query_dose_rate_msv_day(float t_lmst,
                                         int dem_pixel_x,
                                         int dem_pixel_y,
                                         bool is_rover,
                                         float sep_storm_uSv_h);

#endif /* RADIATION_BRIDGE_H */
