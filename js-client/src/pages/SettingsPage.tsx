import { Button, Switch, Text } from "@gravity-ui/uikit";
import { useAppTheme } from "../hooks/useAppTheme";
import { useCallback } from "react";
const SettingsPage = () => {

  const { theme, system, setTheme, toggleSystem } = useAppTheme();

  const onDarkMode = useCallback((checked: boolean) => {
    setTheme(checked ? 'dark' : 'light');
  }, []);

  const onSystemTheme = useCallback((checked: boolean) => {
    toggleSystem(checked);
  }, []);

  return (
    <>
      <Text variant="header-2">Settings</Text>
      <div className="flex flex-col h-full mt-4 gap-3">
          <Switch className="items-center" onUpdate={onDarkMode} disabled={system} checked={theme === 'dark'}>
            <Text className="block" variant="body-2">Dark theme</Text>
          </Switch>
          <Switch className="items-center" onUpdate={onSystemTheme} checked={system}>
            <Text className="block" variant="body-2">Use system theme</Text>
          </Switch>
          <Switch className="items-center">
            <Text className="block" variant="body-2">Automatically check for updates</Text>
          </Switch>
          <div>
            <Button className="self-start" view="action">Check for updates</Button>
            <Text className="block mt-1">Last time checked: {new Date().toLocaleString(navigator.language)}</Text>
          </div>
      </div>
    </>
  );
};

export default SettingsPage;