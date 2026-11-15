# plot_svf_heatmap.py
import matplotlib.pyplot as plt
import numpy as np

# Leer metadatos del grid
with open("hirise_meta.txt", "r") as f:
    rows, cols, px_size = f.read().split()
    rows, cols = int(rows), int(cols)

print(f"Cargando LUT de {rows} x {cols} para generar el heatmap...")

# Cargar LUT y DEM con memmap (gastando cero RAM)
lut = np.memmap("gale_shielding_lut.bin", dtype=np.float32, mode="r", shape=(rows, cols))
dem = np.memmap("hirise_dem.bin", dtype=np.float32, mode="r", shape=(rows, cols))

# Submuestrear un poco si es muy grande para que renderice al instante en el PC
step = 2 if rows > 10000 else 1
lut_view = np.array(lut[::step, ::step], copy=True)
dem_view = np.array(dem[::step, ::step], copy=True)

# Poner en transparente (NaN) los bordes fuera del franjeado HiRISE
lut_view[np.isnan(dem_view) | (np.abs(dem_view) > 1e5)] = np.nan

plt.figure(figsize=(12, 10), dpi=300)
# 'magma' o 'inferno' dan un aspecto muy "NASA/HiRISE"
im = plt.imshow(lut_view, cmap="inferno", vmin=0.4, vmax=1.0)

cbar = plt.colorbar(im, fraction=0.03, pad=0.04)
cbar.set_label("Topographic Radiation Factor (0.15 + 0.85 * SVF_eff)", color="white", fontsize=12)
cbar.ax.yaxis.set_tick_params(color="white")
plt.setp(cbar.ax.get_yticklabels(), color="white")

plt.title(
    "Gale Crater - Mars Curiosity Rover Topographic Shielding Heatmap",
    color="white",
    fontsize=14,
    pad=15,
)
plt.axis("off")

# Fondo oscuro corporativo para que resalte el mapa de calor
plt.gca().set_facecolor("#0b0f19")
plt.gcf().patch.set_facecolor("#0b0f19")

plt.tight_layout()
plt.savefig("gale_radiation_shielding_heatmap.png", facecolor=plt.gcf().get_facecolor(), edgecolor="none")
print("¡Heatmap guardado como 'gale_radiation_shielding_heatmap.png'!")
plt.show()