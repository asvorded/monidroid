import { Button, Icon, Select, SelectItem, Switch, Text } from "@gravity-ui/uikit";
import { Sun } from '@gravity-ui/icons';
import { useAppTheme } from "../hooks/useAppTheme";
import { useCallback, useState } from "react";

import { AppTheme } from "../../common/control.types";
import service from "../services/control";

const initial = (await service.getOptions()).notifications;

const SettingsPage = () => {
  const { theme, setTheme } = useAppTheme();

  const [notifications, setNotifications] = useState(initial);

  return (
    <>
      <Text variant="header-2">Settings</Text>
      <div className="flex flex-col mt-4 gap-3">
        {/* TODO: theme and notifications */}
          <Switch className="items-center" checked={notifications} onUpdate={(c) => {
            service.setOptions({ notifications: c });
            setNotifications(c);
          }}>
            <Text className="block" variant="body-2">System notifications</Text>
          </Switch>
          <div>
            <Text variant="body-2">Application theme</Text>
            <div className="inline-block w-4"></div>
            <Select value={[theme]} width={96} onUpdate={v => {setTheme(v[0] as AppTheme)}}>
              <SelectItem value={ "light" satisfies AppTheme } content="Light" />
              <SelectItem value={ "dark" satisfies AppTheme } content="Dark" />
              <SelectItem value={ "system" satisfies AppTheme } content="System" />
            </Select>
          </div>
      </div>
    </>
  );
};

export default SettingsPage;