import { ThemeProvider } from '@gravity-ui/uikit';
import React, { createContext, useContext, useEffect, useState } from 'react';
import { AppTheme } from '../../common/control.types';
import service from '../services/control';

const THEME_KEY = 'md-app-theme';

type ThemeService = {
  theme: AppTheme,
  setTheme: (theme: AppTheme) => void,
}

const initial = await service.getOptions();

const ThemeContext = createContext<ThemeService>({
  theme: initial.theme,
  setTheme: () => { },
});

export function useAppTheme() {
  const context = useContext(ThemeContext);
  if (context === undefined) {
    throw new Error('useAppTheme called outside provider component');
  }
  return context;
};

export const CustomThemeProvider = ({ children } : { children: React.ReactNode }) => {
  const [theme, setTheme] = useState<AppTheme>(initial.theme);
  // const [realTheme, setRealTheme] = useState<"dark" | "light">();

  // useEffect(() => {
  //   const mediaQuery = window.matchMedia('(prefers-color-scheme: dark)');
  //   const updateResolvedTheme = () => {
  //     if (theme == "system") {
  //       setRealTheme(mediaQuery.matches ? 'dark' : 'light');
  //     }
  //   };
  //   updateResolvedTheme();
  //   mediaQuery.addEventListener('change', updateResolvedTheme);
  //   return () => mediaQuery.removeEventListener('change', updateResolvedTheme);
  // }, []);

  useEffect(() => {
    service.setOptions({ theme });
  }, [theme]);

  return (
    <ThemeContext value={{ theme, setTheme }}>
      <ThemeProvider theme={theme}>
        {children}
      </ThemeProvider>
    </ThemeContext>
  )
};