/*
 * mars_geo.h — Geometría y conversiones de coordenadas marcianas.
 *
 * Todas las funciones son puras (sin estado, sin I/O).
 * Radio marciano medio: 3 396 190 m (IAU 2015, esfera areocéntrica).
 *
 * Convención de coordenadas:
 *   - Latitud areocéntrica, grados, positiva al Norte.
 *   - Longitud Este-positiva, [0°, 360°) o [−180°, 180°).
 *   - Para distancias cortas (< 50 km) se puede usar proyección plana
 *     local con error < 0.03 %; ver mars_project_local().
 */

#ifndef MARS_GEO_H
#define MARS_GEO_H

/* Radio ecuatorial marciano (m), IAU 2015 */
#define MARS_RADIUS_M 3396190.0

/*
 * mars_haversine_m — Distancia geodésica entre dos puntos (m).
 *
 * Usa la fórmula haversine con radio marciano. Precisa para cualquier
 * separación angular; evitar para puntos antípodas (raro en la práctica).
 *
 * lat1, lon1, lat2, lon2: coordenadas en grados decimales.
 */
double mars_haversine_m(double lat1_deg, double lon1_deg,
                        double lat2_deg, double lon2_deg);

/*
 * mars_bearing_deg — Rumbo inicial de p1 → p2 (grados, 0 = Norte, horario).
 *
 * Útil para comparar con la dirección del viento y calcular componente
 * frontal o lateral.
 */
double mars_bearing_deg(double lat1_deg, double lon1_deg,
                        double lat2_deg, double lon2_deg);

/*
 * mars_slope_deg — Pendiente media de un segmento (grados desde horizontal).
 *
 * dz_m: cambio de elevación signed (positivo = subida).
 * ds_m: longitud horizontal del segmento (> 0).
 *
 * Devuelve el ángulo absoluto de inclinación (siempre ≥ 0).
 * El signo de dz_m se conserva aparte para saber si es subida o bajada.
 */
double mars_slope_deg(double dz_m, double ds_m);

/*
 * Proyección plana local (ENU) — útil para zonas pequeñas (< 50 km).
 *
 * Convierte (lat, lon) a (Este, Norte) en metros respecto a un origen
 * de referencia. Escala métrica: 1 grado ≈ 59 274 m en Marte.
 *
 * lat_ref, lon_ref: origen de la proyección.
 * lat, lon:        punto a proyectar.
 * out_east_m, out_north_m: salida en metros.
 */
void mars_project_local(double lat_ref_deg, double lon_ref_deg,
                        double lat_deg,     double lon_deg,
                        double *out_east_m, double *out_north_m);

/*
 * mars_unproject_local — Inversa de mars_project_local.
 *
 * east_m, north_m: coordenadas locales en metros.
 * out_lat_deg, out_lon_deg: latitud y longitud resultantes.
 */
void mars_unproject_local(double lat_ref_deg, double lon_ref_deg,
                          double east_m,      double north_m,
                          double *out_lat_deg, double *out_lon_deg);

/*
 * mars_wind_crosstrack_factor — Componente lateral del viento relativa
 * al rumbo de marcha.
 *
 * Devuelve un factor en [0, 1]: 0 = viento de cara/espalda (sin desestabilización
 * lateral), 1 = viento perpendicular (máximo efecto de derribo en EVA).
 *
 * heading_deg: rumbo de marcha (0 = Norte).
 * wind_from_deg: dirección de procedencia del viento (meteorológica, 0 = Norte).
 */
double mars_wind_crosstrack_factor(double heading_deg, double wind_from_deg);

#endif /* MARS_GEO_H */
