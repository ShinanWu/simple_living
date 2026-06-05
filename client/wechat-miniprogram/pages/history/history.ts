import { getGateway } from '../../services/gateway/runtime';
import { GatewayBusinessError } from '../../utils/errors';

type LoadState = 'loading' | 'success' | 'error';

Page({
  data: {
    loadState: 'loading' as LoadState,
    errorMessage: '',
    items: [] as Array<{ guideCardId: string; lastSeenAt: string }>,
  },

  onLoad() {
    this.load();
  },

  async load() {
    this.setData({ loadState: 'loading' });
    try {
      const res = await getGateway().listHistory({ limit: 50 });
      this.setData({ loadState: 'success', items: res.items });
    } catch (err) {
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
      url: `/pages/guide-detail/guide-detail?guide_card_id=${encodeURIComponent(id)}&recommendation_id=&scene=home_feed&item_rank=1&title=&reason=-&theme=clothing`,
    });
  },
});
