import {
  PduEncoding,
  PduManager,
  WebSocketCommunicationService
} from 'hakoniwa-pdu-javascript';

const ROBOT = 'ZenohTopology';
const PDU = 'topology';

export class PduTopologySource {
  constructor({
    configPath = '/pdu/pdudef.json',
    pollIntervalMs = 200,
    onSnapshot,
    onStatus
  } = {}) {
    this.configPath = configPath;
    this.pollIntervalMs = pollIntervalMs;
    this.onSnapshot = onSnapshot ?? (() => {});
    this.onStatus = onStatus ?? (() => {});
    this.manager = null;
    this.timer = null;
    this.lastJson = null;
  }

  async connect(uri) {
    await this.disconnect();

    this.onStatus('connecting');
    const manager = new PduManager({
      wire_version: 'v2',
      pdu_encoding: PduEncoding.CDR
    });
    const transport = new WebSocketCommunicationService('v2');

    await manager.initialize(this.configPath, transport);
    if (!await manager.start_service(uri)) {
      this.onStatus('error');
      throw new Error(`Failed to connect to ${uri}`);
    }

    if (!await manager.declare_pdu_for_read(ROBOT, PDU)) {
      await manager.stop_service();
      this.onStatus('error');
      throw new Error('Failed to declare ZenohTopology/topology for read');
    }

    this.manager = manager;
    this.onStatus('live');

    this.timer = window.setInterval(() => {
      this.poll().catch((error) => {
        console.error(error);
        this.onStatus('error');
      });
    }, this.pollIntervalMs);

    await this.poll();
  }

  async poll() {
    if (!this.manager?.is_service_enabled()) {
      return;
    }

    const raw = this.manager.read_pdu_raw_data(ROBOT, PDU);
    if (!raw) {
      return;
    }

    const value = await this.manager.pdu_convertor.convert_binary_to_json(ROBOT, PDU, raw);
    if (!value || typeof value.data !== 'string' || value.data === this.lastJson) {
      return;
    }

    this.lastJson = value.data;
    this.onSnapshot(JSON.parse(value.data));
  }

  async disconnect() {
    if (this.timer !== null) {
      window.clearInterval(this.timer);
      this.timer = null;
    }
    if (this.manager) {
      await this.manager.stop_service();
      this.manager = null;
    }
    this.lastJson = null;
  }
}
