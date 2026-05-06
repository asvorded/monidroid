import { Button, Icon, Select, SelectItem, Switch, Text } from "@gravity-ui/uikit";
import { useAppTheme } from "../hooks/useAppTheme";
import { useCallback, useEffect, useState } from "react";

import { AppTheme, PanelOptions } from "../../common/control.types";
import service from "../services/control";
import { useLoaderData } from "react-router";

const SettingsPage = () => {
  const { theme, setTheme } = useAppTheme();
  const { notifications } = useLoaderData<PanelOptions>();

  const [notif, setNotif] = useState(notifications);

  return (
    <>
      <Text variant="header-2">Settings</Text>
      <div className="flex flex-col mt-4 gap-3">
          <Switch className="items-center" checked={notif} onUpdate={(c) => {
            service.setOptions({ notifications: c });
            setNotif(c);
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