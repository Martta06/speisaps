/*
 * radiation_model.c — Implementación del cálculo de SVF y modulación de dosis.
 */

#include "radiation_model.h"
#include <math.h>
#include <stdio.h>

double radiation_sky_view_factor(const DemContext *ctx,
                                 double lon_deg,
                                 double lat_deg,
                                 int n_azimuths,
                                 double max_radius_m,
                                 double *out_solid_angle_sr)
{
    if (!ctx || n_azimuths <= 0 || max_radius_m <= 0.0) {
        if (out_solid_angle_sr) *out_solid_angle_sr = HEMISPHERE_SOLID_ANGLE_SR;
        return 1.0;
    }

    int x0, y0;
    dem_geo_to_pixel(ctx, lon_deg, lat_deg, &x0, &y0);

    float z0;
    if (!dem_get_elevation(ctx, x0, y0, &z0)) {
        /* Fuera de límites o nodata -> horizonte completamente abierto */
        if (out_solid_angle_sr) *out_solid_angle_sr = HEMISPHERE_SOLID_ANGLE_SR;
        return 1.0;
    }

    /* Cada píxel en este DEM es ~1.0 metro */
    int max_steps = (int)ceil(max_radius_m);
    if (max_steps < 1) max_steps = 1;

    double sum_cos2_gamma = 0.0;
    double d_phi = (2.0 * M_PI) / (double)n_azimuths;

    for (int j = 0; j < n_azimuths; j++) {
        double phi = (double)j * d_phi;
        double cos_phi = cos(phi);
        double sin_phi = sin(phi);

        double max_tan_gamma = 0.0; /* Horizon elevation angle >= 0 */

        /* Marcha a lo largo del rayo */
        for (int step = 1; step <= max_steps; step++) {
            /* En coordenadas de imagen, Y crece hacia el sur */
            int px = x0 + (int)round((double)step * sin_phi);
            int py = y0 - (int)round((double)step * cos_phi);

            /* Truncamiento al borde del mapa */
            if (!dem_in_bounds(ctx, px, py)) {
                break;
            }

            float z_step;
            if (!dem_get_elevation(ctx, px, py, &z_step)) {
                continue;
            }

            double dz = (double)(z_step - z0);
            if (dz > 0.0) {
                double r = (double)step; /* r en metros */
                double tan_gamma = dz / r;
                if (tan_gamma > max_tan_gamma) {
                    max_tan_gamma = tan_gamma;
                }
            }
        }

        /* cos²(gamma) = 1 / (1 + tan²(gamma)) */
        double cos2_gamma = 1.0 / (1.0 + max_tan_gamma * max_tan_gamma);
        sum_cos2_gamma += cos2_gamma;
    }

    double svf = sum_cos2_gamma / (double)n_azimuths;
    if (svf > 1.0) svf = 1.0;
    if (svf < 0.0) svf = 0.0;

    if (out_solid_angle_sr) {
        *out_solid_angle_sr = HEMISPHERE_SOLID_ANGLE_SR * svf;
    }

    return svf;
}

double radiation_dose_rate(double local_sol_hour,
                           double svf,
                           bool sep_active)
{
    /* Normalizar hora al rango [0, 24) */
    double h = fmod(local_sol_hour, 24.0);
    if (h < 0.0) h += 24.0;

    /*
     * Modulación de la masa de columna atmosférica diurna (marea térmica de presión).
     * Curiosity REMS/RAD detectó que la presión máxima superficial ocurre alrededor de las
     * 06:00 - 08:00 LMST (mayor masa de gas CO2 arriba -> mayor atenuación, ~5% menos de dosis).
     * La presión mínima ocurre en la tarde (~17:00), donde la dosis aumenta levemente.
     */
    double phase = 2.0 * M_PI * (h - 7.0) / 24.0;
    double f_atm = 1.0 - 0.05 * cos(phase); /* Oscila entre 0.95 y 1.05 */

    /* El apantallamiento topográfico escala linealmente la dosis recibida del hemisferio */
    double clamped_svf = svf;
    if (clamped_svf < 0.0) clamped_svf = 0.0;
    if (clamped_svf > 1.0) clamped_svf = 1.0;

    double dose_rate = GCR_BASE_DOSE_RATE_MSV_DAY * clamped_svf * f_atm;

    /* Si hay tormenta solar activa (SEP), la tasa se incrementa drásticamente */
    if (sep_active) {
        dose_rate += 10.0 * clamped_svf; /* Pico adicional directo no atenuado */
    }

    return dose_rate;
}
