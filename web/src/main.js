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
  fitButton: document.querySelector('#fit-button')
};

const view = new TopologyView(elements.graph, elements.details);
const source = new PduTopologySource({
  onSnapshot: renderSnapshot,
  onStatus: setStatus
});

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
  const staleSources = topology.sources.filter((source) => source.status === 'stale');
  if (staleSources.length > 0) {
    setStatus(`partial: stale ${staleSources.map((source) => source.name).join(', ')}`);
  } else if (topology.status === 'partial') {
    setStatus('partial');
  } else {
    setStatus('connected');
  }
}

function setStatus(text) {
  elements.status.textContent = text;
}

elements.connectButton.addEventListener('click', connectLive);
elements.fitButton.addEventListener('click', () => view.fit());
