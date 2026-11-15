import Feature from 'ol/Feature.js';
import Point from 'ol/geom/Point.js';
import VectorLayer from 'ol/layer/Vector.js';
import VectorSource from 'ol/source/Vector.js';
import { Circle, Fill, Stroke, Style, Text } from 'ol/style.js';
import Icon from 'ol/style/Icon.js';

const UPDATE_MS = 2000;

export function addRover(map) {
    const status = document.getElementById('rover-status');
    const travelModeSelect = document.getElementById('travel-mode');
    const marker = new Feature();

    const roverIcon = new Icon({
        src: `${import.meta.env.BASE_URL}icons/rover.svg`,
        anchor: [0.5, 0.5],
    });

    const evaIcon = new Icon({
        src: `${import.meta.env.BASE_URL}icons/eva-walk.svg`,
        anchor: [0.5, 0.5],
    });

    let currentMode = travelModeSelect?.value === 'eva' ? 'eva' : 'rover';

    const label = new Text({
        text: currentMode === 'eva' ? 'Astronaut' : 'Rover',
        font: '600 11px system-ui',
        fill: new Fill({ color: '#ffffff' }),
        stroke: new Stroke({ color: '#07121a', width: 3 }),
    });

    const roverStyle = new Style({
        image: currentMode === 'eva' ? evaIcon : roverIcon,
        text: label,
    });

    function setMode(mode) {
        currentMode = mode === 'eva' ? 'eva' : 'rover';
        label.setText(currentMode === 'eva' ? 'Astronaut' : 'Rover');
        roverStyle.setImage(currentMode === 'eva' ? evaIcon : roverIcon);
        marker.changed();
    }

    if (travelModeSelect) {
        travelModeSelect.addEventListener('change', () => {
            setMode(travelModeSelect.value);
        });
    }

// Use the initial overview as the reference zoom.
    const overviewResolution = map.getView().getResolution();

    marker.setStyle((_feature, resolution) => {
        const zoomFactor = Math.sqrt(overviewResolution / resolution);
        const sizePx = Math.max(14, Math.min(26, 17 * zoomFactor));

        const activeIcon = currentMode === 'eva' ? evaIcon : roverIcon;
        activeIcon.setScale(sizePx / 32); // The SVG is 32 px wide.
        label.setOffsetY(-(sizePx / 2 + 8));

        return roverStyle;
    });

    const layer = new VectorLayer({
        source: new VectorSource({ features: [marker], wrapX: false }),
        zIndex: 25,
    });
    map.addLayer(layer);

    let frame = 0;
    let speedMultiplier = 1;
    let isPaused = false;
    let isRoverActive = false;
    let currentElapsed = 0;
    let activeSamples = null;
    let activeSegment = 0;
    let activeStartHour = 0;
    let lastNow = 0;
    // 600 s simulados por segundo real en 1× (10 min solares/s -> 1 h de ruta cada 6 s reales)
    const BASE_PLAY_SPEED = 600;

    window.addEventListener('sim-speed-change', event => {
        speedMultiplier = event.detail?.multiplier ?? 1;
    });

    window.addEventListener('sim-play-toggle', event => {
        if (!isRoverActive) return;
        isPaused = !event.detail?.playing;
        lastNow = performance.now();
        status.textContent = isPaused
            ? 'Route paused'
            : `Route playback · ${speedMultiplier}×`;
    });

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
        currentElapsed = 0;
        activeSamples = null;
        activeSegment = 0;
        if (isRoverActive) {
            isRoverActive = false;
            isPaused = false;
            window.dispatchEvent(new CustomEvent('rover-time-step', {
                detail: { active: false, arrived: false },
            }));
        }

        if (!position) {
            marker.setGeometry(undefined);
            status.textContent = 'Waiting for origin';
            return;
        }

        place(position);
        status.textContent = 'At origin';
    }

    function getState() {
        const coords = marker.getGeometry()?.getCoordinates();
        return {
            active: isRoverActive,
            position: coords ? [coords[0], coords[1]] : null,
            elapsed: currentElapsed,
            localHour: (((activeStartHour + currentElapsed / 3600) % 24) + 24) % 24,
        };
    }

    // Empalma una nueva ruta desde la posición viva actual del rover hasta B sin teletransportarlo jamás
    function spliceRoute(newFutureSamples) {
        if (
            !isRoverActive ||
            !Array.isArray(activeSamples) ||
            !Array.isArray(newFutureSamples) ||
            newFutureSamples.length < 2
        ) {
            return null;
        }

        const curPos = marker.getGeometry()?.getCoordinates();
        if (!curPos) return null;

        const traversed = [];
        for (let i = 0; i < activeSamples.length; i++) {
            if (activeSamples[i][3] < currentElapsed) {
                traversed.push(activeSamples[i]);
            } else {
                break;
            }
        }

        const refSample = activeSamples[Math.min(activeSegment, activeSamples.length - 1)] ?? [0, 0, 0, 0, 0, 0];
        traversed.push([
            curPos[0],
            curPos[1],
            refSample[2] ?? 0,
            currentElapsed,
            refSample[4] ?? 0,
            refSample[5] ?? 0,
        ]);

        // Saltamos los primeros puntos que hayan quedado muy pegados durante los ~150 ms de cálculo
        let startIdx = 1;
        while (
            startIdx + 1 < newFutureSamples.length &&
            newFutureSamples[startIdx][3] < 25
        ) {
            startIdx++;
        }

        const futureShifted = [];
        let prevTime = currentElapsed;
        for (let i = startIdx; i < newFutureSamples.length; i++) {
            const pt = newFutureSamples[i];
            const rawTargetTime = currentElapsed + Math.max(25, pt[3]);
            const nextTime = Math.max(prevTime + 5, rawTargetTime);
            prevTime = nextTime;
            futureShifted.push([
                pt[0],
                pt[1],
                pt[2] ?? 0,
                nextTime,
                pt[4] ?? 0,
                pt[5] ?? 0,
            ]);
        }

        activeSamples = [...traversed, ...futureShifted];
        activeSegment = traversed.length - 1;

        return activeSamples.map(pt => [pt[0], pt[1]]);
    }

    function play(samples, departureHour = 0, speed = 1) {
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

        activeStartHour = Number.isFinite(departureHour) ? departureHour : 0;
        activeSamples = samples.map(pt => [...pt]);
        activeSegment = 0;
        currentElapsed = 0;
        lastNow = performance.now();
        isPaused = false;
        isRoverActive = true;

        place(activeSamples[0].slice(0, 2));
        status.textContent = `Route playback · ${speed * speedMultiplier}×`;

        window.dispatchEvent(new CustomEvent('rover-time-step', {
            detail: {
                localHour: ((activeStartHour % 24) + 24) % 24,
                active: true,
                arrived: false,
            },
        }));

        function animate(now) {
            const dt = (now - lastNow) / 1000;
            lastNow = now;

            if (!activeSamples || activeSamples.length < 2) return;
            const last = activeSamples[activeSamples.length - 1];

            if (!isPaused) {
                currentElapsed += dt * speed * BASE_PLAY_SPEED * speedMultiplier;
                status.textContent = `Route playback · ${speed * speedMultiplier}×`;
            }

            if (currentElapsed >= last[3]) {
                isRoverActive = false;
                isPaused = false;
                currentElapsed = last[3];
                const arrivalHour = (((activeStartHour + last[3] / 3600) % 24) + 24) % 24;
                place(last.slice(0, 2));
                status.textContent = 'Arrived at destination';
                window.dispatchEvent(new CustomEvent('rover-time-step', {
                    detail: {
                        localHour: arrivalHour,
                        active: false,
                        arrived: true,
                    },
                }));
                return;
            }

            const currentHour = (((activeStartHour + currentElapsed / 3600) % 24) + 24) % 24;
            window.dispatchEvent(new CustomEvent('rover-time-step', {
                detail: {
                    localHour: currentHour,
                    active: true,
                    arrived: false,
                },
            }));

            // Advance past every waypoint already reached, including equal timestamps.
            while (
                activeSegment + 1 < activeSamples.length &&
                activeSamples[activeSegment + 1][3] <= currentElapsed
            ) {
                activeSegment++;
            }

            const a = activeSamples[activeSegment];
            const b = activeSamples[activeSegment + 1];
            const span = b[3] - a[3];
            const fraction = currentElapsed <= a[3] || span <= 0
                ? 0
                : (currentElapsed - a[3]) / span;

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
    return { setOrigin, play, getState, spliceRoute };
}
