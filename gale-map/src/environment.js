import Feature from 'ol/Feature.js';
import Polygon from 'ol/geom/Polygon.js';
import VectorLayer from 'ol/layer/Vector.js';
import VectorSource from 'ol/source/Vector.js';
import { Fill, Style } from 'ol/style.js';

const layers = {
  temperature: {
    unit: '°C',
    colors: ['#476bc2', '#63a5d5', '#a4dacf', '#f4bd72', '#df6553'],
  },
  radiation: {
    unit: 'µSv/h',
    colors: ['#5084a1', '#66b8ae', '#d4ce78', '#ec9951', '#da5451'],
  },
};

export function addEnvironmentPanel(map) {
  const metric = document.getElementById('environment-metric');
  const hour = document.getElementById('sol-hour');
  const hourLabel = document.getElementById('sol-hour-label');
  const status = document.getElementById('environment-status');
  const legend = document.querySelector('#environment-panel .risk-legend');

  const source = new VectorSource({ wrapX: false });
  map.addLayer(new VectorLayer({ source, zIndex: 5 }));

  let pending = null;

  function drawGrid(grid, selectedMetric) {
    const { rows, columns, extent, values } = grid;
    const { colors, unit } = layers[selectedMetric];

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

    const min = Math.min(...valid);
    const max = Math.max(...valid);
    const styles = colors.map(color =>
      new Style({ fill: new Fill({ color: `${color}99` }) })
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

      const fraction = max === min ? 0.5 : (value - min) / (max - min);
      const index = Math.min(colors.length - 1, Math.floor(fraction * colors.length));
      cell.setStyle(styles[index]);
      cells.push(cell);
    }

    source.addFeatures(cells);

    if (legend) {
      const bar = document.createElement('div');
      bar.style.width = '100%';
      bar.style.height = '6px';
      bar.style.background = `linear-gradient(to right, ${colors.join(', ')})`;
      legend.replaceChildren(bar);
    }

    status.textContent =
      `${hourLabel.textContent} · ${min.toFixed(2)} to ${max.toFixed(2)} ${unit}`;
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

  metric.addEventListener('change', update);
  hour.addEventListener('input', update);
  update();
}