export const APP_VERSION = '1.0.0';
export const CLIENT_PLATFORM = 'wechat_miniprogram';

/** 公网 frp 入口；与 environments/local-qemu/nodes.example.env 中 FRP_CUSTOM_DOMAIN 示例一致 */
export const DEFAULT_GATEWAY_BASE_URL = 'http://8.152.103.12';

const STORAGE_GATEWAY_URL = 'gateway_base_url';
export function readGatewayBaseUrl(): string {
  try {
    const fromStorage = wx.getStorageSync(STORAGE_GATEWAY_URL) as string;
    if (typeof fromStorage === 'string' && fromStorage.trim()) {
      return fromStorage.trim().replace(/\/$/, '');
    }
  } catch {
    /* ignore */
  }
  return DEFAULT_GATEWAY_BASE_URL;
}

/** 启动时绑定真实 Gateway（frp 公网），禁止静默回退 Mock */
export function ensureRealGatewayConfig(): void {
  try {
    const cur = wx.getStorageSync(STORAGE_GATEWAY_URL) as string;
    if (typeof cur !== 'string' || !cur.trim()) {
      wx.setStorageSync(STORAGE_GATEWAY_URL, DEFAULT_GATEWAY_BASE_URL);
    }
  } catch {
    /* ignore */
  }
}

export function resolvedDeviceId(): string {
  const key = 'device_id';
  try {
    let id = wx.getStorageSync(key) as string;
    if (typeof id === 'string' && id.length > 8) return id;
    id = `wxmp_${Date.now().toString(36)}_${Math.random().toString(36).slice(2, 10)}`;
    wx.setStorageSync(key, id);
    return id;
  } catch {
    return `wxmp_fallback_${Date.now()}`;
  }
}
