import { useCallback, useEffect, useState } from "react";

import service from "./services/wsservice";

import { Divider, Text, Icon, Button } from "@gravity-ui/uikit";
import { House, Gear, LogoAndroid, PlugConnection, CircleExclamationFill } from '@gravity-ui/icons';
import { MenuButton, ShutdownButton } from "./MenuButtons";
import { Device } from "./services/wsservice.types";
import { Outlet } from "react-router";

const App = () => {
  const [ connected, setConnected ] = useState(true);
  const [ confirm, setConfirm ] = useState(false);

  useEffect(() => {
    service.onConnected = () => {
      setConnected(true);
    }
    service.onConnectionLost = () => {
      setConnected(false)
    }
  }, []);
  
  return (
    <>
      {!connected ? <ErrorBanner/> : null}
      {confirm ? <ShutdownConfirm
        onConfirm={ () => { setConfirm(false); service.shutdown(); } }
        onCancel={ () => setConfirm(false) }
      /> : null}
      <BaseApp onShutdownClick={ () => setConfirm(true) } />
    </>
  )
}

const ShutdownConfirm = ({onConfirm, onCancel} : {
  onConfirm: () => void,
  onCancel: () => void,
}) => {
  return (
    <div className="absolute flex flex-col items-center justify-center size-full backdrop-blur-md z-10">
      <div
        className="flex flex-col items-center text-center p-7 rounded-3xl bg-white"
        style={{ boxShadow: "0 12px 40px rgba(0, 0, 0, 0.25)" }}
      >
        <Icon data={CircleExclamationFill} size={48} fill="#ff0000"></Icon>
        <Text variant="display-2" color="primary" className="mt-4 mb-4">Are you sure?</Text>
        <div>
          <Button size="l" onClick={onConfirm}>Yes</Button>
          <div className="inline-block w-2"></div>
          <Button size="l" view="action" onClick={onCancel}>No</Button>
        </div>
      </div>
    </div>
  )
}

const ErrorBanner = () => {
  return (
    <div className="absolute flex flex-col items-center justify-center size-full backdrop-blur-md z-20">
      <div
        className="flex flex-col items-center text-center p-6 rounded-3xl bg-red-300"
        style={{ boxShadow: "0 12px 40px rgba(0, 0, 0, 0.25)" }}
      >
        <Icon data={PlugConnection} size={48} fill="#ff0000"></Icon>
        <Text variant="display-2" color="primary" className="mt-4 mb-2">Connection lost</Text>
        <Text variant="body-2">Connection lost. We are trying to restore it. Please ensure that server is running.</Text>
      </div>
    </div>
  )
}

const BaseApp = ({onShutdownClick} : {
  onShutdownClick?: () => void
}) => {
  const [ clients, setClients ] = useState<Device[]>([]);

  useEffect(() => {
    service.onClientConnected = (client, alreadyPresent) => {
      if (!alreadyPresent) {
        setClients([...clients, client]);
      }
    };

    service.onClientDisconnected = (id, alreadyRemoved) => {
      if (!alreadyRemoved) {
        setClients(clients.filter(c => c.id != id));
      }
    };

    return () => {
      service.onClientConnected = () => {};
      service.onClientDisconnected = () => {};
    };
  }, []);

  return (
    <div className="flex h-dvh pt-8">
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
          <Divider className="mt-3"/>
          <MenuButton className="mt-2" icon={Gear} text={"Settings"} to="/settings" />
          <ShutdownButton className="mt-2" onClick={onShutdownClick} />
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
