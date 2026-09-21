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

test('v2 aggregated topology uses each observation source as the edge source', () => {
  const snapshot = {
    schema: 'hakoniwa.zenoh.topology/v2',
    timestamp: 1,
    collector: { zid: '' },
    nodes: [
      { zid: 'router-r', mode: 'router' },
      { zid: 'peer-a', mode: 'peer' },
      { zid: 'peer-b', mode: 'peer' }
    ],
    transports: [],
    links: [
      { source_zid: 'router-r', remote_zid: 'peer-a', protocol: 'tcp' },
      { source_zid: 'router-r', remote_zid: 'peer-b', protocol: 'tcp' }
    ]
  };

  const edges = toCytoscapeElements(snapshot).filter((element) => element.group === 'edges');
  assert.equal(edges.length, 2);
  assert.equal(edges[0].data.source, 'router-r');
  assert.equal(edges[0].data.target, 'peer-a');
  assert.equal(edges[1].data.source, 'router-r');
  assert.equal(edges[1].data.target, 'peer-b');
});

test('v2 stale source marks its node and links as stale', () => {
  const snapshot = {
    schema: 'hakoniwa.zenoh.topology/v2',
    status: 'partial',
    nodes: [
      { zid: 'router-r', mode: 'router' },
      { zid: 'peer-a', mode: 'peer' }
    ],
    transports: [],
    links: [
      { source_zid: 'peer-a', remote_zid: 'router-r', protocol: 'tcp' }
    ],
    sources: [
      { name: 'node-a', role: 'peer', endpoint: 'node-a.json', zid: 'peer-a', status: 'stale' }
    ]
  };

  const topology = normalizeTopology(snapshot);
  assert.equal(topology.status, 'partial');
  assert.equal(topology.sources[0].status, 'stale');

  const elements = toCytoscapeElements(snapshot);
  const peerNode = elements.find((item) => item.group === 'nodes' && item.data.id === 'peer-a');
  assert.equal(peerNode.data.stale, true);
  assert.equal(peerNode.data.label, 'node-a');
  assert.equal(peerNode.data.zid, 'peer-a');
  assert.equal(elements.find((item) => item.group === 'edges').data.stale, true);
});

test('link remains fresh when at least one observer is fresh', () => {
  const snapshot = {
    schema: 'hakoniwa.zenoh.topology/v2',
    status: 'partial',
    nodes: [
      { zid: 'router-r', mode: 'router' },
      { zid: 'peer-a', mode: 'peer' }
    ],
    transports: [],
    links: [
      {
        source_zid: 'peer-a',
        remote_zid: 'router-r',
        protocol: 'tcp',
        observed_by: ['peer-a', 'router-r']
      }
    ],
    sources: [
      { name: 'node-a', zid: 'peer-a', status: 'stale' },
      { name: 'router', zid: 'router-r', status: 'ok' }
    ]
  };

  const elements = toCytoscapeElements(snapshot);
  assert.equal(elements.find((item) => item.group === 'edges').data.stale, false);
});

test('duplicate agent names remain distinct and include shortened ZIDs', () => {
  const duplicateNames = {
    schema: 'hakoniwa.zenoh.topology/v2',
    nodes: [
      { zid: '1234567890abcdef', mode: 'peer' },
      { zid: 'abcdef1234567890', mode: 'peer' }
    ],
    transports: [],
    links: [],
    sources: [
      { name: 'sensor', zid: '1234567890abcdef', status: 'ok' },
      { name: 'sensor', zid: 'abcdef1234567890', status: 'ok' }
    ]
  };

  const nodes = toCytoscapeElements(duplicateNames)
    .filter((element) => element.group === 'nodes');
  assert.equal(nodes.length, 2);
  assert.deepEqual(
    nodes.map((node) => node.data.label).sort(),
    ['sensor (123456…cdef)', 'sensor (abcdef…7890)']
  );
});

test('link IDs stay stable when snapshot order changes', () => {
  const first = {
    schema: 'hakoniwa.zenoh.topology/v2',
    nodes: [
      { zid: 'peer-a', mode: 'peer' },
      { zid: 'peer-b', mode: 'peer' },
      { zid: 'router-r', mode: 'router' }
    ],
    transports: [],
    links: [
      { source_zid: 'peer-a', remote_zid: 'router-r', protocol: 'tcp', src_endpoint: 'a:1' },
      { source_zid: 'peer-b', remote_zid: 'router-r', protocol: 'tcp', src_endpoint: 'b:1' }
    ]
  };
  const second = { ...first, links: [...first.links].reverse() };

  const edgeIds = (value) => toCytoscapeElements(value)
    .filter((element) => element.group === 'edges')
    .map((element) => element.data.id)
    .sort();

  assert.deepEqual(edgeIds(first), edgeIds(second));
});
