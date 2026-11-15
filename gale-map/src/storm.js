import Feature from 'ol/Feature.js';
import Polygon from 'ol/geom/Polygon.js';
import Translate from 'ol/interaction/Translate.js';
import VectorLayer from 'ol/layer/Vector.js';
import VectorSource from 'ol/source/Vector.js';
import { getCenter } from 'ol/extent.js';
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
    let animFrame = 0;
    let currentStorm = null;
    let isFrozen = false;
    let simSpeedMult = 1;
    // Velocidad visual en m/s reales a 1× (~250 m/s -> recorre ~7.5 km en 30 s a 1×, muy fluido a 60 FPS)
    const VISUAL_DRIFT_MS_AT_1X = 250;
    let activeDrift = null;
    let savedVelocity = { velEastMs: 0.03, velNorthMs: 0.0, dirCos: 1.0, dirSin: 0.0, motion: 'approaching', spec: null };

    window.addEventListener('sim-speed-change', event => {
        simSpeedMult = event.detail?.multiplier ?? 1;
    });

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
        // Velocidad simulada coherente con VISUAL_DRIFT_MS_AT_1X / BASE_PLAY_SPEED (250 / 600 ≈ 0.42 m/s)
        // para que el cono predictivo en C anticipe hacia dónde avanza el frente de la tormenta.
        const driftSpeed = randomBetween(0.35, 0.50);
        const dirCos = Math.cos(angle);
        const dirSin = Math.sin(angle);
        const velEastMs = Number((driftSpeed * dirCos).toFixed(4));
        const velNorthMs = Number((driftSpeed * dirSin).toFixed(4));
        const motion = velEastMs >= 0 ? 'approaching' : 'receding';
        // Multiplicador alto para que tanto el núcleo como la periferia fuercen un rodeo claro
        const severityPenalty = Number((120.0 + dangerLevel * 50.0).toFixed(1));

        return {
            center: [Number(lon.toFixed(6)), Number(lat.toFixed(6))],
            diameterM,
            windSpeedMs,
            dangerLevel,
            velEastMs,
            velNorthMs,
            dirCos,
            dirSin,
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
        cancelAnimationFrame(animFrame);
        animFrame = 0;
        activeDrift = null;
    }

  // Entry point for future backend updates.
    function updateStorm(storm) {
        if (activeDrift || currentStorm) return;
        if (storm && storm.active === undefined && !currentStorm) {
            return;
        }
        renderStorm(storm);
    }

    function getCurrentStorm() {
        return currentStorm;
    }

    function isLocalFallback() {
        return true;
    }

    function startLocalDriftFrom(spec) {
        stopDemo();
        let lastTime = performance.now();
        let lastRecalcTime = lastTime;
        let curLon = spec.center[0];
        let curLat = spec.center[1];
        const [xScale, yScale] = metresPerDegree(spec.center[1]);
        const hypot = Math.hypot(spec.velEastMs ?? 0, spec.velNorthMs ?? 0);
        const dirCos = Number.isFinite(spec.dirCos)
            ? spec.dirCos
            : (hypot > 1e-6 ? (spec.velEastMs / hypot) : 1.0);
        const dirSin = Number.isFinite(spec.dirSin)
            ? spec.dirSin
            : (hypot > 1e-6 ? (spec.velNorthMs / hypot) : 0.0);

        activeDrift = { spec, dirCos, dirSin };

        function animate(now) {
            if (isFrozen || !activeDrift) return;
            const dt = (now - lastTime) / 1000;
            lastTime = now;

            const stepMeters = dt * VISUAL_DRIFT_MS_AT_1X * simSpeedMult;
            curLon += (dirCos * stepMeters) / xScale;
            curLat += (dirSin * stepMeters) / yScale;

            // Comprobar si salió por completo de Gale Crater
            const radiusDeg = (spec.diameterM / 2) / yScale;
            if (
                curLon - radiusDeg > 137.7298 ||
                curLon + radiusDeg < 137.1224 ||
                curLat - radiusDeg > -4.2489 ||
                curLat + radiusDeg < -4.9254
            ) {
                stopDemo();
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

            // Recalcular ruta periódicamente (~cada 2.5 s en 1×, más frecuente en 4×/10×) mientras deriva
            const recalcIntervalMs = 2500 / Math.sqrt(simSpeedMult);
            if (now - lastRecalcTime >= recalcIntervalMs) {
                lastRecalcTime = now;
                window.dispatchEvent(new CustomEvent('storm-drift', { detail: currentStorm }));
            }

            animFrame = requestAnimationFrame(animate);
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
        animFrame = requestAnimationFrame(animate);
    }

    document.getElementById('start-storm').addEventListener('click', async () => {
        stopDemo();
        isFrozen = false;

        const spec = generateRandomStormSpec();
        savedVelocity = {
            velEastMs: spec.velEastMs,
            velNorthMs: spec.velNorthMs,
            dirCos: spec.dirCos,
            dirSin: spec.dirSin,
            motion: spec.motion,
            spec,
        };

        // Animamos siempre a 60 FPS en el cliente para evitar saltos de red o rebotes ("bounce")
        startLocalDriftFrom(spec);
        window.dispatchEvent(new CustomEvent('storm-change', { detail: currentStorm }));

        try {
            await fetch('/api/storm', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(spec),
            });
        } catch (_err) {
            // Ignorar si el backend no tiene /api/storm activo
        }
    });

    if (freezeBtn) {
        freezeBtn.addEventListener('click', async () => {
            if (!currentStorm) return;

            isFrozen = !isFrozen;

            if (isFrozen) {
                stopDemo();
                renderStorm({
                    ...currentStorm,
                    frozen: true,
                    velEastMs: 0.0,
                    velNorthMs: 0.0,
                    motion: 'stationary',
                    simulated: true,
                });
                window.dispatchEvent(new CustomEvent('storm-change', { detail: currentStorm }));
                try {
                    await fetch('/api/storm', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ action: 'freeze' }),
                    });
                } catch (_e) {
                    // Fallback local
                }
            } else {
                // Reanudar movimiento a 60 FPS desde la posición exacta donde estaba congelada
                const resumeSpec = {
                    ...(savedVelocity.spec ?? currentStorm),
                    center: [...currentStorm.center],
                    velEastMs: savedVelocity.velEastMs,
                    velNorthMs: savedVelocity.velNorthMs,
                    dirCos: savedVelocity.dirCos,
                    dirSin: savedVelocity.dirSin,
                    motion: savedVelocity.motion,
                };
                savedVelocity.spec = resumeSpec;

                startLocalDriftFrom(resumeSpec);
                window.dispatchEvent(new CustomEvent('storm-change', { detail: currentStorm }));

                try {
                    await fetch('/api/storm', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ action: 'unfreeze' }),
                    });
                } catch (_e) {
                    // Fallback local
                }
            }
        });
    }

    // Permitir arrastrar la tormenta con el ratón a cualquier punto del mapa
    const translateStorm = new Translate({
        filter: feature => feature === footprint,
    });
    map.addInteraction(translateStorm);

    translateStorm.on('translatestart', () => {
        stopDemo();
        map.getTargetElement().style.cursor = 'grabbing';
    });

    translateStorm.on('translateend', async () => {
        map.getTargetElement().style.cursor = '';
        const geom = footprint.getGeometry();
        if (!geom || !currentStorm) return;

        const [rawLon, rawLat] = getCenter(geom.getExtent());
        const clampedLon = Number(Math.min(137.70, Math.max(137.15, rawLon)).toFixed(6));
        const clampedLat = Number(Math.min(-4.28, Math.max(-4.90, rawLat)).toFixed(6));
        const newCenter = [clampedLon, clampedLat];

        const movedSpec = {
            ...(savedVelocity.spec ?? currentStorm),
            center: newCenter,
            diameterM: currentStorm.diameterM,
            windSpeedMs: currentStorm.windSpeedMs,
            dangerLevel: currentStorm.dangerLevel,
            velEastMs: isFrozen ? 0.0 : savedVelocity.velEastMs,
            velNorthMs: isFrozen ? 0.0 : savedVelocity.velNorthMs,
            dirCos: savedVelocity.dirCos,
            dirSin: savedVelocity.dirSin,
            motion: isFrozen ? 'stationary' : savedVelocity.motion,
            severityPenalty: currentStorm.severityPenalty,
            frozen: isFrozen,
            simulated: true,
        };
        savedVelocity.spec = {
            ...movedSpec,
            velEastMs: savedVelocity.velEastMs,
            velNorthMs: savedVelocity.velNorthMs,
            dirCos: savedVelocity.dirCos,
            dirSin: savedVelocity.dirSin,
            motion: savedVelocity.motion,
        };

        if (isFrozen) {
            renderStorm({ active: true, ...movedSpec });
        } else {
            startLocalDriftFrom(savedVelocity.spec);
        }

        window.dispatchEvent(new CustomEvent('storm-change', { detail: currentStorm }));

        try {
            await fetch('/api/storm', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(movedSpec),
            });
        } catch (_e) {
            // Fallback local
        }
    });

    document.getElementById('stop-storm').addEventListener('click', async () => {
        stopDemo();
        isFrozen = false;
        renderStorm(null);
        window.dispatchEvent(new CustomEvent('storm-change', { detail: null }));
        try {
            await fetch('/api/storm', { method: 'DELETE' });
        } catch (_e) {
            // Ignore offline error
        }
    });

  // Prevent duplicate demo timers after Vite hot reloads.
    if (import.meta.hot) {
        import.meta.hot.dispose(stopDemo);
    }

    return { updateStorm, getCurrentStorm, isLocalFallback };
}