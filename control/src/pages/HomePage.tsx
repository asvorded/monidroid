import { Card, Switch, Text } from "@gravity-ui/uikit";
import { useLoaderData } from "react-router";
import { ServerInfo } from "../services/wsservice.types";

const HomePage = () => {
  const info = useLoaderData<ServerInfo>();

  return (
    <div className="flex flex-col justify-center items-center h-full">
      <Card className="flex flex-col p-12 overflow-scroll" size="l" style={{ scrollbarWidth: 'none' }}>
        <div className="text-center">
          <Text className="block" variant="display-3">Monidroid control panel</Text>
        </div>

        <div className="mt-8 mb-4 grid grid-cols-2 gap-1">
          <Text variant="body-3">Server</Text>
          <Switch size="l" className="justify-self-end" checked={info.enabled} />
        </div>

        <div className="mt-4 mb-4 grid grid-cols-2 gap-2">
          <Text variant="body-3">Server vesrion</Text>
          <Text variant="body-3" className="justify-self-end">{info.version}</Text>
          <Text variant="body-3">Hostname</Text>
          <Text variant="body-3" className="justify-self-end">{info.hostname}</Text>
          <Text variant="body-3">Host addresses</Text>
          <div className="justify-self-end">
            {info.addresses.map(a => (
              <Text variant="body-3" className="block text-end">{a}</Text>
            ))}
          </div>
        </div>
      </Card>
    </div>
  );
};

export default HomePage;