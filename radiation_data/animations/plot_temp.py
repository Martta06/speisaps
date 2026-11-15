# plot_temperature_diurnal.py
import matplotlib.pyplot as plt
import numpy as np


def get_mars_temperature_py(
    t_lmst,
    elevation_m,
    t_min_k=192.0,
    t_max_k=260.0,
    base_elevation_m=-4500.0,
):
    """Réplica exacta en Python del modelo térmico implementado en mars_env.c"""
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

    # Ciclo diurno (senoidal adaptado)
    normalized_cycle[day_mask] = np.sin(
        np.pi * (t[day_mask] - t_sunrise) / denom
    )

    # Ciclo nocturno (decaída exponencial con inercia térmica)
    dt_night = (t[night_mask] - t_sunset) % 24.0
    night_length = 24.0 - day_length
    exp_b = np.exp(-nocturnal_decay)
    normalized_cycle[night_mask] = (
        f_sunset
        * (np.exp(-nocturnal_decay * dt_night / night_length) - exp_b)
        / (1.0 - exp_b)
    )

    # Corrección de temperatura por altitud (lapse rate)
    lapse_correction_k = -0.002 * (elevation_m - base_elevation_m)
    temp_k = t_min_k + (t_max_k - t_min_k) * normalized_cycle + lapse_correction_k
    temp_c = temp_k - 273.15
    return temp_k, temp_c


# Generar resolución fina para las 24 horas del Sol
hours = np.linspace(0, 24, 300)

# Simular dos puntos: el fondo del cráter (-4500 m) y una colina más alta (-3200 m)
_, temp_crater = get_mars_temperature_py(hours, elevation_m=-4500.0)
_, temp_ridge = get_mars_temperature_py(hours, elevation_m=-3200.0)

# Configuración visual de la gráfica estilo NASA/Space
plt.style.use("dark_background")
plt.figure(figsize=(10, 5), dpi=300)

# Sombrear las horas nocturnas
plt.axvspan(0, 6, color="#1e293b", alpha=0.5, lw=0)
plt.axvspan(18, 24, color="#1e293b", alpha=0.5, lw=0, label="Noche marciana")

# Curvas de temperatura
plt.plot(
    hours,
    temp_crater,
    color="#38bdf8",
    lw=2.5,
    label="Fondo del Cráter (-4.500 m)",
)
plt.plot(
    hours,
    temp_ridge,
    color="#f43f5e",
    lw=2.0,
    linestyle="--",
    label="Cresta / Colina (-3.200 m)",
)

# Estética y anotaciones
plt.title(
    "Evolución Térmica Diurna en Gale Crater (Modelo C & Python)",
    fontsize=13,
    pad=15,
    color="white",
    fontweight="bold",
)
plt.xlabel("Hora Local Solar Media (LMST)", fontsize=11, color="white")
plt.ylabel("Temperatura (°C)", fontsize=11, color="white")
plt.xlim(0, 24)
plt.xticks(
    [0, 3, 6, 9, 12, 15, 18, 21, 24],
    [
        "00:00",
        "03:00",
        "06:00 (Amanecer)",
        "09:00",
        "12:00 (Mediodía)",
        "15:00",
        "18:00 (Atardecer)",
        "21:00",
        "24:00",
    ],
    color="white",
)
plt.yticks(color="white")
plt.grid(True, color="#374151", linestyle="--", alpha=0.4)
plt.legend(facecolor="#111827", edgecolor="#374151", labelcolor="white", loc="upper left")

# Fondo general corporativo oscuro
plt.gca().set_facecolor("#0b0f19")
plt.gcf().patch.set_facecolor("#0b0f19")

plt.tight_layout()
plt.savefig(
    "mars_temperature_diurnal_curve.png",
    facecolor=plt.gcf().get_facecolor(),
    edgecolor="none",
)
print("¡Gráfica generada y guardada como 'mars_temperature_diurnal_curve.png'!")
plt.show()