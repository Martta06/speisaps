import json
import numpy as np
from PIL import Image
import matplotlib.pyplot as plt
import matplotlib.patheffects as pe
from matplotlib.colors import LinearSegmentedColormap
import os

Image.MAX_IMAGE_PIXELS = None

# Cargar DEM ligero para renderizado
dem_path = '/home/eneko/speisaps/gale_dem_ligero.tif'
im = Image.open(dem_path)
dem_arr = np.array(im, dtype=np.float32)

scale_x = 0.000607341774892113
scale_y = 0.0006072811982139869
origin_lon = 137.12247113204074
origin_lat = -4.248963057145466

h, w = dem_arr.shape
extent_lon_min = origin_lon
extent_lon_max = origin_lon + w * scale_x
extent_lat_max = origin_lat
extent_lat_min = origin_lat - h * scale_y

dem_arr[dem_arr <= -1e30] = np.nan

# Escala hipsométrica continua multicolor para Marte
mars_colors = [
    (0.00, '#03051e'),
    (0.20, '#004c6d'),
    (0.35, '#00838f'),
    (0.50, '#2e7d32'),
    (0.65, '#cddc39'),
    (0.78, '#ffb300'),
    (0.88, '#f4511e'),
    (0.96, '#b71c1c'),
    (1.00, '#ffffff')
]
cmap_hypso = LinearSegmentedColormap.from_list('mars_hypso', [c[1] for c in mars_colors])

def render_comprehensive_route(geojson_path, output_png_path, title_main, title_detail):
    with open(geojson_path) as f:
        gj = json.load(f)

    coords = gj['features'][0]['geometry']['coordinates']
    props = gj['features'][0]['properties']
    route_lons = [c[0] for c in coords]
    route_lats = [c[1] for c in coords]
    route_elevs = [c[2] for c in coords]
    route_times_min = [c[3] / 60.0 for c in coords]
    route_rads = [c[4] for c in coords] # mSv/day
    has_temp = len(coords[0]) >= 6
    route_temps = [c[5] for c in coords] if has_temp else []

    fig = plt.figure(figsize=(22, 12), facecolor='#0b0e14')
    gs = fig.add_gridspec(3, 2, width_ratios=[1.1, 1.2], height_ratios=[1.1, 0.45, 0.45])

    ax1 = fig.add_subplot(gs[:, 0])
    ax2 = fig.add_subplot(gs[0, 1])
    ax3 = fig.add_subplot(gs[1, 1])
    ax4 = fig.add_subplot(gs[2, 1])

    # 1. Overview Gale Crater
    im1 = ax1.imshow(dem_arr, extent=[extent_lon_min, extent_lon_max, extent_lat_min, extent_lat_max],
                     cmap=cmap_hypso, origin='upper')
    cbar1 = plt.colorbar(im1, ax=ax1, orientation='horizontal', fraction=0.046, pad=0.07)
    cbar1.set_label('Cota Topográfica Marciana (m)', color='white', fontsize=11)
    cbar1.ax.tick_params(colors='white')

    ax1.plot(route_lons, route_lats, color='#00ffff', linewidth=3.5,
             path_effects=[pe.Stroke(linewidth=5.5, foreground='black'), pe.Normal()],
             label=f'Ruta ({props["total_distance_m"]/1000.0:.2f} km)')
    ax1.plot(route_lons[0], route_lats[0], 'o', color='#00ff66', markersize=9,
             markeredgecolor='black', markeredgewidth=1.5, label='Origen')
    ax1.plot(route_lons[-1], route_lats[-1], 's', color='#ff0055', markersize=9,
             markeredgecolor='black', markeredgewidth=1.5, label='Destino')
    ax1.set_title(title_main, color='white', fontsize=14, pad=10)
    ax1.set_xlabel('Longitud (°E)', color='white')
    ax1.set_ylabel('Latitud (°N)', color='white')
    ax1.tick_params(colors='white')
    ax1.legend(loc='lower right', facecolor='#161b22', edgecolor='#30363d', labelcolor='white')

    # 2. Local Detail
    dlon = max(route_lons) - min(route_lons)
    dlat = max(route_lats) - min(route_lats)
    margin = max(0.008, max(dlon, dlat) * 0.12)

    px_min = int(max(0, (min(route_lons) - margin - origin_lon) / scale_x))
    px_max = int(min(w, (max(route_lons) + margin - origin_lon) / scale_x))
    py_min = int(max(0, (origin_lat - (max(route_lats) + margin)) / scale_y))
    py_max = int(min(h, (origin_lat - (min(route_lats) - margin)) / scale_y))

    crop_dem = dem_arr[py_min:py_max, px_min:px_max]
    crop_extent = [
        origin_lon + px_min * scale_x,
        origin_lon + px_max * scale_x,
        origin_lat - py_max * scale_y,
        origin_lat - py_min * scale_y
    ]

    im2 = ax2.imshow(crop_dem, extent=crop_extent, cmap=cmap_hypso, origin='upper')
    cbar2 = plt.colorbar(im2, ax=ax2, orientation='vertical', fraction=0.046, pad=0.03)
    cbar2.set_label('Elevación (m)', color='white', fontsize=11)
    cbar2.ax.tick_params(colors='white')

    ax2.plot(route_lons, route_lats, color='#ffffff', linewidth=2.8,
             path_effects=[pe.Stroke(linewidth=5.0, foreground='#ffaa00'), pe.Stroke(linewidth=7.0, foreground='black'), pe.Normal()],
             label=f'Ruta ({props["total_distance_m"]:.1f} m)')
    ax2.plot(route_lons[0], route_lats[0], 'o', color='#00ff66', markersize=9,
             markeredgecolor='black', markeredgewidth=1.5, label='Origen')
    ax2.plot(route_lons[-1], route_lats[-1], 's', color='#ff0055', markersize=9,
             markeredgecolor='black', markeredgewidth=1.5, label='Destino')

    ax2.set_title(title_detail, color='white', fontsize=13, pad=10)
    ax2.set_xlabel('Longitud (°E)', color='white')
    ax2.set_ylabel('Latitud (°N)', color='white')
    ax2.tick_params(colors='white')
    ax2.legend(loc='lower left', facecolor='#161b22', edgecolor='#30363d', labelcolor='white')

    # Distancias acumuladas
    distances_km = [0.0]
    mid_lat = (route_lats[0] + route_lats[-1]) / 2.0
    for i in range(1, len(coords)):
        d = np.hypot((route_lons[i] - route_lons[i-1]) * 59274.0 * np.cos(np.radians(mid_lat)),
                     (route_lats[i] - route_lats[i-1]) * 59274.0)
        distances_km.append(distances_km[-1] + d / 1000.0)

    # 3. Altimetric Profile
    ax3.set_facecolor('#141923')
    step = max(1, len(distances_km) // 500)
    ax3.scatter(distances_km[::step], route_elevs[::step], c=route_elevs[::step], cmap=cmap_hypso, s=12, zorder=3)
    ax3.plot(distances_km, route_elevs, color='white', linewidth=1.5, alpha=0.6, zorder=2)
    ax3.fill_between(distances_km, min(route_elevs) - 10, route_elevs, color='#00e5ff', alpha=0.08)

    ax3.set_title(f'Perfil Topográfico (+{props["cumulative_elevation_gain_m"]:.1f} m | Pendiente máx: {props["max_slope_deg"]:.1f}° | Duración: {props.get("total_duration_s", 0)/60:.1f} min)',
                  color='white', fontsize=11, pad=6)
    ax3.set_ylabel('Cota (m)', color='white')
    ax3.tick_params(colors='white')
    ax3.grid(True, linestyle='--', alpha=0.25, color='#455a75')

    # 4. Telemetría Ambiental (Radiación y Temperatura)
    ax4.set_facecolor('#141923')
    rad_color = '#00e5ff'
    ax4.plot(distances_km, route_rads, color=rad_color, linewidth=2.0, label='Tasa radiación (mSv/día)')
    ax4.set_xlabel('Distancia Recorrida (km)', color='white')
    ax4.set_ylabel('Dosis Radiación (mSv/día)', color=rad_color, fontsize=10)
    ax4.tick_params(axis='y', labelcolor=rad_color)
    ax4.tick_params(axis='x', colors='white')
    ax4.grid(True, linestyle='--', alpha=0.25, color='#455a75')

    if has_temp:
        ax4_temp = ax4.twinx()
        temp_color = '#ff9100'
        ax4_temp.plot(distances_km, route_temps, color=temp_color, linewidth=2.0, linestyle='-.', label='Temperatura (°C)')
        ax4_temp.set_ylabel('Temperatura Superficial (°C)', color=temp_color, fontsize=10)
        ax4_temp.tick_params(axis='y', labelcolor=temp_color)
        
        # Combinar leyendas
        lines_1, labels_1 = ax4.get_legend_handles_labels()
        lines_2, labels_2 = ax4_temp.get_legend_handles_labels()
        ax4.legend(lines_1 + lines_2, labels_1 + labels_2, loc='upper right',
                   facecolor='#161b22', edgecolor='#30363d', labelcolor='white')
    else:
        ax4.legend(loc='upper right', facecolor='#161b22', edgecolor='#30363d', labelcolor='white')

    dosis_total = props.get('total_radiation_dose_msv', 0.0)
    t_min = props.get('min_temp_c', min(route_temps) if route_temps else 0)
    t_max = props.get('max_temp_c', max(route_temps) if route_temps else 0)
    ax4.set_title(f'Telemetría Ambiental (Dosis Acumulada: {dosis_total:.4f} mSv | Rango T: {t_min:.1f} °C a {t_max:.1f} °C)',
                  color='white', fontsize=11, pad=6)

    plt.tight_layout()
    plt.savefig(output_png_path, dpi=180, facecolor=fig.get_facecolor(), edgecolor='none')
    plt.close()
    print('Generado exitosamente:', output_png_path)

if __name__ == '__main__':
    render_comprehensive_route(
        '/home/eneko/speisaps/pathfinder/outputs/route_rover_25km_10m.geojson',
        '/home/eneko/speisaps/pathfinder/outputs/route_rover_25km_10m_rendered.png',
        'Gale Crater — Gran Travesía Rover 25 km (Paso decamétrico 10 m)',
        'Detalle Topográfico del Flanco de Monte Sharp'
    )
    render_comprehensive_route(
        '/home/eneko/speisaps/pathfinder/outputs/route_10km_10m.geojson',
        '/home/eneko/speisaps/pathfinder/outputs/route_10km_10m_rendered.png',
        'Gale Crater — Travesía Astronauta EVA 10 km (Paso decamétrico 10 m)',
        'Detalle Topográfico de la Planicie de Aterrizaje'
    )
