/*
 * mars_geo.c — Implementación de geometría marciana.
 */

#include "mars_geo.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---- utilidades internas ----------------------------------------- */

static inline double deg2rad(double d) { return d * (M_PI / 180.0); }
static inline double rad2deg(double r) { return r * (180.0 / M_PI); }

/* ------------------------------------------------------------------ */

double mars_haversine_m(double lat1_deg, double lon1_deg,
                        double lat2_deg, double lon2_deg)
{
    double lat1 = deg2rad(lat1_deg);
    double lat2 = deg2rad(lat2_deg);
    double dlat = deg2rad(lat2_deg - lat1_deg);
    double dlon = deg2rad(lon2_deg - lon1_deg);

    double a = sin(dlat * 0.5) * sin(dlat * 0.5)
             + cos(lat1) * cos(lat2)
             * sin(dlon * 0.5) * sin(dlon * 0.5);

    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return MARS_RADIUS_M * c;
}

/* ------------------------------------------------------------------ */

double mars_bearing_deg(double lat1_deg, double lon1_deg,
                        double lat2_deg, double lon2_deg)
{
    double lat1 = deg2rad(lat1_deg);
    double lat2 = deg2rad(lat2_deg);
    double dlon = deg2rad(lon2_deg - lon1_deg);

    double x = sin(dlon) * cos(lat2);
    double y = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dlon);

    double bearing = rad2deg(atan2(x, y));
    /* normalizar a [0, 360) */
    return fmod(bearing + 360.0, 360.0);
}

/* ------------------------------------------------------------------ */

double mars_slope_deg(double dz_m, double ds_m)
{
    if (ds_m <= 0.0) return 0.0;
    return rad2deg(atan(fabs(dz_m) / ds_m));
}

/* ------------------------------------------------------------------ */

void mars_project_local(double lat_ref_deg, double lon_ref_deg,
                        double lat_deg,     double lon_deg,
                        double *out_east_m, double *out_north_m)
{
    /*
     * Proyección equirectangular centrada en (lat_ref, lon_ref).
     * Escala Norte: R × (1°) = R × π/180
     * Escala Este:  R × cos(lat_ref) × (1°)
     */
    double lat_ref = deg2rad(lat_ref_deg);
    double scale_n = MARS_RADIUS_M * (M_PI / 180.0);
    double scale_e = MARS_RADIUS_M * cos(lat_ref) * (M_PI / 180.0);

    *out_north_m = scale_n * (lat_deg - lat_ref_deg);
    *out_east_m  = scale_e * (lon_deg - lon_ref_deg);
}

void mars_unproject_local(double lat_ref_deg, double lon_ref_deg,
                          double east_m,      double north_m,
                          double *out_lat_deg, double *out_lon_deg)
{
    double lat_ref = deg2rad(lat_ref_deg);
    double scale_n = MARS_RADIUS_M * (M_PI / 180.0);
    double scale_e = MARS_RADIUS_M * cos(lat_ref) * (M_PI / 180.0);

    *out_lat_deg = lat_ref_deg + north_m / scale_n;
    *out_lon_deg = lon_ref_deg + east_m  / scale_e;
}

/* ------------------------------------------------------------------ */

double mars_wind_crosstrack_factor(double heading_deg, double wind_from_deg)
{
    /*
     * El viento "de procedencia" sopla hacia el rumbo opuesto.
     * Componente lateral = |sin(ángulo entre rumbo y dirección del viento)|.
     * Ángulo relativo entre el rumbo de marcha y el vector viento (hacia dónde sopla).
     */
    double wind_to_deg = fmod(wind_from_deg + 180.0, 360.0);
    double diff = deg2rad(wind_to_deg - heading_deg);
    return fabs(sin(diff));
}

/* ------------------------------------------------------------------ */

void mars_lonlat_to_pixel(double lon_deg, double lat_deg,
                          double pixel_size_m,
                          int *out_row, int *out_col)
{
    if (pixel_size_m <= 0.0) {
        pixel_size_m = 10.0;
    }

    double east_m = 0.0;
    double north_m = 0.0;
    mars_project_local(GALE_DEM_ORIGIN_LAT_DEG, GALE_DEM_ORIGIN_LON_DEG,
                       lat_deg, lon_deg, &east_m, &north_m);

    /* Compensar el factor cos(lat_ref) para coincidir exactamente con la rejilla equirectangular del GeoTIFF */
    double cos_ref = cos(deg2rad(GALE_DEM_ORIGIN_LAT_DEG));
    double x_m = (cos_ref > 1e-9) ? (east_m / cos_ref) : east_m;
    double y_m = -north_m; /* En el ráster las filas (row) crecen de Norte a Sur */

    if (out_col) *out_col = (int)round(x_m / pixel_size_m);
    if (out_row) *out_row = (int)round(y_m / pixel_size_m);
}
