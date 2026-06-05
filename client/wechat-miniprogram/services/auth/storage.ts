const KEYS = {
  accessToken: 'auth_access_token',
  refreshToken: 'auth_refresh_token',
  sessionId: 'auth_session_id',
  guestSessionId: 'guest_session_id',
  wxOpenId: 'wx_openid',
  lastTheme: 'last_theme',
} as const;

export interface TokenPair {
  accessToken: string;
  refreshToken: string;
  sessionId?: string;
  expiresInSeconds?: number;
}

export function loadTokenPair(): TokenPair | null {
  try {
    const accessToken = wx.getStorageSync(KEYS.accessToken) as string;
    const refreshToken = wx.getStorageSync(KEYS.refreshToken) as string;
    if (!accessToken || !refreshToken) return null;
    const sessionId = wx.getStorageSync(KEYS.sessionId) as string | undefined;
    return { accessToken, refreshToken, sessionId };
  } catch {
    return null;
  }
}

export function saveTokenPair(pair: TokenPair): void {
  wx.setStorageSync(KEYS.accessToken, pair.accessToken);
  wx.setStorageSync(KEYS.refreshToken, pair.refreshToken);
  if (pair.sessionId) wx.setStorageSync(KEYS.sessionId, pair.sessionId);
}

export function clearAuth(): void {
  wx.removeStorageSync(KEYS.accessToken);
  wx.removeStorageSync(KEYS.refreshToken);
  wx.removeStorageSync(KEYS.sessionId);
}

export function getGuestSessionId(): string | null {
  try {
    const id = wx.getStorageSync(KEYS.guestSessionId) as string;
    return id || null;
  } catch {
    return null;
  }
}

export function setGuestSessionId(id: string): void {
  wx.setStorageSync(KEYS.guestSessionId, id);
}

export function clearGuestSession(): void {
  wx.removeStorageSync(KEYS.guestSessionId);
}

export function getWxOpenId(): string | null {
  try {
    const id = wx.getStorageSync(KEYS.wxOpenId) as string;
    return id || null;
  } catch {
    return null;
  }
}

export function setWxOpenId(id: string): void {
  wx.setStorageSync(KEYS.wxOpenId, id);
}

export function getLastTheme(): string | null {
  try {
    return (wx.getStorageSync(KEYS.lastTheme) as string) || null;
  } catch {
    return null;
  }
}

export function setLastTheme(theme: string): void {
  wx.setStorageSync(KEYS.lastTheme, theme);
}
