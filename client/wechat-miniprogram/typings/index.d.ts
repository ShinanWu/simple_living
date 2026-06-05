/// <reference types="miniprogram-api-typings" />

interface IAppOption {
  globalData: {
    gateway: import('../services/gateway/types').GatewayAPI;
    pendingLoginAction: string | null;
    lastTheme: import('../utils/theme').ThemeKey;
  };
}
