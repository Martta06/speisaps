import Feature from 'ol/Feature.js';
import Polygon from 'ol/geom/Polygon.js';
import VectorLayer from 'ol/layer/Vector.js';
import VectorSource from 'ol/source/Vector.js';
import { Fill, Style } from 'ol/style.js';

// We use colors depending on the risks
const riskStyles = {
    low: new Style({ fill: new Fill({ color: 'rgba(54, 185, 138, 0.33)' }) }),
    moderate: new Style({ fill: new Fill({ color: 'rgba(243, 181, 68, 0.43)' }) }),
    high: new Style({ fill: new Fill({ color: 'rgba(234, 96, 88, 0.53)' }) }),
};

// Synthetic patterns for the interface preview.
function makePreviewGrid(metric, hour, extent) {
    const columns = 32;
    const rows = 36;
    const phase = (2 * Math.PI * hour) / 24;
    const levels = [];

  // Row 0 is the southern edge. Each row runs west to east.
    for (let row = 0; row < rows; row++) {
        for (let column = 0; column < columns; column++) {
        const x = (column + 0.5) / columns;  // Centre of each celda normalized between 0 and 1
        const y = (row + 0.5) / rows;

        const pattern = metric === 'temperature'  // Test function for temperature to make smooth transitions
            ? Math.sin(4 * x + phase) + 0.7 * Math.cos(5 * y - phase)
            : Math.cos(5 * x - 3 * y - phase) + 0.5 * Math.sin(6 * y + phase);

        let level = 'moderate';
        if (pattern < -0.45) level = 'low';
        if (pattern > 0.45) level = 'high';
        levels.push(level);
        }   
    }

    return { extent, columns, rows, levels };
}

function drawGrid(source, grid) {
    const { extent, columns, rows, levels } = grid;
    const [west, south, east, north] = extent;
    const dx = (east - west) / columns;
    const dy = (north - south) / rows;
    const features = [];

    for (let i = 0; i < levels.length; i++) {
        const level = levels[i];
        if (!Object.hasOwn(riskStyles, level)) continue;

        const left = west + (i % columns) * dx;
        const bottom = south + Math.floor(i / columns) * dy;
        const right = left + dx;
        const top = bottom + dy;

        features.push(new Feature({
        level,
        geometry: new Polygon([[
            [left, bottom], [right, bottom], [right, top],
            [left, top], [left, bottom],
        ]]),
        }));
    }

    source.clear();
    source.addFeatures(features);
}

export function addEnvironmentPanel(map, extent) {
    const metric = document.getElementById('environment-metric');
    const hour = document.getElementById('sol-hour');
    const hourLabel = document.getElementById('sol-hour-label');
    const status = document.getElementById('environment-status');

    const source = new VectorSource({ wrapX: false });
    const layer = new VectorLayer({
        source,
        zIndex: 5,
        style: feature => riskStyles[feature.get('level')],
    });
    map.addLayer(layer);

    function update() {
        const time = Number(hour.value);
        hourLabel.textContent = `${String(time).padStart(2, '0')}:00`;

        const active = metric.value !== 'off';
        layer.setVisible(active);

        if (!active) {
        status.textContent = 'Layer off';
        return;
        }

        const grid = makePreviewGrid(metric.value, time, extent);
        drawGrid(source, grid);
        status.textContent = 'Simulated risk · Interface preview';
    }

    metric.addEventListener('change', update);
    hour.addEventListener('input', update);
    update();
}