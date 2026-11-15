import ImageLayer from 'ol/layer/Image.js';
import ImageStatic from 'ol/source/ImageStatic.js';

// Paleta extensa de 24 tonos: Blanco hielo -> Lavanda -> Morado -> Índigo -> Azul -> Cian -> Verde -> Amarillo -> Naranja -> Rojo
const SPECTRUM_24 = [
  '#f8fafc', '#e9d5ff', '#c084fc', '#9333ea',
  '#6d28d9', '#4f46e5', '#3b82f6', '#0284c7',
  '#06b6d4', '#0d9488', '#10b981', '#22c55e',
  '#65a30d', '#a3e635', '#facc15', '#eab308',
  '#f59e0b', '#f97316', '#ea580c', '#ef4444',
  '#dc2626', '#b91c1c', '#991b1b', '#7f1d1d',
];

// Tabla RGB precalculada para renderizar en Canvas/GPU en pocos milisegundos
const SPECTRUM_RGB = SPECTRUM_24.map(hex => [
  parseInt(hex.slice(1, 3), 16),
  parseInt(hex.slice(3, 5), 16),
  parseInt(hex.slice(5, 7), 16),
]);

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
      if (val <= 32) {
        // Rango de calma GCR (22 a 32 µSv/h): ocupa la primera mitad (blanco/morado -> azul -> verde)
        const baseFrac = ((val - 22) / (32 - 22)) * 0.42;
        const contourRelief = (localCdf - 0.5) * 0.24;
        return Math.min(0.58, Math.max(0, baseFrac + contourRelief));
      }
      // En Tormenta Solar (SPE > 32 µSv/h): desplaza la paleta hacia la mitad de alerta
      // (0.48 amarillo/ámbar en cañones protegidos -> 1.00 rojo/granate en llanuras expuestas)
      // para que se vea el peligro global PERO manteniendo nítido el relieve de los refugios.
      const isHeavySpe = val > 200;
      const minAlertFrac = isHeavySpe ? 0.55 : 0.42;
      return Math.min(1, Math.max(minAlertFrac, minAlertFrac + (1 - minAlertFrac) * localCdf));
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

  const layer = new ImageLayer({ zIndex: 5, visible: false, opacity: 0.62 });
  map.addLayer(layer);
  const canvas = document.createElement('canvas');
  const ctx = canvas.getContext('2d');

  let pending = null;
  let debounceTimer = null;
  let speMultiplier = 1.0;
  let autoSwitchedToRadiation = false;

  function formatLmst(val) {
    const num = Number(val);
    const h = Math.floor(num) % 24;
    const m = Math.round((num - Math.floor(num)) * 60) % 60;
    return `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}`;
  }

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

    // Submuestreamos cada 8 puntos para ordenar la CDF en < 1 ms incluso con 324.000 píxeles
    const sampleStep = Math.max(1, Math.floor(valid.length / 8000));
    const sampled = [];
    for (let i = 0; i < valid.length; i += sampleStep) sampled.push(valid[i]);
    const sorted = sampled.sort((a, b) => a - b);

    let min = Infinity;
    let max = -Infinity;
    for (let i = 0; i < valid.length; i++) {
      if (valid[i] < min) min = valid[i];
      if (valid[i] > max) max = valid[i];
    }

    canvas.width = columns;
    canvas.height = rows;
    const imgData = ctx.createImageData(columns, rows);
    const pixels = imgData.data;
    const nColors = colors.length;

    // La fila 0 del backend empieza en el borde Sur (abajo), mientras que la fila 0 del Canvas es el Norte (arriba)
    for (let r = 0; r < rows; r++) {
      const srcRowOffset = r * columns;
      const dstRowOffset = (rows - 1 - r) * columns * 4;
      for (let c = 0; c < columns; c++) {
        const value = values[srcRowOffset + c];
        if (!Number.isFinite(value)) continue;

        const localCdf = max > min ? empiricalCdf(sorted, value) : 0.5;
        const fraction = layerSpec.valueToFraction(value, localCdf);
        const idx = Math.min(nColors - 1, Math.max(0, (fraction * nColors) | 0));
        const rgb = SPECTRUM_RGB[idx];

        const p = dstRowOffset + (c << 2);
        pixels[p] = rgb[0];
        pixels[p + 1] = rgb[1];
        pixels[p + 2] = rgb[2];
        pixels[p + 3] = 225;
      }
    }

    ctx.putImageData(imgData, 0, 0);
    layer.setSource(new ImageStatic({
      url: canvas.toDataURL(),
      projection: map.getView().getProjection(),
      imageExtent: extent,
      interpolate: true,
    }));
    layer.setVisible(true);

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

    const selectedMetric = metric.value;
    hourLabel.textContent = formatLmst(hour.value);

    if (selectedMetric === 'off') {
      layer.setVisible(false);
      legend?.replaceChildren();
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
      if (!controller.signal.aborted) {
        drawGrid(grid, selectedMetric);
      }
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
  hour.addEventListener('input', () => {
    hourLabel.textContent = formatLmst(hour.value);
    clearTimeout(debounceTimer);
    debounceTimer = setTimeout(update, 45);
  });

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