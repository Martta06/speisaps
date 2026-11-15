import Feature from 'ol/Feature.js';
import Polygon from 'ol/geom/Polygon.js';
import VectorLayer from 'ol/layer/Vector.js';
import VectorSource from 'ol/source/Vector.js';
import { Fill, Style } from 'ol/style.js';

// Paleta extensa de 24 tonos: Blanco hielo -> Lavanda -> Morado -> Índigo -> Azul -> Cian -> Verde -> Amarillo -> Naranja -> Rojo
const SPECTRUM_24 = [
  '#f8fafc', '#e9d5ff', '#c084fc', '#9333ea',
  '#6d28d9', '#4f46e5', '#3b82f6', '#0284c7',
  '#06b6d4', '#0d9488', '#10b981', '#22c55e',
  '#65a30d', '#a3e635', '#facc15', '#eab308',
  '#f59e0b', '#f97316', '#ea580c', '#ef4444',
  '#dc2626', '#b91c1c', '#991b1b', '#7f1d1d',
];

const layers = {
  temperature: {
    unit: '°C',
    colors: SPECTRUM_24,
    // 5 marcas fijas (4 intervalos absolutos) de -95 °C a +7 °C
    ticks: [-95, -70, -45, -20, 7],
    valueToFraction(val, localCdf) {
      // Escala absoluta fija [-95 °C, +7 °C] con ligero realce topográfico local (±6%)
      // para que las curvas de nivel del cráter se distingan nítidamente a cualquier hora
      const absFrac = (val - (-95)) / (7 - (-95));
      const contourRelief = (localCdf - 0.5) * 0.14;
      return Math.min(1, Math.max(0, absFrac + contourRelief));
    },
  },
  radiation: {
    unit: 'µSv/h',
    colors: SPECTRUM_24,
    // Escala fija absoluta no lineal: primera mitad (22..30 µSv/h) para calma GCR,
    // segunda mitad (30..350 µSv/h) para Tormenta Solar (SPE)
    ticks: [22, 25, 30, 120, 350],
    valueToFraction(val, localCdf) {
      let baseFrac;
      if (val <= 30) {
        // Rango de calma GCR (22 a 30 µSv/h) ocupa de 0.05 a 0.50 de la barra (blanco/morado -> verde)
        baseFrac = ((val - 22) / (30 - 22)) * 0.50;
      } else {
        // Rango de emergencia SPE (30 a 350 µSv/h) ocupa de 0.50 a 1.00 (amarillo -> naranja -> rojo)
        baseFrac = 0.50 + Math.pow(Math.min(1, (val - 30) / (350 - 30)), 0.6) * 0.50;
      }
      const contourRelief = (localCdf - 0.5) * 0.14;
      return Math.min(1, Math.max(0, baseFrac + contourRelief));
    },
  },
};

// Búsqueda binaria sobre la CDF empírica para obtener el cuantil [0, 1]
function empiricalCdf(sortedValues, val) {
  let lo = 0;
  let hi = sortedValues.length - 1;
  while (lo < hi) {
    const mid = (lo + hi) >> 1;
    if (sortedValues[mid] < val) lo = mid + 1;
    else hi = mid;
  }
  return lo / Math.max(1, sortedValues.length - 1);
}

export function addEnvironmentPanel(map) {
  const metric = document.getElementById('environment-metric');
  const hour = document.getElementById('sol-hour');
  const hourLabel = document.getElementById('sol-hour-label');
  const status = document.getElementById('environment-status');
  const legend = document.querySelector('#environment-panel .risk-legend');

  const source = new VectorSource({ wrapX: false });
  map.addLayer(new VectorLayer({ source, zIndex: 5 }));

  let pending = null;
  let speMultiplier = 1.0;
  let autoSwitchedToRadiation = false;

  function drawGrid(grid, selectedMetric) {
    const { rows, columns, extent, values } = grid;
    const layerSpec = layers[selectedMetric];
    const { colors, unit, ticks } = layerSpec;

    if (
      grid.metric !== selectedMetric || grid.unit !== unit ||
      !Number.isInteger(rows) || rows < 1 ||
      !Number.isInteger(columns) || columns < 1 ||
      !Array.isArray(values) || values.length !== rows * columns ||
      !Array.isArray(extent) || extent.length !== 4 ||
      !extent.every(Number.isFinite)
    ) {
      throw new Error('Unexpected environment API response');
    }

    const valid = values.filter(Number.isFinite);
    if (!valid.length) {
      status.textContent = 'No data available for this hour.';
      return;
    }

    const sorted = [...valid].sort((a, b) => a - b);
    const min = sorted[0];
    const max = sorted[sorted.length - 1];

    const styles = colors.map(color =>
      new Style({ fill: new Fill({ color: `${color}88` }) })
    );

    const [west, south, east, north] = extent;
    const dx = (east - west) / columns;
    const dy = (north - south) / rows;
    const cells = [];

    for (let i = 0; i < values.length; i++) {
      const value = values[i];
      if (!Number.isFinite(value)) continue;

      // Backend row 0 starts at the southwest corner.
      const x = west + (i % columns) * dx;
      const y = south + Math.floor(i / columns) * dy;
      const ring = [
        [x, y], [x + dx, y], [x + dx, y + dy], [x, y + dy], [x, y],
      ];

      const cell = new Feature({
        geometry: new Polygon([ring]),
        value,
        unit,
      });

      const localCdf = max > min ? empiricalCdf(sorted, value) : 0.5;
      const fraction = layerSpec.valueToFraction(value, localCdf);
      const index = Math.min(colors.length - 1, Math.max(0, Math.floor(fraction * colors.length)));
      cell.setStyle(styles[index]);
      cells.push(cell);
    }

    source.addFeatures(cells);

    if (legend) {
      const wrapper = document.createElement('div');
      wrapper.style.width = '100%';
      wrapper.style.display = 'grid';
      wrapper.style.gap = '4px';

      const bar = document.createElement('div');
      bar.style.width = '100%';
      bar.style.height = '8px';
      bar.style.borderRadius = '4px';
      bar.style.border = '1px solid rgba(255, 255, 255, 0.18)';
      bar.style.background = `linear-gradient(to right, ${colors.join(', ')})`;

      const ticksRow = document.createElement('div');
      ticksRow.style.display = 'flex';
      ticksRow.style.justifyContent = 'space-between';
      ticksRow.style.fontSize = '10px';
      ticksRow.style.fontFamily = 'var(--mono)';
      ticksRow.style.color = 'var(--ink-bright)';

      ticks.forEach((tickVal, idx) => {
        const tick = document.createElement('small');
        tick.style.whiteSpace = 'nowrap';
        tick.style.fontSize = '9.5px';
        if (unit === '°C') {
          tick.textContent = `${tickVal > 0 ? `+${tickVal}` : tickVal} °C`;
        } else {
          tick.textContent = idx === ticks.length - 1 ? `${tickVal} µSv/h` : `${tickVal} µSv`;
        }
        ticksRow.appendChild(tick);
      });

      wrapper.append(bar, ticksRow);
      legend.replaceChildren(wrapper);
    }

    status.textContent =
      `${hourLabel.textContent} · Current: ${min.toFixed(1)} to ${max.toFixed(1)} ${unit}`;
  }

  async function update() {
    pending?.abort();
    pending = null;
    source.clear();
    legend?.replaceChildren();

    const selectedMetric = metric.value;
    hourLabel.textContent = `${hour.value.padStart(2, '0')}:00`;

    if (selectedMetric === 'off') {
      status.textContent = 'Layer off';
      return;
    }

    const controller = new AbortController();
    pending = controller;
    status.textContent = 'Loading environment data…';

    try {
      const params = new URLSearchParams({
        metric: selectedMetric,
        localHour: hour.value,
        speMultiplier: String(speMultiplier),
      });

      const response = await fetch(`/api/environment?${params}`, {
        signal: controller.signal,
        cache: 'no-store',
      });

      if (!response.ok) {
        throw new Error(`Environment API returned HTTP ${response.status}`);
      }

      const grid = await response.json();
      if (!controller.signal.aborted) drawGrid(grid, selectedMetric);
    } catch (error) {
      if (controller.signal.aborted) return;
      console.error(error);
      status.textContent = 'Environment data unavailable.';
    } finally {
      if (pending === controller) pending = null;
    }
  }

  metric.addEventListener('change', () => {
    autoSwitchedToRadiation = false;
    update();
  });
  hour.addEventListener('input', update);

  window.addEventListener('solar-change', event => {
    const phase = event.detail?.phase ?? 'clear';
    speMultiplier = event.detail?.speMultiplier ?? 1.0;
    if (phase !== 'clear' && metric.value === 'off') {
      metric.value = 'radiation';
      autoSwitchedToRadiation = true;
    } else if (phase === 'clear' && autoSwitchedToRadiation) {
      metric.value = 'off';
      autoSwitchedToRadiation = false;
    }
    update();
  });

  update();
}