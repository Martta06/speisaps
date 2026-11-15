import Feature from 'ol/Feature.js';
import Polygon from 'ol/geom/Polygon.js';
import VectorLayer from 'ol/layer/Vector.js';
import VectorSource from 'ol/source/Vector.js';
import { Fill, Stroke, Style, Text } from 'ol/style.js';

const colors = ['#58c997', '#c6d957', '#ffb347', '#ff784f', '#f04462'];
const labels = ['Low', 'Moderate', 'High', 'Very high', 'Extreme'];

const motionLabels = {
    approaching: 'Approaching reference point',
    receding: 'Moving away from reference point',
    unknown: 'Approach unknown',
};

// Local metres per degree for the Mars 2000 ellipsoid.
// Suitable for drawing a storm footprint around Gale.
function metresPerDegree(latitude) {
    const a = 3396190;
    const b = 3376200;
    const radians = Math.PI / 180;
    const phi = latitude * radians;
    const e2 = 1 - (b * b) / (a * a);
    const denominator = 1 - e2 * Math.sin(phi) ** 2;
    const n = a / Math.sqrt(denominator);
    const m = a * (1 - e2) / denominator ** 1.5;

    return [n * Math.cos(phi) * radians, m * radians];
}

function stormFootprint(center, diameterM) {
    const [longitude, latitude] = center;
    const [xScale, yScale] = metresPerDegree(latitude);
    const radius = diameterM / 2;
    const ring = [];

    for (let index = 0; index < 64; index++) {
        const angle = (index / 64) * 2 * Math.PI;
        ring.push([
            longitude + radius * Math.cos(angle) / xScale,
            latitude + radius * Math.sin(angle) / yScale,
        ]);
    }

    ring.push([...ring[0]]);
    return new Polygon([ring]);
}

export function addStormOverlay(map) {
    const source = new VectorSource({ wrapX: false });
    const footprint = new Feature();
    source.addFeature(footprint);

  // Above environmental colors and below route markers.
    map.addLayer(new VectorLayer({
        source,
        zIndex: 10,
    }));

    const alert = document.getElementById('storm-alert');
    const title = document.getElementById('storm-title');
    const details = document.getElementById('storm-details');
    let demoTimer = null;

    function renderStorm(storm) {
        if (!storm) {
            footprint.setGeometry(undefined);
            alert.hidden = true;
            return;
        }

        const valid =
            Array.isArray(storm.center) &&
            storm.center.length === 2 &&
            storm.center.every(Number.isFinite) &&
            Math.abs(storm.center[1]) < 90 &&
            Number.isFinite(storm.diameterM) &&
            storm.diameterM > 0 &&
            Number.isInteger(storm.dangerLevel) &&
            storm.dangerLevel >= 1 &&
            storm.dangerLevel <= 5;

        if (!valid) {
            throw new Error('Invalid storm data');
        }

        const color = colors[storm.dangerLevel - 1];

        footprint.setGeometry(stormFootprint(storm.center, storm.diameterM));
        footprint.setStyle(new Style({
            fill: new Fill({ color: `${color}44` }),
            stroke: new Stroke({
                color,
                width: 3,
                lineDash: [10, 6],
            }),
            text: new Text({
                text: `DUST STORM · ${storm.dangerLevel}/5`,
                font: 'bold 13px system-ui',
                fill: new Fill({ color: '#ffffff' }),
                stroke: new Stroke({ color: '#202624', width: 4 }),
                overflow: true,
            }),
        }));

        alert.hidden = false;
        alert.style.setProperty('--storm-color', color);
        title.textContent =
            `Dust storm · ${labels[storm.dangerLevel - 1]} · ${storm.dangerLevel}/5`;

        const motion = motionLabels[storm.motion] ?? motionLabels.unknown;
        const diameter = `${(storm.diameterM / 1000).toFixed(1)} km diameter`;
        const wind = Number.isFinite(storm.windSpeedMs)
            ? `${storm.windSpeedMs.toFixed(1)} m/s wind`
            : 'Wind speed unavailable';

        const provenance = storm.simulated
            ? 'SIMULATED DATA'
            : 'Detector update';

        details.textContent = `${provenance} · ${diameter} · ${wind} · ${motion}`;
    }

    function stopDemo() {
        clearInterval(demoTimer);
        demoTimer = null;
    }

  // Entry point for future backend updates.
    function updateStorm(storm) {
        stopDemo();
        renderStorm(storm);
    }

    document.getElementById('start-storm').addEventListener('click', () => {
        stopDemo();
        const startedAt = performance.now();

        function frame() {
            const seconds = (performance.now() - startedAt) / 1000;
            const simulatedSeconds = seconds * 60;
            const [xScale] = metresPerDegree(-4.65);

            renderStorm({
                center: [
                    137.28 + 10 * simulatedSeconds / xScale,
                    -4.65,
                ],
                diameterM: 5000,
                windSpeedMs: 20,
                dangerLevel: 3,
                motion: 'unknown',
                simulated: true,
            });

            if (seconds >= 30) stopDemo();
        }

        frame();
        demoTimer = setInterval(frame, 1000);
    });

    document.getElementById('stop-storm').addEventListener('click', () => {
        stopDemo();
        renderStorm(null);
    });

  // Prevent duplicate demo timers after Vite hot reloads.
    if (import.meta.hot) {
        import.meta.hot.dispose(stopDemo);
    }

    return { updateStorm };
}