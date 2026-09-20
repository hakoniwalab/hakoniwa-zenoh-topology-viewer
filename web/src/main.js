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
  fitButton: document.querySelector('#fit-button'),
  reloadButton: document.querySelector('#reload-button')
};

const view = new TopologyView(elements.graph, elements.details);

async function loadSample() {
  setStatus('loading');
  try {
    const response = await fetch('./sample-topology.json', { cache: 'no-store' });
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

elements.fitButton.addEventListener('click', () => view.fit());
elements.reloadButton.addEventListener('click', () => loadSample());

loadSample();

// Future PDU integration should call renderSnapshot(snapshot) after:
// 1. WebSocket receive via hakoniwa-pdu-javascript
// 2. std_msgs/String CDR decode
// 3. JSON.parse(pdu.data)
