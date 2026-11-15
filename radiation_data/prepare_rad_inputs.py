# prepare_rad_inputs.py
import glob
import math
import os
import re
import numpy as np
import rasterio

MARS_RADIUS_M = 3_396_190.0
GALE_QUALITY_FACTOR = 3.05  # Hassler et al. (2014), Science: uGy/h -> uSv/h


def export_hirise_to_binary(
    tif_path: str,
    downsample_factor: int = 10,
) -> None:
    """
    Exports the HiRISE GeoTIFF to a downsampled float32 binary grid (10 m/px)
    and metadata file so C can compute the 2,500 m LUT in ~3 seconds.
    """
    with rasterio.open(tif_path) as src:
        out_h = src.height // downsample_factor
        out_w = src.width // downsample_factor
        dem = src.read(
            1,
            out_shape=(out_h, out_w),
            resampling=rasterio.enums.Resampling.average,
        ).astype(np.float32)

        nodata = src.nodata
        raw_px = abs(float(src.transform.a)) * downsample_factor
        px_size_m = raw_px * (math.pi / 180.0) * MARS_RADIUS_M if raw_px < 0.01 else raw_px

    if nodata is not None and not math.isnan(nodata):
        dem[np.isclose(dem, nodata, rtol=1e-5)] = np.nan
    dem[np.abs(dem) > 1e5] = np.nan

    dem.tofile("hirise_dem.bin")
    with open("hirise_meta.txt", "w", encoding="utf-8") as f:
        f.write(f"{dem.shape[0]} {dem.shape[1]} {px_size_m:.6f}\n")

    print(f"Exported hirise_dem.bin ({dem.shape[0]}x{dem.shape[1]}, px={px_size_m:.2f} m)")


def export_rad_baseline_to_binary(
    data_dir: str,
    open_sky_shielding_factor: float = 0.993, #Almost one; we take these dates because of the rover's position (almost flat terrain) at them.
) -> None:
    """
    Parses MSL RAD RDR sectioned .TXT files ([OBSERVATION] + [DOSIMETRY_TOTAL_DOSE_E]),
    converts absorbed dose (uGy/h) to dose equivalent (uSv/h), and exports the
    24-hour Martian diurnal baseline for C.
    """
    txt_files = sorted(glob.glob(os.path.join(data_dir, "*.TXT")))
    if not txt_files:
        raise FileNotFoundError(f"No .TXT files found inside '{data_dir}'.")

    obs_time_re = re.compile(r'^START_OBS_MARS="\d+\s+(\d{2}):\d{2}:\d{2}"')
    dose_e_header_re = re.compile(r"^\[DOSIMETRY_TOTAL_DOSE_E:\s*\d+\]")

    buckets: dict[int, list[float]] = {h: [] for h in range(24)}

    for txt_path in txt_files:
        current_hour: int | None = None
        reading_dose_e = False

        with open(txt_path, "r", encoding="utf-8", errors="ignore") as f:
            for raw_line in f:
                line = raw_line.strip()
                if not line:
                    continue

                # 1. Track current observation Martian hour
                m_time = obs_time_re.match(line)
                if m_time:
                    current_hour = int(m_time.group(1)) % 24
                    reading_dose_e = False
                    continue

                # 2. Detect tissue-equivalent dose block [DOSIMETRY_TOTAL_DOSE_E: XX]
                if dose_e_header_re.match(line):
                    reading_dose_e = True
                    continue

                # 3. Read the numeric value right below the header (in uGy/h)
                if reading_dose_e:
                    reading_dose_e = False
                    if current_hour is None:
                        continue
                    try:
                        dose_uGy_h = float(line)
                    except ValueError:
                        continue

                    # Filter out negative sensor flags (-2.0) and extreme solar spikes
                    if 3.0 <= dose_uGy_h <= 40.0:
                        dose_uSv_h = (dose_uGy_h * GALE_QUALITY_FACTOR) / open_sky_shielding_factor
                        buckets[current_hour].append(dose_uSv_h)

    curve_24h = np.array(
        [np.mean(buckets[h]) if buckets[h] else 26.7 for h in range(24)],
        dtype=np.float32,
    )
    curve_24h.tofile("rad_baseline_24h.bin")

    total_samples = sum(len(v) for v in buckets.values())
    print(f"Processed {len(txt_files)} RAD files ({total_samples} valid observations).")
    for h in range(24):
        print(f"  LMST {h:02d}:00 -> {curve_24h[h]:.3f} uSv/h (n={len(buckets[h])})")
    print("Exported rad_baseline_24h.bin successfully.")


if __name__ == "__main__":
    SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
    os.chdir(SCRIPT_DIR)

    TIF_PATH = r"C:\Users\ilac8\Desktop\Uni\NASA\geotiff\Gale_DEM_SMG_1m.tif"
    RAD_DIR = r"C:\Users\ilac8\Desktop\Uni\NASA\datos_rad"

    export_hirise_to_binary(TIF_PATH, downsample_factor=10)
    export_rad_baseline_to_binary(RAD_DIR)