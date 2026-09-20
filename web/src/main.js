import { PduTopologySource } from './pdu-topology-source.js';
import { TopologyView } from './topology-view.js';

const elements = {
  graph: document.querySelector('#topology'),
  details: document.querySelector('#details'),
  status: document.querySelector('#status'),
  nodeCount: document.querySelector('#node-count'),
  transportCount: document.querySelector('#transport-count'),
  linkCount: document.querySelector('#link-count'),
  schemaName: document.querySelector('#schema-name'),
  timestamp: document.querySelector('#timestamp'),
  wsUri: document.querySelector('#ws-uri'),
  connectButton: document.querySelector('#connect-button'),
  sampleButton: document.querySelector('#sample-button'),
  fitButton: document.querySelector('#fit-button')
};

const view = new TopologyView(elements.graph, elements.details);
const source = new PduTopologySource({
  onSnapshot: renderSnapshot,
  onStatus: setStatus
});

async function loadSample() {
  setStatus('loading sample');
  try {
    const response = await fetch('/sample-topology.json', { cache: 'no-store' });
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
    const snapshot = await response.json();
    renderSnapshot(snapshot);
    setStatus('sample');
  } catch (error) {
    setStatus('error');
    elements.details.textContent = String(error);
  }
}

async function connectLive() {
  try {
    await source.connect(elements.wsUri.value.trim());
  } catch (error) {
    elements.details.textContent = String(error);
  }
}

function renderSnapshot(snapshot) {
  const topology = view.render(snapshot);
  elements.nodeCount.textContent = String(topology.nodes.length);
  elements.transportCount.textContent = String(topology.transports.length);
  elements.linkCount.textContent = String(topology.links.length);
  elements.schemaName.textContent = `schema: ${topology.schema}`;
  elements.timestamp.textContent = `timestamp: ${topology.timestamp || '-'}`;
}

function setStatus(text) {
  elements.status.textContent = text;
}

elements.connectButton.addEventListener('click', connectLive);
elements.sampleButton.addEventListener('click', async () => {
  await source.disconnect();
  await loadSample();
});
elements.fitButton.addEventListener('click', () => view.fit());

loadSample();
