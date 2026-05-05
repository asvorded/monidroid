import ReconnectingWebSocket from "reconnecting-websocket";

import {
  ServerMessage, Device, MessageHandlers, ProtocolMessages, UUID,
  ServerInfo, ServerState, ServerStateOptions, WS_PORT, wsProtocol, ServerResponse,
  ServerRequest,
  ClientMessage,
} from "./wsservice.types";

const WS_URL = `ws://localhost:${WS_PORT}/`;
const HTTP_URL = `http://localhost:${WS_PORT}/`;

type PendingRequest = {
  resolve: (value: any) => void;
  reject: (reason: any) => void;
};

const testDevices: Device[] = Array(5).fill(0).map((v, i) => ({
    id: 'dev_' + i,
    connectionType: i % 2 == 0 ? 'wifi' : 'usb',
    address: '192.168.1.' + i,
    name: 'azaa ' + i,
    connectedAt: Date.now()
  })
);

class ControlClient {
  private readonly rws = new ReconnectingWebSocket(WS_URL);
  private readonly pendingRequests = new Map<UUID, PendingRequest>();
  private readonly devices: Map<string, Device> = new Map<string, Device>();
  private readonly handlers: MessageHandlers = {
    [wsProtocol.CLIENT_CONNECTED]: ({ client }) => {
      const present = this.devices.has(client.id);
      this.devices.set(client.id, client);
      this.onClientConnected(client, present);
    },

    [wsProtocol.CLIENT_DISCONNECTED]: ({ id }) => {
      const removed = !this.devices.has(id);
      this.devices.delete(id);
      this.onClientDisconnected(id, removed);
    }
  };

  constructor() {
    this.rws.onopen = (e) => {
      this.onConnected();
    };
    this.rws.onclose = (e) => {
      this.pendingRequests.forEach((req, uuid) => {
        req.reject(new Error("Connection lost"));
      })
      this.pendingRequests.clear();

      this.onConnectionLost();
    }
    this.rws.onmessage = (e) => {
      const msg: ServerResponse = JSON.parse(e.data);

      if ('requestId' in msg) {
        const pending = this.pendingRequests.get(msg.requestId);
        if (pending) {
          this.pendingRequests.delete(msg.requestId);
          
          if ('error' in msg) pending.reject(new Error(msg.error));
          else pending.resolve(msg.data);
        }
      } else { 
        const h = this.handlers[msg.message];
        if (h) {
          (h as any)(msg);
        }
      }
    }
  }

  onConnectionLost = () => {};

  onConnected = () => {};

  onClientConnected = (client: Device, alreadyPresent: boolean) => {};

  onClientDisconnected = (id: string, alreadyRemoved: boolean) => {};

  private request(message: ProtocolMessages, obj?: any): Promise<any> {
    return new Promise((resolve, reject) => {
      const requestId = crypto.randomUUID();
  
      this.pendingRequests.set(requestId, { resolve, reject });
  
      this.rws.send(JSON.stringify({
        message,
        requestId,
        data: obj
      } as ServerRequest))
    });
  }
  
  async getServerInfo(): Promise<ServerInfo> {
    return await this.request(wsProtocol.SERVER_CONFIG);
  }
  
  async getAllClients(): Promise<Device[]> {
    this.devices.clear();
  
    return await this.request(wsProtocol.ALL_CLIENTS);
  }
  
  getClient(id: string): Device | undefined {
    return this.devices.get(id);
  }
  
  async setServerState(enable: boolean): Promise<ServerState> {
    const req: ServerStateOptions = { enable };
    return await this.request(wsProtocol.SERVER_STATE, req);
  }
  
  shutdown() {
    const msg: ClientMessage = { message: wsProtocol.SHUTDOWN };
    this.rws.send(JSON.stringify(msg));    
    window.controlPanel.shutdown();
  }
};

const service = new ControlClient();

export default service;
