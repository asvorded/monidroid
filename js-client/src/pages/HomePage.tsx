import { Card, Switch, Text } from "@gravity-ui/uikit";

const HomePage = () => {
  return (
    <div className="flex flex-col justify-center items-center h-full">
      <Card className="flex flex-col p-12 overflow-scroll" size="l" style={{ scrollbarWidth: 'none' }}>
        <div className="text-center">
          <Text className="block" variant="display-3">Monidroid control panel</Text>
        </div>

        <div className="mt-8 mb-4 grid grid-cols-2 gap-1">
          <Text variant="body-3">Panel version</Text>
          <Text variant="body-3" className="justify-self-end">X.X.X</Text>
          <Text variant="body-3">Server vesrion</Text>
          <Text variant="body-3" className="justify-self-end">Y.Y.Y</Text>
          <Text variant="body-3">Driver vesrion</Text>
          <Text variant="body-3" className="justify-self-end">Y.Y.Y</Text>
        </div>

        <div className="mt-4 mb-4 grid grid-cols-2 gap-2">
          <Text variant="body-3">Server enabled</Text>
          <Switch size="l" className="justify-self-end"/>
          <Text variant="body-3">Echo server enabled</Text>
          <Switch size="l" className="justify-self-end"/>
        </div>
      </Card>
    </div>
  );
};

export default HomePage;