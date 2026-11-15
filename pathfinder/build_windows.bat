@echo off
REM ============================================================================
REM build_windows.bat — Compilación de martian_pathfinder en Windows nativo (MinGW / GCC)
REM ============================================================================

echo [Speisaps] Compilando martian_pathfinder.exe para Windows...

gcc -O3 -std=c11 -fopenmp ^
    params.c ^
    mars_geo.c ^
    terrain_cost.c ^
    edge_cost.c ^
    dem_reader.c ^
    geojson_export.c ^
    pathfinder_astar.c ^
    radiation_bridge.c ^
    ../radiation_data/mars_env.c ^
    main_route.c ^
    -o martian_pathfinder.exe ^
    -lm

if %ERRORLEVEL% EQU 0 (
    echo [Speisaps] Compilacion exitosa: martian_pathfinder.exe generado correctamente.
) else (
    echo [ERROR] Fallo la compilacion. Verifique que gcc (MinGW-w64 o MSYS2) este instalado y en el PATH.
)
