export function normalizeTopology(snapshot) {
  if (!snapshot || typeof snapshot !== 'object') {
    throw new Error('Topology snapshot must be an object');
  }
  if (snapshot.schema !== 'hakoniwa.zenoh.topology/v1'
      && snapshot.schema !== 'hakoniwa.zenoh.topology/v2') {
    throw new Error(`Unsupported topology schema: ${snapshot.schema ?? '(missing)'}`);
  }

  const collectorZid = snapshot.collector?.zid ?? '';
  const nodes = Array.isArray(snapshot.nodes) ? snapshot.nodes : [];
  const transports = Array.isArray(snapshot.transports) ? snapshot.transports : [];
  const links = Array.isArray(snapshot.links) ? snapshot.links : [];

  const nodeByZid = new Map();

  function ensureNode(zid, mode = 'unknown') {
    if (!zid) {
      return;
    }
    const existing = nodeByZid.get(zid);
    if (!existing) {
      nodeByZid.set(zid, { zid, mode: mode || 'unknown' });
      return;
    }
    if ((existing.mode === 'unknown' || !existing.mode) && mode && mode !== 'unknown') {
      existing.mode = mode;
    }
  }

  for (const node of nodes) {
    ensureNode(node.zid, node.mode);
  }
  ensureNode(collectorZid, nodeByZid.get(collectorZid)?.mode ?? 'collector');

  for (const transport of transports) {
    ensureNode(transport.source_zid || collectorZid);
    ensureNode(transport.remote_zid, transport.remote_mode);
  }
  for (const link of links) {
    ensureNode(link.source_zid || collectorZid);
    ensureNode(link.remote_zid);
  }

  return {
    schema: snapshot.schema,
    timestamp: snapshot.timestamp ?? 0,
    collectorZid,
    nodes: [...nodeByZid.values()],
    transports,
    links
  };
}

export function toCytoscapeElements(snapshot) {
  const topology = normalizeTopology(snapshot);

  const nodeElements = topology.nodes.map((node) => ({
    group: 'nodes',
    data: {
      id: node.zid,
      label: shortZid(node.zid),
      zid: node.zid,
      mode: node.mode || 'unknown',
      collector: node.zid === topology.collectorZid
    }
  }));

  const edgeElements = [];
  const edgeSource = topology.links.length > 0 ? topology.links : topology.transports;

  edgeSource.forEach((item, index) => {
    const source = item.source_zid || topology.collectorZid;
    const target = item.remote_zid;
    if (!source || !target) {
      return;
    }

    const protocol = item.protocol || '';
    const id = `edge-${index}-${source}-${target}`;
    edgeElements.push({
      group: 'edges',
      data: {
        id,
        source,
        target,
        label: protocol,
        kind: topology.links.length > 0 ? 'link' : 'transport',
        raw: item
      }
    });
  });

  return [...nodeElements, ...edgeElements];
}

function shortZid(zid) {
  if (zid.length <= 12) {
    return zid;
  }
  return `${zid.slice(0, 6)}…${zid.slice(-4)}`;
}
