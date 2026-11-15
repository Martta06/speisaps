import Feature from 'ol/Feature.js';
import Point from 'ol/geom/Point.js';
import VectorLayer from 'ol/layer/Vector.js';
import VectorSource from 'ol/source/Vector.js';
import { Circle, Fill, Stroke, Style, Text } from 'ol/style.js';
import Icon from 'ol/style/Icon.js';

const UPDATE_MS = 2000;

export function addRover(map) {
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

    let frame = 0;

    function place(position) {
        const geometry = marker.getGeometry();
        if (geometry) {
            geometry.setCoordinates(position);
        } else {
            marker.setGeometry(new Point(position));
        }
    }

    function setOrigin(position) {
        cancelAnimationFrame(frame);

        if (!position) {
            marker.setGeometry(undefined);
            status.textContent = 'Waiting for origin';
            return;
        }

        place(position);
        status.textContent = 'At origin';
    }

    function play(samples, speed = 1) {
        cancelAnimationFrame(frame);

        const valid = Array.isArray(samples) && samples.length >= 2 &&
            samples.every((point, i) =>
                Array.isArray(point) &&
                Number.isFinite(point[0]) &&
                Number.isFinite(point[1]) &&
                Number.isFinite(point[3]) &&
                point[3] >= 0 &&
                (i === 0 || point[3] >= samples[i - 1][3])
            );

        if (!valid || !Number.isFinite(speed) || speed <= 0) {
            status.textContent = 'Route timing unavailable';
            return;
        }

        const start = performance.now();
        const last = samples[samples.length - 1];
        let segment = 0;
        place(samples[0].slice(0, 2));
        status.textContent = `Route playback · ${speed}×`;

        function animate(now) {
            const elapsed = (now - start) * speed / 1000;

            if (elapsed >= last[3]) {
                place(last.slice(0, 2));
                status.textContent = 'Arrived at destination';
                return;
            }

            // Advance past every waypoint already reached, including equal timestamps.
            while (segment + 1 < samples.length && samples[segment + 1][3] <= elapsed) {
                segment++;
            }

            const a = samples[segment];
            const b = samples[segment + 1];
            const fraction = elapsed <= a[3] ? 0 : (elapsed - a[3]) / (b[3] - a[3]);

            place([
                a[0] + fraction * (b[0] - a[0]),
                a[1] + fraction * (b[1] - a[1]),
            ]);

            frame = requestAnimationFrame(animate);
        }

        frame = requestAnimationFrame(animate);
    }

    if (import.meta.hot) {
        import.meta.hot.dispose(() => {
            cancelAnimationFrame(frame);
            map.removeLayer(layer);
        });
    }

    status.textContent = 'Waiting for origin';
    return { setOrigin, play };
}
