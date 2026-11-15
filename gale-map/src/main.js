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
  status.textContent = 'Drag to explore · Scroll to zoom';
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
    : 'Drag to explore · Scroll to zoom';
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
addRoutePlanner(map, galeExtent);
addStormOverlay(map);

// const storms = addStormOverlay(map);  (Unirlo cuando esté el backend)

let stormTimer;
let stopped = false;
let stormRequest;

async function refreshStorm() {
  stormRequest = new AbortController();

  try {
    const response = await fetch('/api/storm', {
      signal: stormRequest.signal,
    });

    if (!response.ok) {
      throw new Error(`Storm API returned HTTP ${response.status}`);
    }

    const data = await response.json();
    if (stopped) return;

    storms.updateStorm(data);
  } catch (error) {
    if (stopped) return;

    console.error(error);

    const alert = document.getElementById('storm-alert');
    alert.hidden = false;
    alert.style.setProperty('--storm-color', '#aeb7b2');

    document.getElementById('storm-title').textContent =
      'Storm updates unavailable';

    document.getElementById('storm-details').textContent =
      'Any storm shown is the last received position.';
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

addRover(map, { demo: true });
addSolarAlert();