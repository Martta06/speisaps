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

    /* trafficability_cost */
    check("traff TI=200 (safe) → 0",  approx(trafficability_cost(200.0, 150.0), 0.0, 1e-9));
    check("traff TI=75  → 0.5",       approx(trafficability_cost(75.0,  150.0), 0.5, 1e-9));
    check("traff TI=0   → 1.0",       approx(trafficability_cost(0.0,   150.0), 1.0, 1e-9));

    /* rock_abundance_cost */
    check("rock RA=0.10/0.20 → 0.5",  approx(rock_abundance_cost(0.10, 0.20), 0.5, 1e-9));
    check("rock RA=0.25/0.20 → 1.0",  approx(rock_abundance_cost(0.25, 0.20), 1.0, 1e-9));

    /* hazard_proximity_cost */
    check("prox dist=5 excl=10 → INF", !isfinite(hazard_proximity_cost(5.0, 10.0, 20.0)));
    double p = hazard_proximity_cost(10.0 + 20.0, 10.0, 20.0);  /* 1/e */
    check("prox dist=excl+decay → 1/e", approx(p, exp(-1.0), 1e-9));

    /* elevation_cost */
    check("elev +10m w_up=4 → 40",    approx(elevation_cost( 10.0, 4.0, 1.5), 40.0, 1e-9));
    check("elev -10m w_dn=1.5 → 15",  approx(elevation_cost(-10.0, 4.0, 1.5), 15.0, 1e-9));
    check("elev 0m → 0",               approx(elevation_cost(  0.0, 4.0, 1.5),  0.0, 1e-9));

    /* unknown_cell_cost */
    check("cell SAFE → 0.0",     approx(unknown_cell_cost(CELL_SAFE),    0.0, 1e-9));
    check("cell UNSAFE → INF",   !isfinite(unknown_cell_cost(CELL_UNSAFE)));
    check("cell UNKNOWN → 1.0",  approx(unknown_cell_cost(CELL_UNKNOWN), 1.0, 1e-9));

    /* radiation_cost_per_meter */
    /* 0.64 mSv/día a 1 m/s = 0.64/86400 mSv/m ≈ 7.407e-6 */
    double r = radiation_cost_per_meter(0.64, 1.0);
    check("rad 0.64 mSv/día a 1 m/s", approx(r, 0.64 / 86400.0, 1e-12));

    /* wind_cost */
    check("wind 0 m/s → 0.0",        approx(wind_cost(0.0,  0.5, 20.0), 0.0, 1e-9));
    check("wind >= max → INF",        !isfinite(wind_cost(20.0, 0.5, 20.0)));
    /* 10 m/s, crosstrack=0.5 → (10/20) × (0.5+0.5×0.5) = 0.5 × 0.75 = 0.375 */
    check("wind 10/20 crosstrack=0.5", approx(wind_cost(10.0, 0.5, 20.0), 0.375, 1e-9));
}

/* ================================================================== */

static void test_edge_cost(void)
{
    puts("\n--- edge_cost ---");

    TraversalLimits lim = limits_default_foot_eva();
    CostWeights     w   = weights_default_foot_eva();

    /* Segmento nominal plano y seguro */
    EdgeParams ep = {
        .delta_s_m         = 10.0,
        .delta_z_m         = 0.0,
        .slope_deg         = 5.0,
        .roughness_vrm     = 0.1,
        .rock_abundance    = 0.05,
        .thermal_inertia   = 200.0,
        .hazard_distance_m = 50.0,
        .max_rock_height_m = 0.1,
        .cell_class        = CELL_SAFE,
        .dose_rate_msv_day = 0.64,
        .wind_multiplier   = 0.3,   /* 0.3 × max_wind_ms = 6 m/s */
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

    /* Celda insegura → INFINITY */
    ep.cell_class = CELL_UNSAFE;
    check("celda UNSAFE → INF", !isfinite(edge_cost(&ep, &w, &lim)));
    ep.cell_class = CELL_SAFE;

    /* SEP activo → INFINITY */
    lim.sep_alert_active = true;
    check("SEP activo → INF", !isfinite(edge_cost(&ep, &w, &lim)));
    lim.sep_alert_active = false;

    /* Roca demasiado alta → INFINITY */
    ep.max_rock_height_m = 1.0;
    check("roca > límite → INF", !isfinite(edge_cost(&ep, &w, &lim)));
    ep.max_rock_height_m = 0.1;

    /* Desglose de términos */
    double terms[COST_TERM_COUNT];
    double total = edge_cost_breakdown(&ep, &w, &lim, terms);
    check("breakdown total = edge_cost", approx(total, edge_cost(&ep, &w, &lim), 1e-9));
    printf("    desglose: dist=%.3f slope=%.3f rough=%.3f soft=%.3f rock=%.3f\n"
           "              prox=%.3f unk=%.3f rad=%.3f wind=%.3f elev=%.3f\n",
           terms[0], terms[1], terms[2], terms[3], terms[4],
           terms[5], terms[6], terms[7], terms[8], terms[9]);

    /* Rover: w_rad = 0 → la radiación no contribuye */
    CostWeights wr = weights_default_rover();
    check("rover w_rad=0 → no contribución rad",
          approx(wr.w_rad, 0.0, 1e-9));

    /* Coste de subida > coste de bajada con mismos pesos */
    ep.delta_z_m = +10.0;
    double cost_up = edge_cost(&ep, &w, &lim);
    ep.delta_z_m = -10.0;
    double cost_dn = edge_cost(&ep, &w, &lim);
    check("coste subida > coste bajada (EVA)", cost_up > cost_dn);
    ep.delta_z_m = 0.0;
}

/* ================================================================== */

int main(void)
{
    puts("============================================================");
    puts("  pathfinder — test de funciones de coste");
    puts("============================================================");

    test_mars_geo();
    test_terrain_cost();
    test_edge_cost();

    puts("\n============================================================");
    printf("  Resultado: %d PASS, %d FAIL\n", n_pass, n_fail);
    puts("============================================================");

    return n_fail == 0 ? 0 : 1;
}
