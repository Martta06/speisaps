import Feature from 'ol/Feature.js';
import Point from 'ol/geom/Point.js';
import LineString from 'ol/geom/LineString.js';
import VectorLayer from 'ol/layer/Vector.js';
import VectorSource from 'ol/source/Vector.js';
import { Circle, Fill, Stroke, Style, Text } from 'ol/style.js';
import { containsCoordinate } from 'ol/extent.js';

function markerStyle(letter) {
    return new Style({
        image: new Circle({
            radius: 12,
            fill: new Fill({ color: '#172522' }),
            stroke: new Stroke({ color: '#ffffff', width: 2 }),
        }),
        text: new Text({
            text: letter,
            font: 'bold 13px system-ui',
            fill: new Fill({ color: '#ffffff' }),
        }),
        zIndex: 2,
    });
}

const routeStyle = [
    new Style({ stroke: new Stroke({ color: '#142022', width: 7 }) }),
    new Style({ stroke: new Stroke({ color: '#66edff', width: 4 }) }),
];

export function addRoutePlanner(map, extent) {
    const form = document.getElementById('route-form');
    const panel = document.getElementById('route-panel');
    const status = document.getElementById('route-status');
    const submit = document.getElementById('find-route');
    const hour = document.getElementById('sol-hour');
    const source = new VectorSource({ wrapX: false });

  // Above the environmental overlay, which uses zIndex 5.
    map.addLayer(new VectorLayer({ source, zIndex: 20 }));

    const fields = {};
    const markers = {};
    let picking = null;
    let route = null;
    let pending = null;

    function fitRouteView() {
        if (!readPoint('origin') || !readPoint('destination')) return;

        const mapRect = map.getTargetElement().getBoundingClientRect();
        const environmentRect = document
            .getElementById('environment-panel')
            .getBoundingClientRect();
        const plannerRect = panel.getBoundingClientRect();

  // Leave room for each marker and its outline.
        const gap = 28;
        const left = Math.max(0, environmentRect.right - mapRect.left) + gap;
        const right = Math.max(0, mapRect.right - plannerRect.left) + gap;

        const view = map.getView();
        view.cancelAnimations();
        view.fit(source.getExtent(), {
            // OpenLayers order: top, right, bottom, left.
            padding: [gap, right, gap, left],
            duration: 500,
            maxZoom: 19,
        });
    }

    function clearRoute() {
        pending?.abort();
        pending = null;
        submit.disabled = false;
        if (route) source.removeFeature(route);
        route = null;
    }

    function readPoint(key) {
        const { lon, lat } = fields[key];
        const point = [lon.valueAsNumber, lat.valueAsNumber];
        if (!point.every(Number.isFinite)) return null;
        return containsCoordinate(extent, point) ? point : null;
    }

    function refreshMarker(key) {
        const point = readPoint(key);
        markers[key].setGeometry(point ? new Point(point) : undefined);
    }

    function setPicking(key) {
        picking = key;
        map.getTargetElement().style.cursor = key ? 'crosshair' : '';
        for (const name of ['origin', 'destination']) {
        fields[name].button.setAttribute('aria-pressed', String(key === name));
        }
    }

    for (const [key, letter] of [['origin', 'A'], ['destination', 'B']]) {
        const lon = document.getElementById(`${key}-lon`);
        const lat = document.getElementById(`${key}-lat`);
        const button = document.getElementById(`pick-${key}`);
        fields[key] = { lon, lat, button };

        lon.min = extent[0];
        lon.max = extent[2];
        lat.min = extent[1];
        lat.max = extent[3];

        markers[key] = new Feature();
        markers[key].setStyle(markerStyle(letter));
        source.addFeature(markers[key]);

        for (const input of [lon, lat]) {
            input.addEventListener('input', () => {
                setPicking(null);
                clearRoute();
                refreshMarker(key);
                status.textContent = 'Select Find route when both points are ready.';
            });
            input.addEventListener('change', fitRouteView);
        }

        button.addEventListener('click', () => {
            setPicking(key);
            status.textContent = `Click the map to set the ${key}.`;
        });
    }

    map.on('singleclick', event => {
        if (!picking) return;

        const key = picking;
        if (!containsCoordinate(extent, event.coordinate)) {
            status.textContent = 'Choose a point inside the terrain image.';
            return;
        }

        clearRoute();
        fields[key].lon.value = String(event.coordinate[0]);
        fields[key].lat.value = String(event.coordinate[1]);
        refreshMarker(key);

        if (key === 'origin' && !readPoint('destination')) {
            setPicking('destination');
            status.textContent = 'Origin set. Click the map to set the destination.';
        } else {
            setPicking(null);
            status.textContent = 'Point set. Select Find route when ready.';
            fitRouteView();
        }
    });

    hour.addEventListener('input', () => {
        clearRoute();
        status.textContent = 'Departure time updated. Select Find route to recalculate.';
    });

    form.addEventListener('submit', async event => {
        event.preventDefault();
        setPicking(null);
        clearRoute();

        const origin = readPoint('origin');
        const destination = readPoint('destination');
        if (!origin || !destination) {
            status.textContent = 'Choose two points inside the map.';
            return;
        }

        if (origin[0] === destination[0] && origin[1] === destination[1]) {
            status.textContent = 'Choose a destination different from the origin.';
            return;
        }

        const controller = new AbortController();
        pending = controller;
        submit.disabled = true;
        status.textContent = 'Finding route…';

        try {
            const response = await fetch('/api/route', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                signal: controller.signal,
                body: JSON.stringify({
                    crs: 'ESRI:104905',
                    origin,
                    destination,
                    sol: 1,
                    localHour: Number(hour.value),
                }),
            });

            if (!response.ok) throw new Error(`Route API returned ${response.status}`);
            const { coordinates } = await response.json();
            if (controller.signal.aborted) return;

            if (Array.isArray(coordinates) && coordinates.length === 0) {
                status.textContent = 'No feasible route for these points and departure time.';
                return;
            }

            const valid = Array.isArray(coordinates) && coordinates.length >= 2 &&
                coordinates.every(point =>
                    Array.isArray(point) && point.length === 2 &&
                    point.every(Number.isFinite) && containsCoordinate(extent, point)
                );

            if (!valid) throw new Error('Invalid route coordinates');

            const geometry = new LineString(coordinates);
            route = new Feature(geometry);
            route.setStyle(routeStyle);
            source.addFeature(route);

            // Collapse the controls so the route is easier to see.
            panel.open = false;
            fitRouteView();
            status.textContent = 'Route ready.';
        } catch (error) {
            if (controller.signal.aborted) return;
            console.error(error);
            status.textContent = 'Route unavailable. Check the routing service and try again.';
        } finally {
            if (pending === controller) {
                pending = null;
                submit.disabled = false;
            }
        }
    });
}