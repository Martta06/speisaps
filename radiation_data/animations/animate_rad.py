# animate_rad.py
import os
import matplotlib.animation as animation
import matplotlib.pyplot as plt
import numpy as np

# Asegurar que busca los binarios en la misma carpeta donde está este script
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
os.chdir(SCRIPT_DIR)

# Leer directamente el binario generado por prepare_rad_inputs.py
# Sube un nivel desde 'animations' para buscar el .bin en 'radiation_data'
bin_path = os.path.join(os.path.dirname(__file__), "..", "rad_baseline_24h.bin")
doses = np.fromfile(bin_path, dtype=np.float32)
hours = list(range(24))

fig, ax = plt.subplots(figsize=(10, 5), facecolor="#0b0f19")
ax.set_facecolor("#111827")

# Estética científica espacial
ax.set_xlim(0, 23)
ax.set_ylim(25.0, 29.0)
ax.set_title(
    "MSL RAD Diurnal Radiation Baseline (Gale Crater)",
    color="white",
    fontsize=14,
    pad=12,
)
ax.set_xlabel("Local Mean Solar Time (LMST)", color="white", fontsize=11)
ax.set_ylabel(r"Dose Rate ($\mu$Sv/h)", color="white", fontsize=11)
ax.tick_params(colors="white")
ax.grid(True, color="#374151", linestyle="--", alpha=0.5)

(line,) = ax.plot([], [], color="#38bdf8", lw=3, marker="o", markersize=6)
(point,) = ax.plot([], [], color="#f43f5e", marker="o", markersize=10, zorder=5)
text_time = ax.text(
    0.05,
    0.85,
    "",
    transform=ax.transAxes,
    color="white",
    fontsize=14,
    fontweight="bold",
)


def init():
    line.set_data([], [])
    point.set_data([], [])
    text_time.set_text("")
    return line, point, text_time


def animate(i):
    h_idx = i % 24
    current_hours = hours[: h_idx + 1]
    current_doses = doses[: h_idx + 1]

    line.set_data(current_hours, current_doses)
    point.set_data([hours[h_idx]], [doses[h_idx]])
    text_time.set_text(
        f"LMST: {hours[h_idx]:02d}:00\nDose: {doses[h_idx]:.2f} "
        r"$\mu$Sv/h"
    )
    return line, point, text_time


ani = animation.FuncAnimation(
    fig, animate, init_func=init, frames=24, interval=250, blit=True
)

ani.save("mars_radiation_diurnal.gif", writer="pillow", fps=4)
print("¡Animación de 24h guardada como 'mars_radiation_diurnal.gif'!")