import assert from 'node:assert/strict';
import test from 'node:test';

import { normalizeTopology, toCytoscapeElements } from '../src/topology-model.js';

const snapshot = {
  schema: 'hakoniwa.zenoh.topology/v1',
  timestamp: 1,
  collector: { zid: 'collector' },
  nodes: [{ zid: 'collector', mode: 'peer' }],
  transports: [
    {
      remote_zid: 'router',
      remote_mode: 'router',
      qos: true,
      multicast: false
    }
  ],
  links: [
    {
      remote_zid: 'router',
      protocol: 'tcp',
      src_endpoint: 'tcp/127.0.0.1:50000',
      dst_endpoint: 'tcp/127.0.0.1:7447'
    }
  ]
};

test('normalizeTopology derives missing remote node from transport/link data', () => {
  const topology = normalizeTopology(snapshot);
  assert.equal(topology.nodes.length, 2);
  assert.deepEqual(
    topology.nodes.find((node) => node.zid === 'router'),
    { zid: 'router', mode: 'router' }
  );
});

test('toCytoscapeElements creates nodes and a link edge', () => {
  const elements = toCytoscapeElements(snapshot);
  const nodes = elements.filter((element) => element.group === 'nodes');
  const edges = elements.filter((element) => element.group === 'edges');

  assert.equal(nodes.length, 2);
  assert.equal(edges.length, 1);
  assert.equal(edges[0].data.source, 'collector');
  assert.equal(edges[0].data.target, 'router');
  assert.equal(edges[0].data.label, 'tcp');
});

test('unsupported schema is rejected', () => {
  assert.throws(
    () => normalizeTopology({ schema: 'unknown', nodes: [], transports: [], links: [] }),
    /Unsupported topology schema/
  );
});
