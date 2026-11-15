/*
 * test_cost.c — Tests unitarios manuales de las funciones de coste.
 *
 * Compilar:
 *   gcc -O2 -Wall -Wextra -std=c11 -lm \
 *       params.c mars_geo.c terrain_cost.c edge_cost.c test_cost.c \
 *       -o test_cost
 *
 * No usa ningún framework; imprime PASS/FAIL por stdout.
 */

#include <stdio.h>
#include <math.h>

#include "params.h"
#include "mars_geo.h"
#include "terrain_cost.h"
#include "edge_cost.h"
#include "radiation_bridge.h"
#include "pathfinder_astar.h"
#include "../radiation_data/mars_env.h"

/* ---- utilidad ---------------------------------------------------- */

static int n_pass = 0, n_fail = 0;

static void check(const char *name, int cond)
{
    if (cond) { printf("  PASS  %s\n", name); n_pass++; }
    else       { printf("  FAIL  %s\n", name); n_fail++; }
}

static inline int approx(double a, double b, double tol)
{
    return fabs(a - b) < tol;
}

/* ================================================================== */

static void test_mars_geo(void)
{
    puts("\n--- mars_geo ---");

    /* Distancia de Bradbury Landing a un punto 1 km al Norte.
     * 1000 m / (π/180 × R) = 0.016870° de latitud. */
    double lat0 =  -4.5895,       lon0 = 137.4417;  /* Bradbury Landing approx */
    double lat1 =  -4.5726293951, lon1 = 137.4417;  /* exactamente 1 km al Norte */
    double d = mars_haversine_m(lat0, lon0, lat1, lon1);
    check("haversine ~1 km Norte", approx(d, 1000.0, 1.0));

    /* Mismo punto → 0 */
    check("haversine mismo punto", approx(mars_haversine_m(lat0, lon0, lat0, lon0), 0.0, 1e-9));

    /* Pendiente: 100 m desnivel en 1000 m horizontal → atan(0.1) ≈ 5.71° */
    double slope = mars_slope_deg(100.0, 1000.0);
    check("slope_deg 100/1000", approx(slope, 5.711, 0.01));

    /* Pendiente con dz negativo: igual absoluto */
    check("slope_deg negativo", approx(mars_slope_deg(-100.0, 1000.0), 5.711, 0.01));

    /* Proyección local: 1 grado Norte ≈ 59 274 m */
    double e, n;
    mars_project_local(lat0, lon0, lat0 + 1.0, lon0, &e, &n);
    check("project_local 1° Norte ≈ 59 274 m", approx(n, 59274.0, 100.0));
    check("project_local sin componente Este",  approx(e, 0.0, 1.0));

    /* Inversa de la proyección */
    double rlat, rlon;
    mars_unproject_local(lat0, lon0, e, n, &rlat, &rlon);
    check("unproject_local lat", approx(rlat, lat0 + 1.0, 1e-6));

    /* Rumbo Norte → 0° */
    double brg = mars_bearing_deg(lat0, lon0, lat0 + 1.0, lon0);
    check("bearing Norte = 0°", approx(brg, 0.0, 0.1));

    /* Componente lateral: viento E (90°) marchando N (0°) → perpendicular = 1 */
    double ct = mars_wind_crosstrack_factor(0.0, 90.0);
    check("crosstrack perpendicular = 1.0", approx(ct, 1.0, 1e-9));

    /* Viento de cara (N = 0°, marchando N = 0°) → 0 */
    ct = mars_wind_crosstrack_factor(0.0, 180.0);  /* viento del S, sopla al N */
    check("crosstrack de cara = 0.0", approx(ct, 0.0, 1e-9));
}

/* ================================================================== */

static void test_terrain_cost(void)
{
    puts("\n--- terrain_cost ---");

    /* slope_cost */
    check("slope 0° → 0.0",        approx(slope_cost(0.0, 20.0), 0.0, 1e-9));
    check("slope 10°/20° → 0.25",  approx(slope_cost(10.0, 20.0), 0.25, 1e-9));
    check("slope 20°/20° → INF",   !isfinite(slope_cost(20.0, 20.0)));
    check("slope 25°/20° → INF",   !isfinite(slope_cost(25.0, 20.0)));

    /* roughness_cost */
    check("roughness 0.0 → 0.0",   approx(roughness_cost(0.0),  0.0, 1e-9));
    check("roughness 0.5 → 0.5",   approx(roughness_cost(0.5),  0.5, 1e-9));
    check("roughness 1.5 → 1.0",   approx(roughness_cost(1.5),  1.0, 1e-9));

    /* elevation_cost */
    check("elev +10m w_up=4 → 40",    approx(elevation_cost( 10.0, 4.0, 1.5), 40.0, 1e-9));
    check("elev -10m w_dn=1.5 → 15",  approx(elevation_cost(-10.0, 4.0, 1.5), 15.0, 1e-9));
    check("elev 0m → 0",               approx(elevation_cost(  0.0, 4.0, 1.5),  0.0, 1e-9));

    /* radiation_cost_per_meter */
    double r = radiation_cost_per_meter(0.64, 1.0);
    check("rad 0.64 mSv/día a 1 m/s", approx(r, 0.64 / 86400.0, 1e-12));

    /* radiation_cost_normalized: referencia GCR → 1.0; doble tasa → 2.0 */
    check("rad normalizada GCR ref → 1.0",  approx(radiation_cost_normalized(0.64, 1.0), 1.0, 1e-9));
    check("rad normalizada 2x ref → 2.0",   approx(radiation_cost_normalized(1.28, 1.0), 2.0, 1e-9));
    check("rad normalizada mitad vel → 2x", approx(radiation_cost_normalized(0.64, 0.5), 2.0, 1e-9));
}

/* ================================================================== */

static void test_env_state(void)
{
    puts("\n--- DynamicEnvironmentState (SPE) ---");

    DynamicEnvironmentState eva   = env_state_calm(MODE_FOOT_EVA);
    DynamicEnvironmentState rover = env_state_calm(MODE_ROVER);
    check("calma: sin SPE",                 !eva.spe_active);
    check("calma: tasa SPE = 0",            approx(env_spe_rate_at(&eva, 100.0), 0.0, 1e-12));
    check("blindaje EVA = 1.0",             approx(eva.vehicle_shielding, 1.0, 1e-12));
    check("blindaje rover = 0.85",          approx(rover.vehicle_shielding, 0.85, 1e-12));
    check("NULL → tasa SPE = 0",            approx(env_spe_rate_at(NULL, 0.0), 0.0, 1e-12));

    /* SPE prevista: llega a los 600 s y dura hasta los 3600 s */
    DynamicEnvironmentState spe = eva;
    spe.spe_active = true;
    spe.spe_storm_rate_uSv_h = 150.0;
    spe.spe_onset_s = 600.0;
    spe.spe_end_s = 3600.0;
    check("SPE antes del onset → 0",        approx(env_spe_rate_at(&spe,  599.0),   0.0, 1e-12));
    check("SPE en el onset → 150",          approx(env_spe_rate_at(&spe,  600.0), 150.0, 1e-12));
    check("SPE dentro de ventana → 150",    approx(env_spe_rate_at(&spe, 2000.0), 150.0, 1e-12));
    check("SPE en el fin → 0",              approx(env_spe_rate_at(&spe, 3600.0),   0.0, 1e-12));
}

/* ================================================================== */

static void test_edge_cost(void)
{
    puts("\n--- edge_cost ---");

    TraversalLimits lim = limits_default_foot_eva();
    CostWeights     w   = weights_default_foot_eva();

    /* Segmento nominal plano y seguro con datos limpios */
    EdgeParams ep = {
        .delta_s_m         = 10.0,
        .delta_z_m         = 0.0,
        .slope_deg         = 5.0,
        .roughness         = 0.1,
        .elapsed_time_s    = 0.0,
        .dose_rate_msv_day = 0.64,
        .temp_c            = -10.0, /* dentro de zona de confort */
        .speed_ms          = 1.0
    };

    double cost = edge_cost(&ep, &w, &lim);
    check("segmento nominal: coste finito > 0",
          isfinite(cost) && cost > 0.0);
    printf("    coste nominal = %.4f\n", cost);

    /* Pendiente superior al límite → INFINITY */
    ep.slope_deg = 25.0;
    check("pendiente 25° EVA → INF", !isfinite(edge_cost(&ep, &w, &lim)));
    ep.slope_deg = 5.0;

    /* Rugosidad superior al límite → INFINITY */
    ep.roughness = 0.95;
    check("rugosidad > límite → INF", !isfinite(edge_cost(&ep, &w, &lim)));
    ep.roughness = 0.1;

    /* Temperatura fuera de límites → INFINITY */
    ep.temp_c = -95.0;
    check("temperatura < min_temp → INF", !isfinite(edge_cost(&ep, &w, &lim)));
    ep.temp_c = -10.0;

    /* SPE: ya NO bloquea; encarece el tramo en proporción a la tasa de dosis */
    double cost_calm = edge_cost(&ep, &w, &lim);
    ep.dose_rate_msv_day = 0.64 + 150.0 * 24.0 / 1000.0; /* GCR + SPE 150 uSv/h */
    double cost_spe = edge_cost(&ep, &w, &lim);
    check("SPE no bloquea (coste finito)", isfinite(cost_spe));
    check("SPE encarece el tramo",         cost_spe > cost_calm);
    printf("    coste calma = %.3f | coste SPE(150 uSv/h) = %.3f\n", cost_calm, cost_spe);
    ep.dose_rate_msv_day = 0.64;

    /* Desglose de términos */
    double terms[COST_TERM_COUNT];
    double total = edge_cost_breakdown(&ep, &w, &lim, terms);
    check("breakdown total = edge_cost", approx(total, edge_cost(&ep, &w, &lim), 1e-9));
    printf("    desglose: dist=%.3f slope=%.3f rough=%.3f rad=%.3f temp=%.3f elev=%.3f\n",
           terms[0], terms[1], terms[2], terms[3], terms[4], terms[5]);

    /* Rover: la radiación cuenta, pero menos que en EVA */
    CostWeights wr = weights_default_rover();
    check("rover 0 < w_rad < w_rad EVA",
          wr.w_rad > 0.0 && wr.w_rad < w.w_rad);

    /* Coste de subida > coste de bajada con mismos pesos */
    ep.delta_z_m = +10.0;
    double cost_up = edge_cost(&ep, &w, &lim);
    ep.delta_z_m = -10.0;
    double cost_dn = edge_cost(&ep, &w, &lim);
    check("coste subida > coste bajada (EVA)", cost_up > cost_dn);
    ep.delta_z_m = 0.0;
}

/* ================================================================== */

static void test_radiation_model(void)
{
    puts("\n--- radiation_bridge (mars_env.c) ---");

    bool ok_init = radiation_init("../radiation_data/gale_shielding_lut.bin",
                                  "../radiation_data/rad_baseline_24h.bin");
    check("radiation_init desde radiation_data/ exitoso", ok_init);

    /* 1. Consulta en Bradbury Landing (137.425, -4.580) a las 12:00 LMST */
    int px = 18000, py = 20000;
    double rate_open = radiation_query_dose_rate_msv_day(12.0f, px, py, false, 0.0f);
    check("dose_rate en terreno abierto positivo y finito (~0.65 mSv/día)",
          isfinite(rate_open) && rate_open > 0.50 && rate_open < 0.80);

    /* 2. Modo rover aplica factor de blindaje metálico */
    double rate_rover = radiation_query_dose_rate_msv_day(12.0f, px, py, true, 0.0f);
    check("dose_rate rover < dose_rate astronauta EVA", rate_rover < rate_open);

    /* 3. Modulación horaria de la curva de 24 horas */
    double rate_h0 = radiation_query_dose_rate_msv_day(0.0f, px, py, false, 0.0f);
    double rate_h13 = radiation_query_dose_rate_msv_day(13.0f, px, py, false, 0.0f);
    check("curva 24h modula según hora del sol", rate_h0 != rate_h13);

    /* 4. Tormenta solar (SEP activa) incrementa dosis */
    double rate_sep = radiation_query_dose_rate_msv_day(12.0f, px, py, false, 100.0f);
    check("tormenta solar (SEP) incrementa dosis", rate_sep > rate_open);

    /* 4b. Versión con blindaje explícito coincide con la versión bool */
    double rate_sh = radiation_query_dose_rate_shielded(12.0f, px, py, 0.85f, 0.0f);
    check("shielded(0.85) == bool rover", approx(rate_sh, rate_rover, 1e-9));

    /* 5. Comprobación de get_mars_temperature() de mars_env.c */
    MarsTemperature mt_noon = get_mars_temperature(14.5f, -4500.0f, 190.0f, 280.0f, -4500.0f);
    check("temp mediodía en Gale aprox +6.8°C", approx(mt_noon.temp_c, 6.85, 0.5));
    MarsTemperature mt_dawn = get_mars_temperature(6.0f, -4500.0f, 190.0f, 280.0f, -4500.0f);
    check("temp amanecer en Gale aprox -83°C", approx(mt_dawn.temp_c, -83.15, 0.5));
    check("temp mediodía > temp amanecer", mt_noon.temp_c > mt_dawn.temp_c);

    /* 6. Comprobación con DEM real */
    DemContext ctx;
    if (dem_open("../Gale_DEM_SMG_1m.tif", &ctx) ||
        dem_open("../../geotiff/Gale_DEM_SMG_1m.tif", &ctx)) {
        double rough_plain = 0.0;
        dem_geo_to_pixel(&ctx, 137.425, -4.580, &px, &py);
        bool ok_r = dem_get_roughness(&ctx, px, py, &rough_plain);
        check("dem_get_roughness en llanura válido", ok_r);
        check("rugosidad en llanura está en [0.0, 0.4]", rough_plain >= 0.0 && rough_plain <= 0.4);

        /* 6. Comprobación espacio-temporal en A*: travesía a distintas horas */
        TraversalLimits limits = limits_default_foot_eva();
        CostWeights weights = weights_default_foot_eva();
        PathResult p_morning, p_evening;

        DynamicEnvironmentState env = env_state_calm(MODE_FOOT_EVA);
        bool ok_m = astar_find_path(&ctx, 137.425, -4.580, 137.426, -4.581, 7.0, 1, &weights, &limits, &env, &p_morning);
        bool ok_e = astar_find_path(&ctx, 137.425, -4.580, 137.426, -4.581, 13.0, 1, &weights, &limits, &env, &p_evening);

        check("A* espacio-temporal ejecutado a las 07:00", ok_m);
        check("A* espacio-temporal ejecutado a las 13:00", ok_e);
        if (ok_m && ok_e) {
            check("Tiempo transcurrido monótono creciente",
                  p_morning.points[p_morning.count - 1].elapsed_time_s > p_morning.points[0].elapsed_time_s);
            check("Duración estimada positiva", p_morning.total_duration_s > 0.0);
            check("Dosis acumulada positiva y consistente", p_morning.total_radiation_dose_msv > 0.0);
            free_path_result(&p_morning);
            free_path_result(&p_evening);
        }

        /* 7. SPE en A*: la ruta existe (no se bloquea) y acumula más dosis */
        DynamicEnvironmentState env_spe = env;
        env_spe.spe_active = true;
        env_spe.spe_storm_rate_uSv_h = 150.0;
        PathResult p_calm, p_spe;
        bool ok_c = astar_find_path(&ctx, 137.425, -4.580, 137.426, -4.581, 9.0, 1, &weights, &limits, &env,     &p_calm);
        bool ok_s = astar_find_path(&ctx, 137.425, -4.580, 137.426, -4.581, 9.0, 1, &weights, &limits, &env_spe, &p_spe);
        check("A* con SPE encuentra ruta (no bloqueo global)", ok_s);
        if (ok_c && ok_s) {
            check("dosis con SPE > dosis en calma",
                  p_spe.total_radiation_dose_msv > p_calm.total_radiation_dose_msv);
            check("PathResult marca spe_active", p_spe.spe_active && !p_calm.spe_active);
            printf("    dosis calma = %.6f mSv | dosis SPE = %.6f mSv\n",
                   p_calm.total_radiation_dose_msv, p_spe.total_radiation_dose_msv);
            free_path_result(&p_calm);
            free_path_result(&p_spe);
        }

        /* 8. Tormenta de arena dinámica en medio del camino: A* la rodea */
        DynamicEnvironmentState env_dust = env;
        env_dust.dust_storm_active      = 1;
        env_dust.storm_center_lon       = 137.4255;
        env_dust.storm_center_lat       = -4.5805;
        env_dust.storm_radius_m         = 40.0;
        env_dust.storm_vel_east_ms      = 0.0;
        env_dust.storm_vel_north_ms     = 0.0;
        env_dust.storm_severity_penalty = 50.0f;
        PathResult p_base, p_detour;
        bool ok_b = astar_find_path(&ctx, 137.425, -4.580, 137.426, -4.581, 9.0, 1, &weights, &limits, &env,      &p_base);
        bool ok_d = astar_find_path(&ctx, 137.425, -4.580, 137.426, -4.581, 9.0, 1, &weights, &limits, &env_dust, &p_detour);
        check("A* rodea tormenta de arena activa", ok_d);
        if (ok_b && ok_d) {
            check("Desvío por tormenta de arena alarga la distancia frente a ruta libre",
                  p_detour.total_distance_m > p_base.total_distance_m);
            printf("    dist libre = %.1f m | dist rodeando tormenta = %.1f m\n",
                   p_base.total_distance_m, p_detour.total_distance_m);
            free_path_result(&p_base);
            free_path_result(&p_detour);
        }

        dem_close(&ctx);
    }

    radiation_cleanup();
}

/* ================================================================== */

int main(void)
{
    puts("============================================================");
    puts("  pathfinder — test de funciones de coste");
    puts("============================================================");

    test_mars_geo();
    test_terrain_cost();
    test_env_state();
    test_edge_cost();
    test_radiation_model();

    puts("\n============================================================");
    printf("  Resultado: %d PASS, %d FAIL\n", n_pass, n_fail);
    puts("============================================================");

    return n_fail == 0 ? 0 : 1;
}
