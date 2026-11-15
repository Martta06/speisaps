# animate_temp.py
import os
import matplotlib.animation as animation
import matplotlib.pyplot as plt
import numpy as np

# Asegurar que se ejecuta en la carpeta correcta
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
os.chdir(SCRIPT_DIR)


def get_mars_temperature_py(
    t_lmst,
    elevation_m,
    t_min_k=192.0,
    t_max_k=260.0,
    base_elevation_m=-4500.0,
):
    t = t_lmst % 24.0
    t_sunrise = 6.0
    t_sunset = 18.0
    thermal_lag_h = 2.5
    nocturnal_decay = 2.2

    day_length = t_sunset - t_sunrise
    denom = day_length + 2.0 * thermal_lag_h
    f_sunset = np.sin(np.pi * day_length / denom)

    normalized_cycle = np.zeros_like(t)
    day_mask = (t >= t_sunrise) & (t <= t_sunset)
    night_mask = ~day_mask

    normalized_cycle[day_mask] = np.sin(
        np.pi * (t[day_mask] - t_sunrise) / denom
    )
    dt_night = (t[night_mask] - t_sunset) % 24.0
    night_length = 24.0 - day_length
    exp_b = np.exp(-nocturnal_decay)
    normalized_cycle[night_mask] = (
        f_sunset
        * (np.exp(-nocturnal_decay * dt_night / night_length) - exp_b)
        / (1.0 - exp_b)
    )

    lapse_correction_k = -0.002 * (elevation_m - base_elevation_m)
    temp_k = t_min_k + (t_max_k - t_min_k) * normalized_cycle + lapse_correction_k
    return temp_k - 273.15  # Grados Celsius


# Datos para las 24 horas
hours = np.linspace(0, 24, 100)
hours_int = list(range(25))
temps_crater = get_mars_temperature_py(hours, elevation_m=-4500.0)
temps_crater_int = get_mars_temperature_py(
    np.array(hours_int, dtype=float), elevation_m=-4500.0
)

fig, ax = plt.subplots(figsize=(10, 5), facecolor="#0b0f19")
ax.set_facecolor("#111827")

ax.axvspan(0, 6, color="#1e293b", alpha=0.5, lw=0)
ax.axvspan(18, 24, color="#1e293b", alpha=0.5, lw=0)

ax.set_xlim(0, 24)
ax.set_ylim(-85, -10)
ax.set_title(
    "Evolución Térmica Diurna en Gale Crater (24h)",
    color="white",
    fontsize=14,
    pad=12,
)
ax.set_xlabel("Hora Local Solar Media (LMST)", color="white", fontsize=11)
ax.set_ylabel("Temperatura (°C)", color="white", fontsize=11)
ax.tick_params(colors="white")
ax.grid(True, color="#374151", linestyle="--", alpha=0.5)

(line,) = ax.plot([], [], color="#38bdf8", lw=3)
(point,) = ax.plot([], [], color="#f43f5e", marker="o", markersize=10, zorder=5)
text_info = ax.text(
    0.05,
    0.80,
    "",
    transform=ax.transAxes,
    color="white",
    fontsize=13,
    fontweight="bold",
)


def init():
    line.set_data([], [])
    point.set_data([], [])
    text_info.set_text("")
    return line, point, text_info


def animate(i):
    h_idx = i % len(hours_int)
    current_t = hours_int[h_idx]

    # Dibujar curva hasta el punto actual
    mask = hours <= current_t
    line.set_data(hours[mask], temps_crater[mask])
    point.set_data([current_t], [temps_crater_int[h_idx]])

    phase = (
        "Noche / Madrugada"
        if (current_t < 6 or current_t > 18)
        else "Día Marciano"
    )
    text_info.set_text(
        f"LMST: {current_t:02d}:00\nTemp: {temps_crater_int[h_idx]:.1f} °C\n[{phase}]"
    )
    return line, point, text_info


ani = animation.FuncAnimation(
    fig, animate, init_func=init, frames=25, interval=300, blit=True
)

ani.save("mars_temperature_diurnal.gif", writer="pillow", fps=4)
print("¡GIF de la temperatura guardado como 'mars_temperature_diurnal.gif'!")