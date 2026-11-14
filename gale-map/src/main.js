import 'ol/ol.css';
import './style.css';

import Map from 'ol/Map.js';
import View from 'ol/View.js';
import GeoTIFF from 'ol/source/GeoTIFF.js';
import WebGLTileLayer from 'ol/layer/WebGLTile.js';

const status = document.getElementById('status');
const fileInput = document.getElementById('file');

const map = new Map({
  target: 'map',
  layers: [],
  view: new View({
    center: [0, 0],
    zoom: 1,
  }),
});

let currentLayer;
let loadNumber = 0;

fileInput.addEventListener('change', async () => {
  const file = fileInput.files[0];

  if (!file) return;

  const thisLoad = ++loadNumber;
  status.textContent = 	'Reading GeoTIFF metadata…';

  try {
    const source = new GeoTIFF({
      sources: [{ blob: file }],
    });

    const viewOptions = await source.getView();

    if (thisLoad !== loadNumber) return;

    if (!viewOptions.projection) {
      throw new Error(
        'The Mars coordinate reference system must be defined from the file metadata.'
      );
    }

    const nextLayer = new WebGLTileLayer({ source });

    source.on('tileloaderror', () => {
      if (thisLoad === loadNumber) {
        status.textContent =
          'Error al leer la imagen. Revisa la consola del navegador.';
      }
    });

    if (currentLayer) {
      map.removeLayer(currentLayer);
      currentLayer.dispose();
    }

    currentLayer = nextLayer;

    map.setView(
      new View({
        ...viewOptions,
        showFullExtent: true,
      })
    );

    map.addLayer(currentLayer);

    status.textContent =
      'Loading imagery';

    console.log('Configuración del archivo', viewOptions);
  } catch (error) {
    if (thisLoad !== loadNumber) return;

    console.error(error);
    status.textContent = `Unable to open the map. ${error.message}`;
  }
});
