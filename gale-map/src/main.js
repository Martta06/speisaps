import '@astrouxds/astro-web-components/dist/astro-web-components/astro-web-components.css';
import 'ol/ol.css';
import './style.css';


import Map from 'ol/Map.js';
import View from 'ol/View.js';
import ImageLayer from 'ol/layer/Image.js';
import ImageStatic from 'ol/source/ImageStatic.js';
import Projection from 'ol/proj/Projection.js';
import { addEnvironmentPanel } from './environment.js';
import { addRoutePlanner } from './route.js';
import { addStormOverlay } from './storm.js';
import { addRover } from './rover.js';
import { addSolarAlert } from './solar.js';

// DEM bounds in Mars 2000 coordinates: west, south, east, north.
const galeExtent = [
  137.122471132, -4.925474312,
  137.729812907, -4.248963057,
];

const marsProjection = new Projection({
  code: 'ESRI:104905',
  units: 'degrees',
});

const terrain = new ImageStatic({
  url: `${import.meta.env.BASE_URL}data/gale-terrain.png`,
  projection: marsProjection,
  imageExtent: galeExtent,
});

const status = document.getElementById('status');

terrain.on('imageloadend', () => {
  status.textContent = '';
});

terrain.on('imageloaderror', () => {
  status.textContent = 'Unable to load the terrain image.';
});

const view = new View({
  projection: marsProjection,
  extent: galeExtent,
  showFullExtent: true,
});

const map = new Map({
  target: 'map',
  layers: [new ImageLayer({ source: terrain })],
  view,
});

view.fit(galeExtent, { size: map.getSize() });

const styleSelector = document.getElementById('map-style');

const elevationSource = new ImageStatic({
  url: `${import.meta.env.BASE_URL}data/gale-colour.png`,
  projection: marsProjection,
  imageExtent: galeExtent,
});

const elevationLayer = new ImageLayer({
  source: elevationSource,
  visible: false,
  opacity: 0.55,
});

map.addLayer(elevationLayer);

let elevationReady = false;

styleSelector.addEventListener('change', () => {
  const showElevation = styleSelector.value === 'elevation';
  elevationLayer.setVisible(showElevation);

  status.textContent = showElevation
    ? (elevationReady ? 'Purple: lower terrain · Yellow: higher terrain' : 'Loading elevation colors…')
    : '';
});

elevationSource.on('imageloadend', () => {
  elevationReady = true;
  if (styleSelector.value === 'elevation') {
    status.textContent = 'Purple: lower terrain · Yellow: higher terrain';
  }
});

elevationSource.on('imageloaderror', () => {
  if (styleSelector.value === 'elevation') {
    elevationLayer.setVisible(false);
    styleSelector.value = 'relief';
    status.textContent = 'Elevation colors unavailable · Showing shaded relief';
  }
});

addEnvironmentPanel(map, galeExtent);
const storms = addStormOverlay(map);
const solar = addSolarAlert();
const rover = addRover(map);
addRoutePlanner(map, galeExtent, rover, {
  getSolarState: solar.getSolarState,
  getCurrentStorm: storms.getCurrentStorm,
});

let stormTimer;
let stopped = false;
let stormRequest;

async function refreshStorm() {
  if (stopped) return;

  // Solo hacemos polling al backend cuando hay una tormenta activa en movimiento gestionada por el servidor
  if (!storms.getCurrentStorm()?.active || storms.getCurrentStorm()?.frozen || storms.isLocalFallback?.()) {
    stormTimer = setTimeout(refreshStorm, 2000);
    return;
  }

  stormRequest = new AbortController();

  try {
    const response = await fetch('/api/storm', {
      signal: stormRequest.signal,
    });

    if (!response.ok) {
      throw new Error(`Storm API returned HTTP ${response.status}`);
    }

    const data = await response.json();
    if (stopped || storms.isLocalFallback?.()) return;

    const wasActive = Boolean(storms.getCurrentStorm()?.active);
    storms.updateStorm(data);
    const isActive = Boolean(storms.getCurrentStorm()?.active);
    if (wasActive && !isActive) {
      window.dispatchEvent(new CustomEvent('storm-change', { detail: null }));
    }
  } catch (error) {
    if (stopped || storms.isLocalFallback?.()) return;
    console.warn('Storm updates unavailable', error);
  } finally {
    if (!stopped) {
      stormTimer = setTimeout(refreshStorm, 2000);
    }
  }
}

refreshStorm();

if (import.meta.hot) {
  import.meta.hot.dispose(() => {
    stopped = true;
    clearTimeout(stormTimer);
    stormRequest?.abort();
  });
}


const missionLog = document.getElementById('mission-log');

function logMissionEvent(message) {
  const entry = document.createElement('li');
  const time = new Date().toISOString().slice(11, 19);
  entry.textContent = `${time}Z · ${message}`;
  missionLog.prepend(entry);
  if (missionLog.children.length > 5) missionLog.lastElementChild.remove();
}

for (const id of ['route-status', 'solar-title', 'storm-title']) {
  const element = document.getElementById(id);
  let previous = '';
  new MutationObserver(() => {
    const message = element.textContent.trim();
    if (message && message !== previous) logMissionEvent(message);
    previous = message;
  }).observe(element, { childList: true, subtree: true, characterData: true });
}

logMissionEvent('Map console initialized');