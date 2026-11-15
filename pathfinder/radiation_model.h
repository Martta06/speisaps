/*
 * radiation_model.h — Modelo de radiación topográfico y temporal en Marte.
 *
 * Proporciona:
 *   1. Cálculo del factor de visión celeste (Sky-View Factor, SVF) y ángulo sólido
 *      de cielo visible (sr) mediante trazado de rayos de horizonte sobre el DEM.
 *   2. Estimación de la tasa de radiación en superficie (mSv/día) en función de la
 *      hora solar marciana, apantallamiento topográfico y alertas solares.
 */

#ifndef RADIATION_MODEL_H
#define RADIATION_MODEL_H

#include "dem_reader.h"
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Ángulo sólido de un hemisferio completo (2*pi sr ≈ 6.283185 sr) */
#define HEMISPHERE_SOLID_ANGLE_SR (2.0 * M_PI)

/* Dosis basal GCR medida por MSL RAD en Cráter Gale (mSv/día) */
#define GCR_BASE_DOSE_RATE_MSV_DAY 0.64

/*
 * radiation_sky_view_factor — Calcula el SVF \in [0.0, 1.0] y el ángulo sólido visible (sr).
 *
 * ctx:          Contexto del DEM mapeado en memoria.
 * lon_deg:      Longitud del punto a evaluar (°E).
 * lat_deg:      Latitud del punto a evaluar (°N).
 * n_azimuths:   Número de direcciones angulares a muestrear (ej. 16 o 32).
 * max_radius_m: Distancia máxima de búsqueda de horizonte en metros (ej. 100 a 300 m).
 * out_solid_angle_sr: Puntero opcional donde escribir el ángulo sólido visible en estereorradianes.
 *
 * Tratamiento de bordes: los rayos se truncan de forma segura al límite del DEM sin
 * provocar desbordamientos de memoria, asumiendo horizonte abierto en el exterior.
 *
 * Retorna SVF en [0.0, 1.0].
 */
double radiation_sky_view_factor(const DemContext *ctx,
                                 double lon_deg,
                                 double lat_deg,
                                 int n_azimuths,
                                 double max_radius_m,
                                 double *out_solid_angle_sr);

/*
 * radiation_dose_rate — Tasa de radiación instantánea (mSv/día).
 *
 * local_sol_hour: Hora solar local marciana decimal [0.0, 24.0).
 * svf:            Sky-View Factor del punto [0.0, 1.0].
 * sep_active:     Indica si hay una tormenta solar activa (Solar Energetic Particles).
 *
 * Retorna la tasa de dosis efectiva absorbida en mSv/día.
 */
double radiation_dose_rate(double local_sol_hour,
                           double svf,
                           bool sep_active);

#endif /* RADIATION_MODEL_H */
