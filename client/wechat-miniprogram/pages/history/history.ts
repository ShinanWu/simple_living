import { getGateway } from '../../services/gateway/runtime';
import { GatewayBusinessError } from '../../utils/errors';
import { formatDisplayTime } from '../../utils/datetime';
import { guideDetailPath } from '../../utils/guide-route';

type LoadState = 'loading' | 'success' | 'empty' | 'error' | 'offline';

type HistoryRow = {
  guideCardId: string;
  lastSeenAt: string;
  displayTime: string;
};

Page({
  data: {
    loadState: 'loading' as LoadState,
    errorMessage: '',
    items: [] as HistoryRow[],
    clearing: false,
  },

  onLoad() {
    this.load();
  },

  onShow() {
    if (this.data.loadState !== 'loading') {
      this.load();
    }
  },

  async load() {
    this.setData({ loadState: 'loading' });
    try {
      const res = await getGateway().listHistory({ limit: 50 });
      const items = res.items.map((item) => ({
        ...item,
        displayTime: formatDisplayTime(item.lastSeenAt),
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

  onClear() {
    if (this.data.clearing || !this.data.items.length) return;
    wx.showModal({
      title: '清空浏览历史',
      content: '确定清空全部浏览记录吗？',
      success: async (res) => {
        if (!res.confirm) return;
        this.setData({ clearing: true });
        try {
          await getGateway().clearHistory();
          this.setData({ loadState: 'empty', items: [] });
          wx.showToast({ title: '已清空', icon: 'success' });
        } catch (err) {
          const message =
            err instanceof GatewayBusinessError ? err.message : '清空失败';
          wx.showToast({ title: message, icon: 'none' });
        } finally {
          this.setData({ clearing: false });
        }
      },
    });
  },

  isOffline(err: unknown): boolean {
    return Boolean(err && typeof err === 'object' && (err as { kind?: string }).kind === 'offline');
  },
});
