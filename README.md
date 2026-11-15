# Gale et al. | Local quickstart

Gale et al. plans rover and on-foot traverses across Gale Crater using terrain, temperature, and surface radiation data. The map runs in Vite. A local Flask API calls the native route planner and environmental model.

## Prerequisites

Use Windows x64, 64-bit Python 3, and Node.js with npm. Run the commands below in PowerShell, starting from the repository root (the directory containing `server.py`).

Before starting, check that the following runtime assets are present:

| Path relative to the repository root | Purpose |
| --- | --- |
| `pathfinder/martian_pathfinder.exe` | Windows route planner |
| `Gale_DEM_SMG_1m.tif` | Gale Crater elevation model, approximately 5.4 GB |
| `radiation_data/mars_env.dll` | Native temperature and radiation model |
| `radiation_data/` | Model inputs, including `hirise_dem.bin`, `hirise_meta.txt`, `gale_shielding_lut.bin`, and `rad_baseline_24h.bin` |
| `gale-map/public/data/` | Basemap images, including `gale-terrain.png` and `gale-colour.png` |

### Downloading `Gale_DEM_SMG_1m.tif` (5.4 GB — Excluded from GitHub)

Because GitHub enforces a strict file size limit, the 5.4 GB NASA/USGS HiRISE-CTX-HRSC merged Digital Elevation Model (`Gale_DEM_SMG_1m.tif`, $36\,000 \times 40\,100\text{ px}$ at $1\text{ m/px}$) is excluded via `.gitignore`. To install it:

1. **Download from the official USGS Astrogeology Science Center (NASA PDS):**
   - **Direct GeoTIFF download (`5.4 GB`):** <https://astropedia.astrogeology.usgs.gov/download/Mars/MSL/Gale_otti_blend/Gale_DEM_SMG_1m.tif>
   - Or via terminal (`curl`):
     ```powershell
     curl.exe -L -o Gale_DEM_SMG_1m.tif https://astropedia.astrogeology.usgs.gov/download/Mars/MSL/Gale_otti_blend/Gale_DEM_SMG_1m.tif
     ```
2. **Place the file** directly in the repository root (`speisaps/Gale_DEM_SMG_1m.tif`)
3. *(Optional)* The precomputed $10\text{ m/px}$ downsampled binary grids (`radiation_data/hirise_dem.bin`, `gale_shielding_lut.bin`, and `rad_baseline_24h.bin`) are already included in `radiation_data/`. To regenerate them from scratch from the `.tif`, run `python radiation_data/prepare_rad_inputs.py`.

## Start the application

**Terminal 1 — API**

```powershell
python -m venv .venv
if (Test-Path .\requirements.txt) {
    .\.venv\Scripts\python.exe -m pip install -r .\requirements.txt
} else {
    .\.venv\Scripts\python.exe -m pip install Flask
}
.\.venv\Scripts\python.exe .\server.py
```

**Terminal 2 — map**

```powershell
cd gale-map
npm.cmd install
npm.cmd run dev
```

Open the **Local** URL printed by Vite, usually <http://localhost:5173>. Keep both terminals running. On subsequent launches, only the last command in each block is needed, from its respective directory.

## Try a traverse

1. In **Route planner**, enter the following coordinates in decimal degrees.

   | Point | Longitude | Latitude |
   | --- | ---: | ---: |
   | A · Origin | 137.4250 | -4.5800 |
   | B · Destination | 137.4270 | -4.5820 |

2. Select **Rover** and click **Find route**. The calculated path should appear between A and B.
3. Under **Environment layer**, select temperature and change **Local solar time**. Release the slider to update the overlay. Switch to radiation to inspect the modeled surface dose rate in µSv/h.
4. To preview the crew alerts, use **Simulate dust storm** or **Simulate solar event** in the **Event simulator** panel.

## Troubleshooting

- **Map loads but route requests return HTTP 502.** Open <http://127.0.0.1:8000/api/health>. If it is unreachable, check Terminal 1. If it responds, verify that the Vite proxy forwards `/api` to `http://127.0.0.1:8000`.
- **Route calculation fails.** Check `bin_exists` and `dem_exists` in the health response. Both should be `true`. If the API returns HTTP 422, try another destination or travel mode.
- **Routes work but environmental layers are unavailable.** Check Terminal 1 for the model initialization error. Verify the DLL architecture and model input files, then restart Flask.

Press **Ctrl+C** in each terminal to stop the application.
