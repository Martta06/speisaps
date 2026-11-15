# Gale et al. | Local quickstart

Gale et al. plans rover and on-foot traverses across Gale Crater using terrain, temperature, and surface radiation data. The map runs in Vite. A local Flask API calls the native route planner and environmental model.

## Prerequisites

Use Windows x64, 64-bit Python 3, and Node.js (which includes `npm`). All required precompiled binaries (`martian_pathfinder.exe`, `mars_env.dll`), basemap textures, and 10 m/px elevation/radiation grids (`radiation_data/*.bin`) are already included in the repository.

If **Node.js (`npm`)** or **Python 3** are not installed on the machine, install them from PowerShell (or from <https://nodejs.org/> and <https://www.python.org/>), then **close and reopen the terminal**:

```powershell
winget install OpenJS.NodeJS.LTS
winget install Python.Python.3.12
```

### Optional: Full-Resolution 1 m/px GeoTIFF (`5.4 GB`)

**You do NOT need to download the 5.4 GB `.tif` to run the application.** Both `server.py` and `martian_pathfinder.exe` automatically load `radiation_data/hirise_dem.bin` ($4\,010 \times 3\,600$ float32 elevation grid at $10\text{ m/px}$, $55\text{ MB}$), which is already included in this repository.

If you want to install the full-resolution $1\text{ m/px}$ NASA/JPL HiRISE-CTX-HRSC merged GeoTIFF (`Gale_DEM_SMG_1m.tif`, `5.4 GB`, excluded from GitHub due to file size limits):

1. **Download `Gale_DEM_SMG_1m.tif` from NASA Mars Trek:**
   - **Direct GeoTIFF Mirror (`5.4 GB`):** <https://planetarymaps.usgs.gov/mosaic/Mars/MSL/MSL_Gale_DEM_Mosaic_1m_v3.tif>
   - **NASA Mars Trek Portal:** <https://trek.nasa.gov/mars/> *(Data $\rightarrow$ Search `"Gale_DEM_SMG_1m"` / `"Gale Crater Merged DEM 1m"` $\rightarrow$ Download full GeoTIFF)*
2. **Place the file** in the repository root as `Gale_DEM_SMG_1m.tif`. `server.py` will detect it automatically on startup.

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
$env:PATH = "C:\Program Files\nodejs;" + $env:PATH
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
