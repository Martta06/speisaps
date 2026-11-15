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

function readRoutePoints(data) {
    if (data?.type === 'FeatureCollection') {
        const lines = data.features.filter(
            feature => feature.geometry?.type === 'LineString'
        );

        if (lines.length === 0 && data.features.length === 0) return [];
        if (lines.length !== 1) {
            throw new Error(`Expected one route line, found ${lines.length}.`);
        }

        return lines[0].geometry.coordinates;
    }

    const geometry = data?.type === 'Feature' ? data.geometry : data;

    if (geometry?.type && geometry.type !== 'LineString') {
        throw new Error(`Unexpected route geometry: ${geometry.type}`);
    }

    return geometry?.coordinates;
    }
export function addRoutePlanner(map, extent, envHooks = {}) {
    const form = document.getElementById('route-form');
    const panel = document.getElementById('route-panel');
    const status = document.getElementById('route-status');
    const submit = document.getElementById('find-route');
    const hour = document.getElementById('sol-hour');
    const travelMode = document.getElementById('travel-mode');
    const source = new VectorSource({ wrapX: false });
    const { getSolarState, getCurrentStorm } = envHooks;

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
            padding: window.matchMedia('(max-width: 800px)').matches
                ? [gap, gap, gap, gap]
                : [gap, right, gap, left],
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

    travelMode.addEventListener('change', () => {
        clearRoute();
        status.textContent = 'Travel mode changed. Select Find route to recalculate.';
    });

    hour.addEventListener('input', () => {
        clearRoute();
        status.textContent = 'Departure time updated. Select Find route to recalculate.';
    });

    async function calculateRoute({ autoRecalc = false } = {}) {
        setPicking(null);
        clearRoute();

        const origin = readPoint('origin');
        const destination = readPoint('destination');
        if (!origin || !destination) {
            if (!autoRecalc) {
                status.textContent = 'Choose two points inside the map.';
            }
            return;
        }

        if (origin[0] === destination[0] && origin[1] === destination[1]) {
            if (!autoRecalc) {
                status.textContent = 'Choose a destination different from the origin.';
            }
            return;
        }

        const solarState = getSolarState?.() ?? { speActive: false, speRate: 0, speMultiplier: 1.0, speOnset: 0 };
        const stormState = getCurrentStorm?.() ?? null;

        const controller = new AbortController();
        pending = controller;
        submit.disabled = true;
        status.textContent = autoRecalc
            ? 'Emergency condition updated · Recalculating route…'
            : 'Finding route…';

        try {
            const response = await fetch('/api/route', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                signal: controller.signal,
                body: JSON.stringify({
                    origin,
                    destination,
                    localHour: Number(hour.value),
                    mode: travelMode.value,
                    stepMeters: 20,
                    speActive: Boolean(solarState.speActive),
                    speRate: solarState.speRate ?? 0,
                    speMultiplier: solarState.speMultiplier ?? 1.0,
                    speOnset: solarState.speOnset ?? 0,
                    dustStorm: stormState?.active ? stormState : { active: false },
                }),
            });

            if (!response.ok) throw new Error(`Route API returned ${response.status}`);
            const data = await response.json();
            if (controller.signal.aborted) return;

            const points = readRoutePoints(data);

            if (Array.isArray(points) && points.length === 0) {
                status.textContent = 'No feasible route for these points and departure time.';
                return;
            }

            if (!Array.isArray(points) || points.length < 2) {
                throw new Error('The backend returned an invalid route.');
            }

            const mapCoordinates = points.map((point, index) => {
                if (!Array.isArray(point) || point.length < 2) {
                    throw new Error(`Invalid route point at index ${index}.`);
                }

                const [lon, lat] = point;
                if (!Number.isFinite(lon) || !Number.isFinite(lat) ||
                    !containsCoordinate(extent, [lon, lat])) {
                    throw new Error(`Invalid or out-of-bounds coordinates at index ${index}.`);
                }

                return [lon, lat];
            });

            route = new Feature({
                geometry: new LineString(mapCoordinates),
                samples: points,
            });

            route.setStyle(routeStyle);
            source.addFeature(route);

            // Collapse the controls so the route is easier to see.
            panel.open = false;
            fitRouteView();

            const distKm = Number.isFinite(data.distance_m)
                ? `${(data.distance_m / 1000).toFixed(2)} km`
                : null;
            const durMin = Number.isFinite(data.duration_s)
                ? `${Math.round(data.duration_s / 60)} min`
                : null;
            const doseUsv = Number.isFinite(data.radiation_dose_msv)
                ? `${(data.radiation_dose_msv * 1000).toFixed(1)} µSv`
                : null;

            const tags = [distKm, durMin, doseUsv].filter(Boolean);
            if (solarState.speActive) tags.push('SPE shelter priority');
            if (stormState?.active) tags.push('Dust storm detour');

            status.textContent = tags.length > 0
                ? `Route ready · ${tags.join(' · ')}`
                : 'Route ready.';
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
    }

    form.addEventListener('submit', event => {
        event.preventDefault();
        calculateRoute({ autoRecalc: false });
    });

    window.addEventListener('solar-change', () => {
        if (readPoint('origin') && readPoint('destination')) {
            calculateRoute({ autoRecalc: true });
        }
    });

    window.addEventListener('storm-change', () => {
        if (readPoint('origin') && readPoint('destination')) {
            calculateRoute({ autoRecalc: true });
        }
    });
}