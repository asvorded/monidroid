import ReconnectingWebSocket from "reconnecting-websocket";

import {
  ServerMessage, Device, MessageHandlers, ProtocolMessages, UUID,
  ServerInfo, ServerState, ServerStateOptions, WS_PORT, wsProtocol, ServerResponse,
  ServerRequest,
  ClientMessage,
} from "../server/websocket";

const WS_URL = `ws://localhost:${WS_PORT}/`;
const HTTP_URL = `http://localhost:${WS_PORT}/`;

type PendingRequest = {
  resolve: (value: any) => void;
  reject: (reason: any) => void;
};

const rws = new ReconnectingWebSocket(WS_URL);

const pendingRequests = new Map<UUID, PendingRequest>();

const devices: Map<string, Device> = new Map<string, Device>();

let onConnectionLost = () => {};
let onConnected = () => {};
let onClientConnected = (client: Device, alreadyPresent: boolean) => {};
let onClientDisconnected = (id: string, alreadyRemoved: boolean) => {};

const handlers: MessageHandlers = {
  [wsProtocol.CLIENT_CONNECTED]: ({ client }) => {
    const present = devices.has(client.id);
    devices.set(client.id, client);
    onClientConnected(client, present);
  },

  [wsProtocol.CLIENT_DISCONNECTED]: ({ id }) => {
    const removed = !devices.has(id);
    devices.delete(id);
    onClientDisconnected(id, removed);
  }
};

rws.onopen = (e) => {
  onConnected();
};

rws.onclose = (e) => {
  pendingRequests.forEach((req, uuid) => {
    req.reject(new Error("Connection lost"));
  })
  pendingRequests.clear();

  onConnectionLost();
}

rws.onmessage = (e) => {
  const msg: ServerResponse = JSON.parse(e.data);

  if ('requestId' in msg) {
    // const { requestId, error, data } = msg;

    const pending = pendingRequests.get(msg.requestId);
    if (pending) {
      pendingRequests.delete(msg.requestId);
      
      if ('error' in msg) pending.reject(new Error(msg.error));
      else pending.resolve(msg.data);
    }
  } else { 
    const h = handlers[msg.message];
    if (h) {
      (h as any)(msg);
    }
  }
}

async function request(message: ProtocolMessages, obj?: any) : Promise<any> {
  return new Promise((resolve, reject) => {
    const requestId = crypto.randomUUID();

    pendingRequests.set(requestId, { resolve, reject });

    rws.send(JSON.stringify({
      message,
      requestId,
      data: obj
    } as ServerRequest))
  });
}

const testDevices: Device[] = Array(5).fill(0).map((v, i) => ({
    id: 'dev_' + i,
    connectionType: i % 2 == 0 ? 'wifi' : 'usb',
    address: '192.168.1.' + i,
    name: 'azaa ' + i,
    connectedAt: Date.now()
  })
);

async function getServerInfo(): Promise<ServerInfo> {
  // return {
  //   version: "0.1.0", enabled: true, hostname: "mypc",
  //   addresses: [ "1.1.1.1", "2.2.2.2" ]
  // }

  return await request(wsProtocol.SERVER_CONFIG);
}

async function getAllClients(): Promise<Device[]> {
  devices.clear();

  // return Promise.resolve(testDevices);
  
  return await request(wsProtocol.ALL_CLIENTS);
}

function getClient(id: string): Device | undefined {
  // return testDevices.find(e => e.id == id);
  
  return devices.get(id);
}

async function setServerState(enable: boolean): Promise<ServerState> {
  const req: ServerStateOptions = { enable };
  return await request(wsProtocol.SERVER_STATE, req);
}

function shutdown() {
  const msg: ClientMessage = { message: wsProtocol.SHUTDOWN };
  rws.send(JSON.stringify(msg));
  // TODO: close window and stop process
}

const service = {
  getServerInfo,
  getAllClients,
  getClient,
  setServerState,
  shutdown,
  onClientConnected,
  onClientDisconnected,
  onConnected,
  onConnectionLost,
};

export default service;
