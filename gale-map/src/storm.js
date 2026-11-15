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
    stationary: 'Stationary (frozen)',
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
    const freezeBtn = document.getElementById('freeze-storm');
    let demoTimer = null;
    let currentStorm = null;
    let usingLocalFallback = false;
    let isFrozen = false;
    let savedVelocity = { velEastMs: 8.0, velNorthMs: 0.0, motion: 'approaching', spec: null };

    function randomBetween(min, max) {
        return min + Math.random() * (max - min);
    }

    function generateRandomStormSpec() {
        const origLon = parseFloat(document.getElementById('origin-lon')?.value);
        const origLat = parseFloat(document.getElementById('origin-lat')?.value);
        const destLon = parseFloat(document.getElementById('destination-lon')?.value);
        const destLat = parseFloat(document.getElementById('destination-lat')?.value);
        const hasRoutePoints = [origLon, origLat, destLon, destLat].every(Number.isFinite);

        // Gale Crater safe interior bounds
        const minLon = 137.18, maxLon = 137.66;
        const minLat = -4.86, maxLat = -4.31;

        let lon, lat;
        if (hasRoutePoints && Math.random() < 0.7) {
            // 70% de probabilidad de spawnear cerca de la ruta activa (con jitter aleatorio)
            const t = randomBetween(0.25, 0.75);
            lon = origLon + (destLon - origLon) * t + randomBetween(-0.025, 0.025);
            lat = origLat + (destLat - origLat) * t + randomBetween(-0.025, 0.025);
            lon = Math.min(maxLon, Math.max(minLon, lon));
            lat = Math.min(maxLat, Math.max(minLat, lat));
        } else {
            lon = randomBetween(minLon, maxLon);
            lat = randomBetween(minLat, maxLat);
        }

        const dangerLevel = Math.floor(randomBetween(1, 6)); // 1..5
        const diameterM = Math.round(randomBetween(3500, 8500));
        const windSpeedMs = Number(randomBetween(12 + dangerLevel * 3, 18 + dangerLevel * 4.5).toFixed(1));
        const angle = randomBetween(-Math.PI * 0.75, Math.PI * 0.75);
        // Velocidad de deriva para la predicción en C (~0.02 - 0.05 m/s):
        // desplaza el centro ~60-150 m durante la travesía para sesgar el rodeo por barlovento,
        // pero manteniendo el núcleo bloqueado alineado con el círculo visible en pantalla.
        const driftSpeed = randomBetween(0.02, 0.05);
        const velEastMs = Number((driftSpeed * Math.cos(angle)).toFixed(4));
        const velNorthMs = Number((driftSpeed * Math.sin(angle)).toFixed(4));
        const motion = velEastMs >= 0 ? 'approaching' : 'receding';
        // Multiplicador alto para que tanto el núcleo como la periferia fuercen un rodeo claro
        const severityPenalty = Number((80.0 + dangerLevel * 40.0).toFixed(1));

        return {
            center: [Number(lon.toFixed(6)), Number(lat.toFixed(6))],
            diameterM,
            windSpeedMs,
            dangerLevel,
            velEastMs,
            velNorthMs,
            motion,
            severityPenalty,
        };
    }

    function renderStorm(storm) {
        if (!storm || storm.active === false) {
            currentStorm = null;
            isFrozen = false;
            if (freezeBtn) {
                freezeBtn.hidden = true;
                freezeBtn.textContent = 'Freeze storm';
            }
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

        if (freezeBtn) {
            freezeBtn.hidden = false;
            freezeBtn.textContent = (isFrozen || storm.frozen) ? 'Resume storm' : 'Freeze storm';
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
                text: `DUST STORM · ${storm.dangerLevel}/5${(isFrozen || storm.frozen) ? ' (FROZEN)' : ''}`,
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

        const effectiveMotion = (isFrozen || storm.frozen) ? 'stationary' : storm.motion;
        const motion = motionLabels[effectiveMotion] ?? motionLabels.unknown;
        const diameter = `${(storm.diameterM / 1000).toFixed(1)} km diameter`;
        const wind = Number.isFinite(storm.windSpeedMs)
            ? `${storm.windSpeedMs.toFixed(1)} m/s wind`
            : 'Wind speed unavailable';

        const provenance = storm.simulated
            ? 'SIMULATED DATA'
            : 'Detector update';

        details.textContent = `${provenance} · ${diameter} · ${wind} · ${motion}`;
        currentStorm = {
            active: true,
            frozen: Boolean(isFrozen || storm.frozen),
            center: storm.center,
            diameterM: storm.diameterM,
            radiusM: storm.radiusM ?? (storm.diameterM / 2),
            velEastMs: (isFrozen || storm.frozen) ? 0.0 : (storm.velEastMs ?? 10.0),
            velNorthMs: (isFrozen || storm.frozen) ? 0.0 : (storm.velNorthMs ?? 0.0),
            windSpeedMs: storm.windSpeedMs,
            dangerLevel: storm.dangerLevel,
            motion: effectiveMotion,
            severityPenalty: storm.severityPenalty ?? (storm.dangerLevel * 10.0),
        };
    }

    function stopDemo() {
        clearInterval(demoTimer);
        demoTimer = null;
    }

  // Entry point for future backend updates.
    function updateStorm(storm) {
        if (usingLocalFallback) return;
        // Si el backend antiguo devuelve la tormenta estática por defecto sin 'active',
        // la ignoramos hasta que el usuario pulse "Simulate dust storm".
        if (storm && storm.active === undefined && !currentStorm) {
            return;
        }
        renderStorm(storm);
    }

    function getCurrentStorm() {
        return currentStorm;
    }

    function isLocalFallback() {
        return usingLocalFallback;
    }

    function startLocalDriftFrom(spec) {
        stopDemo();
        usingLocalFallback = true;
        const startedAt = performance.now();
        const [xScale, yScale] = metresPerDegree(spec.center[1]);

        function frame() {
            if (isFrozen) return;
            const seconds = (performance.now() - startedAt) / 1000;
            const simulatedSeconds = seconds * 250; // avance visual suave (~15-20 m cada 2s en el mapa)
            const curLon = spec.center[0] + (spec.velEastMs * simulatedSeconds) / xScale;
            const curLat = spec.center[1] + (spec.velNorthMs * simulatedSeconds) / yScale;

            // Comprobar si salió de Gale Crater
            const radiusDeg = (spec.diameterM / 2) / yScale;
            if (
                curLon - radiusDeg > 137.7298 ||
                curLon + radiusDeg < 137.1224 ||
                curLat - radiusDeg > -4.2489 ||
                curLat + radiusDeg < -4.9254
            ) {
                stopDemo();
                usingLocalFallback = false;
                renderStorm(null);
                window.dispatchEvent(new CustomEvent('storm-change', { detail: null }));
                return;
            }

            renderStorm({
                active: true,
                center: [curLon, curLat],
                diameterM: spec.diameterM,
                windSpeedMs: spec.windSpeedMs,
                dangerLevel: spec.dangerLevel,
                velEastMs: spec.velEastMs,
                velNorthMs: spec.velNorthMs,
                severityPenalty: spec.severityPenalty,
                motion: spec.motion,
                simulated: true,
            });
        }

        frame();
        demoTimer = setInterval(frame, 2000);
    }

    document.getElementById('start-storm').addEventListener('click', async () => {
        stopDemo();
        usingLocalFallback = false;
        isFrozen = false;

        const spec = generateRandomStormSpec();
        savedVelocity = {
            velEastMs: spec.velEastMs,
            velNorthMs: spec.velNorthMs,
            motion: spec.motion,
            spec,
        };

        try {
            const res = await fetch('/api/storm', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(spec),
            });
            if (res.ok) {
                const stormData = await res.json();
                renderStorm(stormData);
                window.dispatchEvent(new CustomEvent('storm-change', { detail: currentStorm }));
                return;
            }
        } catch (err) {
            console.warn('Backend /api/storm POST unavailable, using local storm simulation', err);
        }

        // Fallback local si server.py no está corriendo o es una versión anterior sin POST /api/storm
        startLocalDriftFrom(spec);
        window.dispatchEvent(new CustomEvent('storm-change', { detail: currentStorm }));
    });

    if (freezeBtn) {
        freezeBtn.addEventListener('click', async () => {
            if (!currentStorm) return;

            isFrozen = !isFrozen;

            if (isFrozen) {
                stopDemo();
                try {
                    const res = await fetch('/api/storm', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ action: 'freeze' }),
                    });
                    if (res.ok) {
                        const data = await res.json();
                        if (data?.active) {
                            renderStorm(data);
                            window.dispatchEvent(new CustomEvent('storm-change', { detail: currentStorm }));
                            return;
                        }
                    }
                } catch (_e) {
                    // Fallback local
                }
                renderStorm({
                    ...currentStorm,
                    frozen: true,
                    velEastMs: 0.0,
                    velNorthMs: 0.0,
                    motion: 'stationary',
                    simulated: true,
                });
                window.dispatchEvent(new CustomEvent('storm-change', { detail: currentStorm }));
            } else {
                // Reanudar movimiento desde donde estaba congelada
                const resumeSpec = {
                    ...(savedVelocity.spec ?? currentStorm),
                    center: [...currentStorm.center],
                    velEastMs: savedVelocity.velEastMs,
                    velNorthMs: savedVelocity.velNorthMs,
                    motion: savedVelocity.motion,
                };
                savedVelocity.spec = resumeSpec;

                if (!usingLocalFallback) {
                    try {
                        const res = await fetch('/api/storm', {
                            method: 'POST',
                            headers: { 'Content-Type': 'application/json' },
                            body: JSON.stringify({ action: 'unfreeze' }),
                        });
                        if (res.ok) {
                            const data = await res.json();
                            if (data?.active) {
                                renderStorm(data);
                                window.dispatchEvent(new CustomEvent('storm-change', { detail: currentStorm }));
                                return;
                            }
                        }
                    } catch (_e) {
                        // Fallback local
                    }
                }
                startLocalDriftFrom(resumeSpec);
                window.dispatchEvent(new CustomEvent('storm-change', { detail: currentStorm }));
            }
        });
    }

    document.getElementById('stop-storm').addEventListener('click', async () => {
        stopDemo();
        usingLocalFallback = false;
        isFrozen = false;
        renderStorm(null);
        try {
            await fetch('/api/storm', { method: 'DELETE' });
        } catch (_e) {
            // Ignore offline error
        }
        window.dispatchEvent(new CustomEvent('storm-change', { detail: null }));
    });

  // Prevent duplicate demo timers after Vite hot reloads.
    if (import.meta.hot) {
        import.meta.hot.dispose(stopDemo);
    }

    return { updateStorm, getCurrentStorm, isLocalFallback };
}