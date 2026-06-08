import { ensureRealGatewayConfig } from './config/env';
import { getGateway, resetGatewayForTests } from './services/gateway/runtime';
import { loadTokenPair, getLastTheme } from './services/auth/storage';
import type { ThemeKey } from './utils/theme';
import { trackEvent } from './utils/analytics';
import { setBrandNavigationTitle } from './utils/navigation';
import { startColdPrefetchAllThemes } from './services/feed/theme-feed-cache';

App<IAppOption>({
  globalData: {
    gateway: null as unknown as ReturnType<typeof getGateway>,
    pendingLoginAction: null,
    lastTheme: (getLastTheme() as ThemeKey) || 'clothing',
  },
  onLaunch() {
    ensureRealGatewayConfig();
    resetGatewayForTests();
    this.globalData.gateway = getGateway();
    setBrandNavigationTitle();
    const hasAuth = Boolean(loadTokenPair()?.accessToken);
    trackEvent('app_launch', { logged_in: hasAuth });
    void startColdPrefetchAllThemes();
  },
});
