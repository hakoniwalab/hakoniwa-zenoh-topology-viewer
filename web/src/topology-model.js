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
  const sources = Array.isArray(snapshot.sources) ? snapshot.sources : [];
  const staleZids = new Set(
    sources.filter((source) => source.status === 'stale' && source.zid).map((source) => source.zid)
  );

  function isStale(item, source) {
    const observers = Array.isArray(item.observed_by) && item.observed_by.length > 0
      ? item.observed_by
      : [source];
    return observers.length > 0 && observers.every((zid) => staleZids.has(zid));
  }

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
    links,
    status: snapshot.status ?? 'complete',
    sources,
    staleZids,
    isStale
  };
}

export function toCytoscapeElements(snapshot) {
  const topology = normalizeTopology(snapshot);
  const sourceNameByZid = new Map(
    topology.sources
      .filter((source) => source.zid && source.name)
      .map((source) => [source.zid, source.name])
  );
  const sourceNameCounts = new Map();
  for (const name of sourceNameByZid.values()) {
    sourceNameCounts.set(name, (sourceNameCounts.get(name) ?? 0) + 1);
  }

  const nodeElements = topology.nodes.map((node) => {
    const name = sourceNameByZid.get(node.zid) ?? '';
    const label = name
      ? (sourceNameCounts.get(name) > 1 ? `${name} (${shortZid(node.zid)})` : name)
      : shortZid(node.zid);
    return {
      group: 'nodes',
      data: {
        id: node.zid,
        label,
        name,
        zid: node.zid,
        mode: node.mode || 'unknown',
        collector: node.zid === topology.collectorZid,
        stale: topology.staleZids.has(node.zid)
      }
    };
  });

  const edgeElements = [];
  const edgeSource = topology.links.length > 0 ? topology.links : topology.transports;
  const edgeIdOccurrences = new Map();

  edgeSource.forEach((item) => {
    const source = item.source_zid || topology.collectorZid;
    const target = item.remote_zid;
    if (!source || !target) {
      return;
    }

    const protocol = item.protocol || '';
    const kind = topology.links.length > 0 ? 'link' : 'transport';
    const edgeKey = [
      kind,
      source,
      target,
      protocol,
      item.src_endpoint || '',
      item.dst_endpoint || ''
    ].map((value) => encodeURIComponent(value)).join(':');
    const occurrence = (edgeIdOccurrences.get(edgeKey) || 0) + 1;
    edgeIdOccurrences.set(edgeKey, occurrence);
    const id = `edge:${edgeKey}:${occurrence}`;
    edgeElements.push({
      group: 'edges',
      data: {
        id,
        source,
        target,
        label: protocol,
        kind,
        stale: topology.isStale(item, source),
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
