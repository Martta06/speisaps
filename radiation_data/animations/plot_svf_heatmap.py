# plot_svf_heatmap.py
import os
import matplotlib.pyplot as plt
import matplotlib.colors as mcolors
import numpy as np

# Locate binary files whether run from radiation_data/ or radiation_data/animations/
base_dir = "." if os.path.exists("hirise_meta.txt") else ".."
meta_path = os.path.join(base_dir, "hirise_meta.txt")
lut_path = os.path.join(base_dir, "gale_shielding_lut.bin")
dem_path = os.path.join(base_dir, "hirise_dem.bin")

with open(meta_path, "r") as f:
    rows, cols, px_size = f.read().split()
    rows, cols = int(rows), int(cols)
    px_size = float(px_size)

print(f"Loading {rows} x {cols} Topographic Shielding LUT & DEM...")

lut = np.memmap(lut_path, dtype=np.float32, mode="r", shape=(rows, cols))
dem = np.memmap(dem_path, dtype=np.float32, mode="r", shape=(rows, cols))

step = 2 if rows > 10000 else 1
lut_view = np.array(lut[::step, ::step], copy=True)
dem_view = np.array(dem[::step, ::step], copy=True)

invalid_mask = np.isnan(dem_view) | (np.abs(dem_view) > 1e5) | (lut_view <= 0.0)
lut_view[invalid_mask] = np.nan
dem_view[invalid_mask] = np.nan

# 24-stop extended spectrum: Ice White -> Lavender -> Deep Purple -> Indigo -> Blue -> Cyan -> Green -> Amber -> Crimson
SPECTRUM_24 = [
    "#f8fafc", "#e9d5ff", "#c084fc", "#9333ea",
    "#6d28d9", "#4f46e5", "#2563eb", "#0284c7",
    "#06b6d4", "#0d9488", "#10b981", "#22c55e",
    "#65a30d", "#a3e635", "#facc15", "#eab308",
    "#f59e0b", "#f97316", "#ea580c", "#ef4444",
    "#dc2626", "#b91c1c", "#991b1b", "#7f1d1d",
]
cmap = mcolors.LinearSegmentedColormap.from_list("mars_spectrum", SPECTRUM_24, N=512)
cmap.set_bad(color="#0b0f19")

# Non-linear normalization: expands the high-density interval [0.90, 1.00] where most
# topographic horizon variations occur, while preserving deep canyon values down to 0.72.
vmin, vmax = 0.75, 1.00


def forward_norm(x):
    x_clamped = np.clip((x - vmin) / (vmax - vmin), 0.0, 1.0)
    return np.power(x_clamped, 6.5)


def inverse_norm(y):
    y_clamped = np.clip(y, 0.0, 1.0)
    return vmin + (vmax - vmin) * np.power(y_clamped, 1.0 / 6.5)


norm = mcolors.FuncNorm((forward_norm, inverse_norm), vmin=vmin, vmax=vmax)

# Compute subtle 3D topographic hillshade from the DEM to enhance crater rims and scarps
gy, gx = np.gradient(dem_view, px_size * step)
slope = np.pi / 2.0 - np.arctan(np.hypot(gx, gy))
aspect = np.arctan2(-gx, gy)
az = np.deg2rad(315.0)
alt = np.deg2rad(38.0)
hillshade = np.sin(alt) * np.sin(slope) + np.cos(alt) * np.cos(slope) * np.cos(az - aspect)
hillshade = np.clip((hillshade - 0.2) / 0.8, 0.0, 1.0)

# Blend non-linear colormap with 3D hillshading
rgba = cmap(norm(lut_view))
blend_factor = 0.72 + 0.28 * hillshade[..., np.newaxis]
rgba[..., :3] = np.clip(rgba[..., :3] * blend_factor, 0.0, 1.0)
rgba[invalid_mask] = [11 / 255.0, 15 / 255.0, 25 / 255.0, 1.0]

plt.style.use("dark_background")
fig, ax = plt.subplots(figsize=(12.5, 10.5), dpi=300)
fig.patch.set_facecolor("#0b0f19")
ax.set_facecolor("#0b0f19")

# Plot blended RGB image and attach scalar mappable for the graduated colorbar
ax.imshow(rgba, interpolation="bilinear")
sm = plt.cm.ScalarMappable(cmap=cmap, norm=norm)
sm.set_array([])

cbar = fig.colorbar(sm, ax=ax, fraction=0.032, pad=0.035)
ticks = [0.75, 0.90, 0.95, 0.98, 0.992, 1.00]
cbar.set_ticks(ticks)
cbar.set_ticklabels([
    "0.75  (Deep Canyon Shelter · ~21 µSv/h)",
    "0.90  (Steep Crater Wall · ~24 µSv/h)",
    "0.95  (Scarp / Valley Floor · ~25.5 µSv/h)",
    "0.98  (Moderate Relief · ~26.5 µSv/h)",
    "0.992 (Low Undulation · ~27.0 µSv/h)",
    "1.00  (Exposed Open Plain · ~27.5 µSv/h)",
])
cbar.set_label(
    "Topographic Radiation Exposure Factor  [0.15 + 0.85 × SVF_eff]",
    color="#f8fafc",
    fontsize=11.5,
    fontweight="bold",
    labelpad=12,
)
cbar.ax.yaxis.set_tick_params(color="#cbd5e1", labelsize=9.5)
cbar.outline.set_edgecolor("#334155")
plt.setp(cbar.ax.get_yticklabels(), color="#e2e8f0", family="monospace")

ax.set_title(
    "Gale Crater · Topographic Radiation Shielding & Sky View Factor (SVF)",
    color="#f8fafc",
    fontsize=14.5,
    fontweight="bold",
    pad=18,
)
ax.text(
    0.5,
    1.006,
    "Non-Linear Hypsometric Scale · 32-Azimuth Horizon Ray-Casting over HiRISE DEM (Gale Crater / Aeolis Mons)",
    transform=ax.transAxes,
    ha="center",
    va="bottom",
    color="#94a3b8",
    fontsize=10,
)
ax.axis("off")

plt.tight_layout()
out_file = os.path.join(os.path.dirname(__file__) or ".", "gale_radiation_shielding_heatmap.png")
plt.savefig(out_file, facecolor=fig.get_facecolor(), edgecolor="none", dpi=300)
print(f"Saved heatmap to '{out_file}'!")
