import { useEffect, useState } from "react";

import service from "./services/wsservice";

import { Divider, Text } from "@gravity-ui/uikit";
import { House, Gear, LogoAndroid } from '@gravity-ui/icons';
import { MenuButton } from "./MenuButtons";
import { Device } from "monidroid-server/src/types/websocket";
import { Outlet } from "react-router";

const testDevices: Device[] = Array(5).fill(0).map((v, i) => ({
    id: 'dev_' + i,
    os: 'android',
    address: '123',
    name: 'azaa'
  } as Device));

const App = () => {
  const [ clients, setClients ] = useState<Device[]>(testDevices);

  useEffect(() => {
    // service.getAllClients().then(result => setClients(result));

    service.registerOnClientConnected((client, alreadyPresent) => {
      if (!alreadyPresent) {
        setClients([...clients, client]);
      }
    });

    service.registerOnClientDisconnected((id, alreadyRemoved) => {
      if (!alreadyRemoved) {
        setClients(clients.filter(c => c.id != id));
      }
    });

    return () => { service.unregisterAll(); };
  }, []);

  return (
    <div className="flex h-dvh">
      <main className="flex relative flex-col flex-1 min-w-56 p-3">
        <header className="mb-3">
          <Text variant="display-2" className="block">Monidroid</Text>
          <Text className="block" variant="header-1">Control panel</Text>
        </header>

        <section
          className="shadowed-scroll-area"
          style={{ height: 'stretch', scrollbarWidth: 'thin' }}
        >
          <MenuButton icon={House} text={"Home"} to="/" />
          <Divider className="mt-3 mb-3"/>
          <Text className="block mb-3 mt-1" variant="header-1">Devices</Text>
          <ul>
            { clients.map((item) => (
              <li key={item.id}>
                <MenuButton icon={LogoAndroid} text={item.name} to={`/devices/${item.id}`} />
              </li>
            )) }
          </ul>
        </section>

        <section>
          <Divider className="mt-3 mb-3"/>
          <MenuButton icon={Gear} text={"Settings"} to="/settings" />
        </section>
      </main>

      <Divider orientation="vertical" />

      <div className="flex-3 p-4">
        <Outlet />
      </div>
    </div>
  );
}

export default App;
