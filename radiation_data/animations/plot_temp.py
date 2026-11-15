# plot_temp.py
import os
import matplotlib.pyplot as plt
import matplotlib.colors as mcolors
from matplotlib.collections import LineCollection
import numpy as np


def get_mars_temperature_py(
    t_lmst,
    elevation_m,
    t_min_k=190.0,
    t_max_k=280.0,
    base_elevation_m=-4500.0,
):
    """Exact Python replica of the C thermal model in mars_env.c"""
    t = np.asarray(t_lmst, dtype=float) % 24.0
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

    # Daytime cycle (asymmetric lagged sinusoid, peak at 14:30 LMST)
    normalized_cycle[day_mask] = np.sin(
        np.pi * (t[day_mask] - t_sunrise) / denom
    )

    # Nighttime cycle (exponential thermal inertia radiative cooling)
    dt_night = (t[night_mask] - t_sunset) % 24.0
    night_length = 24.0 - day_length
    exp_b = np.exp(-nocturnal_decay)
    normalized_cycle[night_mask] = (
        f_sunset
        * (np.exp(-nocturnal_decay * dt_night / night_length) - exp_b)
        / (1.0 - exp_b)
    )

    # Dry adiabatic lapse rate correction (-2.5 K/km)
    lapse_correction_k = -0.0025 * (elevation_m - base_elevation_m)
    temp_k = t_min_k + (t_max_k - t_min_k) * normalized_cycle + lapse_correction_k
    temp_c = temp_k - 273.15
    return temp_k, temp_c


# Extended 24-stop spectrum (Ice White -> Lavender -> Purple -> Blue -> Cyan -> Green -> Amber -> Red)
SPECTRUM_24 = [
    "#f8fafc", "#e9d5ff", "#c084fc", "#9333ea",
    "#6d28d9", "#4f46e5", "#2563eb", "#0284c7",
    "#06b6d4", "#0d9488", "#10b981", "#22c55e",
    "#65a30d", "#a3e635", "#facc15", "#eab308",
    "#f59e0b", "#f97316", "#ea580c", "#ef4444",
    "#dc2626", "#b91c1c", "#991b1b", "#7f1d1d",
]
cmap = mcolors.LinearSegmentedColormap.from_list("mars_temp_spectrum", SPECTRUM_24, N=512)
norm = mcolors.Normalize(vmin=-95.0, vmax=7.0)

hours = np.linspace(0, 24, 600)

# Hypsometric contour levels across Gale Crater
contour_profiles = [
    ("Crater Floor Landing Site (-4,500 m)", -4500.0, "-", 3.0),
    ("Vera Rubin Ridge (-3,800 m)", -3800.0, "--", 2.0),
    ("Aeolis Mons Flank (-2,200 m)", -2200.0, "-.", 1.8),
    ("Mount Sharp Summit (+500 m)", 500.0, ":", 2.0),
]

plt.style.use("dark_background")
fig, ax = plt.subplots(figsize=(12, 6.2), dpi=300)
fig.patch.set_facecolor("#0b0f19")
ax.set_facecolor("#0b0f19")

# Shade Martian night hours
ax.axvspan(0, 6, color="#172033", alpha=0.55, lw=0)
ax.axvspan(18, 24, color="#172033", alpha=0.55, lw=0, label="Martian Night (Radiative Cooling)")

# Shade EVA Suit Thermal Limit (-80 °C) and Optimal Operating Band (-20 °C to 0 °C)
ax.axhspan(-20, 0, color="#10b981", alpha=0.08, lw=0, label="Optimal EVA Thermal Window (-20 °C to 0 °C)")
ax.axhline(-80.0, color="#c084fc", linestyle="--", lw=1.2, alpha=0.75, label="EVA Suit Minimum Limit (-80 °C)")

# Plot multicolor temperature gradient curve for the Crater Floor (-4,500 m)
_, temp_floor = get_mars_temperature_py(hours, elevation_m=-4500.0)
_, temp_summit = get_mars_temperature_py(hours, elevation_m=500.0)

# Soft gradient band between Crater Floor and Mount Sharp Summit
ax.fill_between(
    hours,
    temp_summit,
    temp_floor,
    color="#38bdf8",
    alpha=0.10,
    label="Gale Crater Elevation Thermal Span (-4,500 m to +500 m)",
)

points = np.array([hours, temp_floor]).T.reshape(-1, 1, 2)
segments = np.concatenate([points[:-1], points[1:]], axis=1)
lc = LineCollection(segments, cmap=cmap, norm=norm, linewidth=3.4, zorder=5)
lc.set_array(temp_floor[:-1])
ax.add_collection(lc)

# Plot reference hypsometric elevation contours
contour_colors = ["#ef4444", "#f59e0b", "#06b6d4", "#c084fc"]
for (label, elev_m, ls, lw), col in zip(contour_profiles, contour_colors):
    _, t_c = get_mars_temperature_py(hours, elevation_m=elev_m)
    ax.plot(hours, t_c, linestyle=ls, linewidth=lw if elev_m != -4500.0 else 0.0, color=col, alpha=0.9, label=label)

# Annotate diurnal extrema on the Crater Floor curve
idx_min = int(np.argmin(temp_floor))
idx_max = int(np.argmax(temp_floor))
ax.scatter([hours[idx_min], hours[idx_max]], [temp_floor[idx_min], temp_floor[idx_max]],
           color=["#e9d5ff", "#ef4444"], edgecolors="white", s=55, zorder=6)
ax.annotate(
    f"Dawn Minimum: {temp_floor[idx_min]:.1f} °C (06:00 LMST)",
    xy=(hours[idx_min], temp_floor[idx_min]),
    xytext=(hours[idx_min] + 0.8, temp_floor[idx_min] - 7.5),
    color="#e9d5ff",
    fontsize=9.5,
    fontweight="bold",
    arrowprops=dict(arrowstyle="->", color="#e9d5ff", lw=1.2),
)
ax.annotate(
    f"Afternoon Thermal Peak: +{temp_floor[idx_max]:.1f} °C (14:30 LMST)",
    xy=(hours[idx_max], temp_floor[idx_max]),
    xytext=(hours[idx_max] + 1.2, temp_floor[idx_max] + 5.5),
    color="#fca5a5",
    fontsize=9.5,
    fontweight="bold",
    arrowprops=dict(arrowstyle="->", color="#fca5a5", lw=1.2),
)

# Add graduated colorbar on the right matching the frontend (-95 °C to +7 °C)
sm = plt.cm.ScalarMappable(cmap=cmap, norm=norm)
sm.set_array([])
cbar = fig.colorbar(sm, ax=ax, pad=0.025, fraction=0.03)
cbar.set_ticks([-95, -70, -45, -20, 7])
cbar.set_ticklabels(["-95 °C", "-70 °C", "-45 °C", "-20 °C", "+7 °C"])
cbar.set_label("Absolute Temperature Scale (°C)", color="#f8fafc", fontsize=10.5, fontweight="bold")
cbar.ax.yaxis.set_tick_params(color="#cbd5e1", labelsize=9.5)
cbar.outline.set_edgecolor("#334155")
plt.setp(cbar.ax.get_yticklabels(), color="#e2e8f0", family="monospace")

# Styling and axes in English
ax.set_title(
    "Gale Crater · Diurnal Surface Temperature & Hypsometric Elevation Contours",
    fontsize=14,
    pad=16,
    color="#f8fafc",
    fontweight="bold",
)
ax.set_xlabel("Local Mean Solar Time (LMST · Martian Sol Hours)", fontsize=11, color="#e2e8f0", labelpad=8)
ax.set_ylabel("Atmospheric Surface Temperature (°C)", fontsize=11, color="#e2e8f0")
ax.set_xlim(0, 24)
ax.set_ylim(-102, 20)
ax.set_xticks([0, 3, 6, 9, 12, 15, 18, 21, 24])
ax.set_xticklabels([
    "00:00",
    "03:00",
    "06:00\n(Sunrise)",
    "09:00",
    "12:00\n(Noon)",
    "15:00\n(Peak Lag)",
    "18:00\n(Sunset)",
    "21:00",
    "24:00",
], color="#cbd5e1", fontsize=9.5)
ax.tick_params(axis="y", colors="#cbd5e1", labelsize=9.5)
ax.grid(True, color="#334155", linestyle="--", alpha=0.45)
for spine in ax.spines.values():
    spine.set_edgecolor("#334155")

ax.legend(
    facecolor="#111827",
    edgecolor="#334155",
    labelcolor="#e2e8f0",
    loc="upper left",
    fontsize=8.3,
    ncol=1,
    framealpha=0.92,
)

plt.tight_layout()
out_file = os.path.join(os.path.dirname(__file__) or ".", "mars_temperature_diurnal_curve.png")
plt.savefig(out_file, facecolor=fig.get_facecolor(), edgecolor="none", dpi=300)
print(f"Saved temperature chart to '{out_file}'!")
