import { ThemeProvider } from '@gravity-ui/uikit';
import { createContext, useContext, useEffect, useState } from 'react';

const THEME_KEY = 'md-app-theme';

type AppTheme = 'dark' | 'light';

type ThemeService = {
  theme: AppTheme,
  system: boolean,
  setTheme: (theme: AppTheme) => void,
  toggleSystem: (system: boolean) => void,
}

const ThemeContext = createContext<ThemeService>(null);

export function useAppTheme() {
  const context = useContext(ThemeContext);
  if (context === undefined) {
    throw new Error('useAppTheme called outside provider component');
  }
  return context;
};

export const CustomThemeProvider = ({children}) => {
  const [theme, setTheme] = useState<AppTheme>('light');
  const [system, setSystem] = useState<boolean>(true);

  useEffect(() => {
    const storedTheme = localStorage.getItem(THEME_KEY) as AppTheme;
    if (storedTheme) {
      setTheme(storedTheme);
    } else {
      setSystem(true);
    }
  }, []);

  useEffect(() => {
    if (system) {
      localStorage.removeItem(THEME_KEY)
    } else {
      localStorage.setItem(THEME_KEY, theme);
    }
  }, [theme, system]);

  useEffect(() => {
    const mediaQuery = window.matchMedia('(prefers-color-scheme: dark)');
    const updateResolvedTheme = () => {
      if (system) {
        setTheme(mediaQuery.matches ? 'dark' : 'light');
      }
    };
    updateResolvedTheme();
    mediaQuery.addEventListener('change', updateResolvedTheme);
    return () => mediaQuery.removeEventListener('change', updateResolvedTheme);
  }, []);

  return (
    <ThemeContext value={{ theme, system, setTheme, toggleSystem: setSystem }}>
      <ThemeProvider theme={system ? 'system' : theme}>
        {children}
      </ThemeProvider>
    </ThemeContext>
  )
};