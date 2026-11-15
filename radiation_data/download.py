import os
import re
import urllib.request

CARPETAS_URL = [
    "https://pds-ppi.igpp.ucla.edu/data/MSL-M-RAD-3-RDR-V1.0/DATA/SOL_00090_00179/",
    "https://pds-ppi.igpp.ucla.edu/data/MSL-M-RAD-3-RDR-V1.0/DATA/SOL_00180_00269/",
]
CARPETA_LOCAL = "datos_rad"
os.makedirs(CARPETA_LOCAL, exist_ok=True)

# Rango de días del año 2013: 1 = 1 de enero, 120 = 30 de abril
DOY_INICIO = 1
DOY_FIN = 120

enlaces_descarga = []
lbl_descargado = False

for base_url in CARPETAS_URL:
    html = urllib.request.urlopen(base_url).read().decode("utf-8", errors="ignore")

    # Guardar un único archivo .LBL para tener el diccionario de columnas
    if not lbl_descargado:
        lbls = re.findall(r'href="(RAD_RDR_2013_\d{3}_[^"]+\.LBL)"', html)
        if lbls:
            enlaces_descarga.append((base_url, lbls[0]))
            lbl_descargado = True

    # Buscar todos los .TXT de 2013 y filtrar por día del año (001 a 120)
    txts = re.findall(r'href="(RAD_RDR_2013_(\d{3})_[^"]+\.TXT)"', html)
    for nombre_archivo, doy_str in txts:
        doy = int(doy_str)
        if DOY_INICIO <= doy <= DOY_FIN:
            enlaces_descarga.append((base_url, nombre_archivo))

# Eliminar posibles duplicados manteniendo el orden
enlaces_descarga = list(dict.fromkeys(enlaces_descarga))
print(f"Archivos encontrados (1 ene - 30 abr 2013): {len(enlaces_descarga)}")

for i, (base_url, nombre) in enumerate(enlaces_descarga, start=1):
    destino = os.path.join(CARPETA_LOCAL, nombre)
    if not os.path.exists(destino):
        print(f"[{i}/{len(enlaces_descarga)}] Descargando {nombre}...")
        urllib.request.urlretrieve(base_url + nombre, destino)
    else:
        print(f"[{i}/{len(enlaces_descarga)}] Ya existe: {nombre}")

print("Descarga completada.")