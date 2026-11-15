/*
 * radiation_bridge.c — Implementación del puente hacia mars_env.c.
 */

#include "radiation_bridge.h"
#include "../radiation_data/mars_env.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

static float *s_shielding_lut = NULL;
static size_t s_lut_size_bytes = 0;
static int    s_lut_fd = -1;
static float  s_baseline_24h[24];
static bool   s_is_ready = false;

bool radiation_init(const char *lut_bin_path, const char *baseline_24h_path) {
    if (s_is_ready) return true;

    /* 1. Cargar curva de 24 horas (96 bytes) */
    FILE *f_base = fopen(baseline_24h_path, "rb");
    if (!f_base) {
        perror("radiation_init: no se pudo abrir rad_baseline_24h.bin");
        return false;
    }
    if (fread(s_baseline_24h, sizeof(float), 24, f_base) != 24) {
        fprintf(stderr, "radiation_init: error leyendo rad_baseline_24h.bin\n");
        fclose(f_base);
        return false;
    }
    fclose(f_base);

    /* 2. Mapear en memoria gale_shielding_lut.bin (56 MB) */
    s_lut_fd = open(lut_bin_path, O_RDONLY);
    if (s_lut_fd < 0) {
        perror("radiation_init: no se pudo abrir gale_shielding_lut.bin");
        return false;
    }

    struct stat st;
    if (fstat(s_lut_fd, &st) < 0) {
        perror("radiation_init: fstat falló en LUT");
        close(s_lut_fd);
        s_lut_fd = -1;
        return false;
    }
    s_lut_size_bytes = (size_t)st.st_size;

    void *map = mmap(NULL, s_lut_size_bytes, PROT_READ, MAP_SHARED, s_lut_fd, 0);
    if (map == MAP_FAILED) {
        perror("radiation_init: mmap falló en LUT");
        close(s_lut_fd);
        s_lut_fd = -1;
        return false;
    }

    s_shielding_lut = (float *)map;
    s_is_ready = true;
    return true;
}

void radiation_cleanup(void) {
    if (s_shielding_lut && s_shielding_lut != MAP_FAILED) {
        munmap(s_shielding_lut, s_lut_size_bytes);
        s_shielding_lut = NULL;
    }
    if (s_lut_fd >= 0) {
        close(s_lut_fd);
        s_lut_fd = -1;
    }
    s_is_ready = false;
}

bool radiation_is_ready(void) {
    return s_is_ready;
}

double radiation_query_dose_rate_msv_day(float t_lmst,
                                         int dem_pixel_x,
                                         int dem_pixel_y,
                                         bool is_rover,
                                         float sep_storm_uSv_h)
{
    if (!s_is_ready) {
        /* Fallback de seguridad: ~0.65 mSv/día */
        return 0.65;
    }

    /*
     * El DEM original de HiRISE es de 36000 x 40100 a 1.0 m/px.
     * La LUT de apantallamiento de mars_env es de 4010 x 3600 a 10.0 m/px.
     * Mapeamos: col = dem_pixel_x / 10, row = dem_pixel_y / 10.
     */
    int col = dem_pixel_x / 10;
    int row = dem_pixel_y / 10;

    /* Rover shielding: ~0.85 para rover Curiosity, 1.0 para EVA a pie */
    float rover_shield = is_rover ? 0.85f : 1.0f;

    /*
     * Llamada a get_radiation_dose_rate() de mars_env.c:
     * Devuelve la tasa en uSv/h.
     */
    float rate_uSv_h = get_radiation_dose_rate(
        t_lmst,
        row,
        col,
        RAD_LUT_ROWS,
        RAD_LUT_COLS,
        s_baseline_24h,
        s_shielding_lut,
        rover_shield,
        sep_storm_uSv_h
    );

    /* Convertir uSv/h -> mSv/día: rate * 24.0 / 1000.0 */
    return (double)rate_uSv_h * 24.0 / 1000.0;
}
