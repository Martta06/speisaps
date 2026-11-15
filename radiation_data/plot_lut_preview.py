# plot_lut_preview.py
import numpy as np
import matplotlib.pyplot as plt

with open("hirise_meta.txt", "r") as f:
    rows, cols, _ = f.read().split()
    rows, cols = int(rows), int(cols)

# Memory-map the 5.77 GB binary without loading it all into RAM
lut_mmap = np.memmap("gale_shielding_lut.bin", dtype=np.float32, mode="r", shape=(rows, cols))
dem_mmap = np.memmap("hirise_dem.bin", dtype=np.float32, mode="r", shape=(rows, cols))

step = 20  # Downsample 20x (~2000 x 1800 px) for instant plotting
lut_sub = np.array(lut_mmap[::step, ::step], copy=True)
dem_sub = np.array(dem_mmap[::step, ::step], copy=True)
lut_sub[np.isnan(dem_sub)] = np.nan  # Transparent outside HiRISE swath

plt.figure(figsize=(10, 9), dpi=200)
im = plt.imshow(lut_sub, cmap="magma", vmin=0.55, vmax=1.0)
plt.colorbar(im, label="Topographic Radiation Factor (0.15 + 0.85 * SVF)")
plt.title("Gale Crater - Topographic Radiation Shielding LUT (2,500 m horizon)")
plt.axis("off")
plt.tight_layout()
plt.savefig("gale_shielding_map_preview.png", transparent=True)
print("Saved gale_shielding_map_preview.png")