import cytoscape from 'cytoscape';
import { normalizeTopology, toCytoscapeElements } from './topology-model.js';

export class TopologyView {
  constructor(container, detailsElement) {
    this.detailsElement = detailsElement;
    this.cy = cytoscape({
      container,
      elements: [],
      layout: { name: 'cose', animate: false },
      style: [
        {
          selector: 'node',
          style: {
            'background-color': '#667085',
            'label': 'data(label)',
            'color': '#172033',
            'font-size': 11,
            'text-valign': 'bottom',
            'text-margin-y': 8,
            'width': 42,
            'height': 42
          }
        },
        {
          selector: 'node[mode = "peer"]',
          style: { 'background-color': '#3b82f6' }
        },
        {
          selector: 'node[mode = "router"]',
          style: { 'background-color': '#f59e0b', 'shape': 'diamond' }
        },
        {
          selector: 'node[mode = "client"]',
          style: { 'background-color': '#10b981', 'shape': 'round-rectangle' }
        },
        {
          selector: 'node[collector]',
          style: {
            'border-width': 4,
            'border-color': '#111827'
          }
        },
        {
          selector: 'node[?stale]',
          style: {
            'opacity': 0.55,
            'border-width': 3,
            'border-color': '#dc2626'
          }
        },
        {
          selector: 'edge',
          style: {
            'curve-style': 'bezier',
            'width': 2,
            'line-color': '#98a2b3',
            'target-arrow-shape': 'triangle',
            'target-arrow-color': '#98a2b3',
            'label': 'data(label)',
            'font-size': 10,
            'text-background-color': '#ffffff',
            'text-background-opacity': 0.9,
            'text-background-padding': 2
          }
        },
        {
          selector: 'edge[?stale]',
          style: {
            'line-style': 'dashed',
            'line-color': '#dc2626',
            'target-arrow-color': '#dc2626',
            'opacity': 0.6
          }
        },
        {
          selector: ':selected',
          style: {
            'border-width': 4,
            'border-color': '#7c3aed',
            'line-color': '#7c3aed',
            'target-arrow-color': '#7c3aed'
          }
        }
      ]
    });

    this.cy.on('tap', 'node, edge', (event) => {
      const element = event.target;
      this.showDetails(element.data());
    });
  }

  render(snapshot) {
    const topology = normalizeTopology(snapshot);
    this.cy.elements().remove();
    this.cy.add(toCytoscapeElements(snapshot));
    this.cy.layout({
      name: 'cose',
      animate: false,
      fit: true,
      padding: 40
    }).run();
    this.showDetails({
      schema: topology.schema,
      collector_zid: topology.collectorZid,
      nodes: topology.nodes.length,
      transports: topology.transports.length,
      links: topology.links.length,
      status: topology.status,
      sources: topology.sources
    });
    return topology;
  }

  fit() {
    this.cy.fit(undefined, 40);
  }

  showDetails(value) {
    if (!this.detailsElement) {
      return;
    }
    this.detailsElement.textContent = JSON.stringify(value, null, 2);
  }
}
