import { getGateway } from '../../services/gateway/runtime';
import { loadTokenPair } from '../../services/auth/storage';
import { GatewayBusinessError } from '../../utils/errors';
import { formatDisplayTime } from '../../utils/datetime';
import { guideDetailPath } from '../../utils/guide-route';

type LoadState = 'loading' | 'success' | 'empty' | 'error' | 'offline';

type FavoriteRow = {
  favoriteId: string;
  guideCardId: string;
  favoritedAt: string;
  displayTime: string;
};

Page({
  data: {
    loadState: 'loading' as LoadState,
    errorMessage: '',
    items: [] as FavoriteRow[],
    removingId: '',
  },

  onLoad() {
    if (!loadTokenPair()?.accessToken) {
      const returnUrl = encodeURIComponent('/pages/favorites/favorites');
      wx.redirectTo({ url: `/pages/login/login?return_url=${returnUrl}` });
      return;
    }
    this.load();
  },

  onShow() {
    if (loadTokenPair()?.accessToken && this.data.loadState !== 'loading') {
      this.load();
    }
  },

  async load() {
    this.setData({ loadState: 'loading' });
    try {
      const res = await getGateway().listFavorites({ limit: 50 });
      const items = res.items.map((item) => ({
        ...item,
        displayTime: formatDisplayTime(item.favoritedAt),
      }));
      this.setData({
        loadState: items.length ? 'success' : 'empty',
        items,
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
    this.load();
  },

  openDetail(e: WechatMiniprogram.TouchEvent) {
    const id = e.currentTarget.dataset.id as string;
    wx.navigateTo({
      url: guideDetailPath({ guideCardId: id }),
    });
  },

  async onRemove(e: WechatMiniprogram.TouchEvent) {
    const favoriteId = e.currentTarget.dataset.fid as string;
    if (!favoriteId || this.data.removingId) return;
    this.setData({ removingId: favoriteId });
    try {
      await getGateway().removeFavorite(favoriteId);
      const items = this.data.items.filter((item) => item.favoriteId !== favoriteId);
      this.setData({
        items,
        loadState: items.length ? 'success' : 'empty',
      });
      wx.showToast({ title: '已取消收藏', icon: 'none' });
    } catch (err) {
      const message =
        err instanceof GatewayBusinessError ? err.message : '操作失败';
      wx.showToast({ title: message, icon: 'none' });
    } finally {
      this.setData({ removingId: '' });
    }
  },

  isOffline(err: unknown): boolean {
    return Boolean(err && typeof err === 'object' && (err as { kind?: string }).kind === 'offline');
  },
});
