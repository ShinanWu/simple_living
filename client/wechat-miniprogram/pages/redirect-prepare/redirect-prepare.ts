import { getGateway } from '../../services/gateway/runtime';
import type { FeedItemContext } from '../../services/gateway/types';
import { GatewayBusinessError } from '../../utils/errors';
import { trackEvent } from '../../utils/analytics';

type LoadState = 'loading' | 'success' | 'error' | 'offline';

Page({
  data: {
    loadState: 'loading' as LoadState,
    errorMessage: '',
    landingUrl: '',
    guideCardId: '',
  },

  context: null as FeedItemContext | null,

  onLoad(query: Record<string, string | undefined>) {
    this.context = {
      guideCardId: query.guide_card_id ?? '',
      recommendationId: query.recommendation_id ?? '',
      scene: query.scene ?? 'home_feed',
      itemRank: Number(query.item_rank ?? '1'),
    };
    this.setData({ guideCardId: this.context.guideCardId });
    this.prepare();
  },

  async prepare() {
    if (!this.context) return;
    this.setData({ loadState: 'loading' });
    try {
      const api = getGateway();
      const res = await api.postRedirectPrepare(this.context);
      this.setData({ loadState: 'success', landingUrl: res.landingUrl });
      trackEvent('redirect_prepare_success', { guide_card_id: this.context.guideCardId });
    } catch (err) {
      if (this.isOffline(err)) {
        this.setData({ loadState: 'offline' });
        return;
      }
      const message =
        err instanceof GatewayBusinessError ? err.message : '暂时无法生成链接';
      this.setData({ loadState: 'error', errorMessage: message });
    }
  },

  onRetry() {
    this.prepare();
  },

  onCopy() {
    wx.setClipboardData({
      data: this.data.landingUrl,
      success: () => wx.showToast({ title: '已复制', icon: 'success' }),
    });
  },

  onOpen() {
    const url = encodeURIComponent(this.data.landingUrl);
    wx.navigateTo({
      url: `/pages/web-outbound/web-outbound?url=${url}`,
      fail: () => {
        wx.showModal({
          title: '无法打开网页',
          content: '请复制链接到浏览器打开，或在小程序后台配置业务域名。',
          showCancel: false,
        });
      },
    });
  },

  isOffline(err: unknown): boolean {
    return Boolean(err && typeof err === 'object' && (err as { kind?: string }).kind === 'offline');
  },
});
