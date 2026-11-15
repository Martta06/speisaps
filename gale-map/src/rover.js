import Feature from 'ol/Feature.js';
import Point from 'ol/geom/Point.js';
import VectorLayer from 'ol/layer/Vector.js';
import VectorSource from 'ol/source/Vector.js';
import { Circle, Fill, Stroke, Style, Text } from 'ol/style.js';
import Icon from 'ol/style/Icon.js';

const UPDATE_MS = 2000;

export function addRover(map, { demo = true } = {}) {
    const status = document.getElementById('rover-status');
    const marker = new Feature();

    const icon = new Icon({
        src: `${import.meta.env.BASE_URL}icons/user-round.svg`,
        anchor: [0.5, 0.5],
    });

    const label = new Text({
        text: 'Rover',
        font: '600 11px system-ui',
        fill: new Fill({ color: '#ffffff' }),
        stroke: new Stroke({ color: '#07121a', width: 3 }),
    });

    const roverStyle = new Style({ image: icon, text: label });

// Use the initial overview as the reference zoom.
    const overviewResolution = map.getView().getResolution();

    marker.setStyle((_feature, resolution) => {
        const zoomFactor = Math.sqrt(overviewResolution / resolution);
        const sizePx = Math.max(14, Math.min(24, 16 * zoomFactor));

        icon.setScale(sizePx / 32); // The SVG is 32 px wide.
        label.setOffsetY(-(sizePx / 2 + 8));

        return roverStyle;
    });

    const layer = new VectorLayer({
        source: new VectorSource({ features: [marker], wrapX: false }),
        zIndex: 25,
    });
    map.addLayer(layer);

    let timer;
    let frame;
    let request;
    let stopped = false;
    let lastTimestamp = -Infinity;
    const demoStart = performance.now();

    function moveTo(target) {
        cancelAnimationFrame(frame);
        const point = marker.getGeometry();

        // Show the first known position immediately.
        if (!point) {
            marker.setGeometry(new Point(target));
            return;
        }

        const start = point.getCoordinates();
        const startedAt = performance.now();

        function animate(now) {
            const t = Math.min(1, (now - startedAt) / UPDATE_MS);
            point.setCoordinates([
                start[0] + (target[0] - start[0]) * t,
                start[1] + (target[1] - start[1]) * t,
            ]);
            if (t < 1) frame = requestAnimationFrame(animate);
        }

        frame = requestAnimationFrame(animate);
    }

    function demoPosition() {
        // Accelerated preview inside the Gale map, near the demo storm.
        const t = (performance.now() - demoStart) / 1000;
        return {
            position: [
                137.38 + 0.035 * Math.sin(t / 12),
                -4.65 + 0.012 * Math.sin(t / 6),
            ],
            timestamp: new Date().toISOString(),
        };
    }

    async function poll() {
        const startedAt = performance.now();
        request = new AbortController();
        const timeout = setTimeout(() => request.abort(), 5000);

        try {
            let sample;
            if (demo) {
                sample = demoPosition();
            } else {
                const response = await fetch('/api/rover', {
                    signal: request.signal,
                    cache: 'no-store',
                });
                if (!response.ok) throw new Error(`Rover API: ${response.status}`);
                sample = await response.json();
            }

            if (stopped) return;
            const position = sample?.position;
            const timestamp = Date.parse(sample?.timestamp);
            if (
                !Array.isArray(position) || position.length !== 2 ||
                !position.every(Number.isFinite) || !Number.isFinite(timestamp)
            ) {
                throw new Error('Invalid rover position');
            }

            if (timestamp > lastTimestamp) {
                lastTimestamp = timestamp;
                moveTo(position);
                status.textContent = demo
                    ? 'Rover · Simulated position'
                    : `Rover · Position received ${sample.timestamp}`;
            } else {
                status.textContent = 'Rover · Waiting for a newer position';
            }
        } catch (error) {
            if (stopped) return;
            console.warn('Rover update unavailable', error);
            status.textContent = marker.getGeometry()
                ? 'Rover offline · Last known position'
                : 'Rover offline · Position unavailable';
        } finally {
            clearTimeout(timeout);
            if (!stopped) {
                const elapsed = performance.now() - startedAt;
                timer = setTimeout(poll, Math.max(250, UPDATE_MS - elapsed));
            }
        }
    }

    poll();

    if (import.meta.hot) {
        import.meta.hot.dispose(() => {
            stopped = true;
            clearTimeout(timer);
            cancelAnimationFrame(frame);
            request?.abort();
            map.removeLayer(layer);
        });
    }
}