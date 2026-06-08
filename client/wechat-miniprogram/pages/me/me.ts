import { BRAND_DEFAULT_USER } from '../../config/brand';
import { getGateway } from '../../services/gateway/runtime';
import { GatewayBusinessError } from '../../utils/errors';
import { trackEvent } from '../../utils/analytics';
import { pageBackgroundStyle } from '../../utils/theme-surface';
import { themeByKey } from '../../utils/theme';

type LoadState = 'loading' | 'success' | 'error' | 'offline';

Page({
  data: {
    loadState: 'loading' as LoadState,
    errorMessage: '',
    isLoggedIn: false,
    displayName: '访客',
    statusText: '访客模式',
    avatarEmoji: '👋',
    favoritesCount: 0,
    historyCount: 0,
    consentText: '未同意',
    loggingOut: false,
    pageStyle: pageBackgroundStyle(themeByKey('clothing').accent),
  },

  onShow() {
    this.refresh();
  },

  async refresh() {
    this.setData({ loadState: 'loading' });
    try {
      const summary = await getGateway().getMeSummary();
      this.setData({
        loadState: 'success',
        isLoggedIn: summary.isLoggedIn,
        displayName: summary.displayName || (summary.isLoggedIn ? BRAND_DEFAULT_USER : '访客'),
        statusText: summary.isLoggedIn ? '已登录' : '访客模式',
        avatarEmoji: summary.isLoggedIn ? '😊' : '👋',
        favoritesCount: summary.favoritesCount,
        historyCount: summary.historyCount,
        consentText: summary.consentGranted ? '已同意个性化' : '未同意',
      });
    } catch (err) {
      if (this.isOffline(err)) {
        this.setData({ loadState: 'offline' });
        return;
      }
      const message =
        err instanceof GatewayBusinessError ? err.message : '加载失败';
      this.setData({ loadState: 'error', errorMessage: message });
    }
  },

  onRetry() {
    this.refresh();
  },

  goLogin() {
    const returnUrl = encodeURIComponent('/pages/me/me');
    wx.navigateTo({ url: `/pages/login/login?return_url=${returnUrl}` });
  },

  async onLogout() {
    this.setData({ loggingOut: true });
    try {
      await getGateway().logoutSession(false);
      trackEvent('auth_logout_success');
      wx.showToast({ title: '已退出', icon: 'success' });
      this.refresh();
    } catch {
      wx.showToast({ title: '退出失败', icon: 'none' });
    } finally {
      this.setData({ loggingOut: false });
    }
  },

  goFavorites() {
    if (!this.data.isLoggedIn) {
      const returnUrl = encodeURIComponent('/pages/favorites/favorites');
      wx.navigateTo({ url: `/pages/login/login?return_url=${returnUrl}` });
      return;
    }
    wx.navigateTo({ url: '/pages/favorites/favorites' });
  },

  goHistory() {
    wx.navigateTo({ url: '/pages/history/history' });
  },

  isOffline(err: unknown): boolean {
    return Boolean(err && typeof err === 'object' && (err as { kind?: string }).kind === 'offline');
  },
});
