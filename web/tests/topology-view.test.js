import assert from 'node:assert/strict';
import test from 'node:test';

import cytoscape from 'cytoscape';

import { syncCytoscapeElements } from '../src/topology-view.js';

test('sync updates data without replacing nodes or changing positions', () => {
  const cy = cytoscape({
    headless: true,
    elements: [
      { group: 'nodes', data: { id: 'peer-a', stale: false }, position: { x: 40, y: 60 } },
      { group: 'nodes', data: { id: 'peer-b', stale: false }, position: { x: 140, y: 60 } },
      { group: 'edges', data: { id: 'edge-a-b', source: 'peer-a', target: 'peer-b', stale: false } }
    ],
    layout: { name: 'preset' }
  });
  const peerA = cy.getElementById('peer-a');
  const position = { ...peerA.position() };

  const structureChanged = syncCytoscapeElements(cy, [
    { group: 'nodes', data: { id: 'peer-a', stale: true } },
    { group: 'nodes', data: { id: 'peer-b', stale: false } },
    { group: 'edges', data: { id: 'edge-a-b', source: 'peer-a', target: 'peer-b', stale: true } }
  ]);

  assert.equal(structureChanged, false);
  assert.equal(cy.getElementById('peer-a')[0], peerA[0]);
  assert.deepEqual(cy.getElementById('peer-a').position(), position);
  assert.equal(cy.getElementById('peer-a').data('stale'), true);
  assert.equal(cy.getElementById('edge-a-b').data('stale'), true);
});

test('sync reports structural additions and removals', () => {
  const cy = cytoscape({
    headless: true,
    elements: [{ group: 'nodes', data: { id: 'peer-a' } }]
  });

  const structureChanged = syncCytoscapeElements(cy, [
    { group: 'nodes', data: { id: 'peer-b' } }
  ]);

  assert.equal(structureChanged, true);
  assert.equal(cy.getElementById('peer-a').length, 0);
  assert.equal(cy.getElementById('peer-b').length, 1);
});
