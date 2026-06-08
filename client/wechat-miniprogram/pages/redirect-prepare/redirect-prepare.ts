import { getGateway } from '../../services/gateway/runtime';
import type { FeedItemContext } from '../../services/gateway/types';
import { GatewayBusinessError } from '../../utils/errors';
import { trackEvent } from '../../utils/analytics';
import { themeByKey, type ThemeKey } from '../../utils/theme';
import { pageBackgroundStyle } from '../../utils/theme-surface';

type LoadState = 'loading' | 'success' | 'error' | 'offline';

Page({
  data: {
    loadState: 'loading' as LoadState,
    errorMessage: '',
    landingUrl: '',
    displayTitle: '',
    pageStyle: '',
  },

  context: null as FeedItemContext | null,

  onLoad(query: Record<string, string | undefined>) {
    this.context = {
      guideCardId: query.guide_card_id ?? '',
      recommendationId: query.recommendation_id ?? '',
      scene: query.scene ?? 'home_feed',
      itemRank: Number(query.item_rank ?? '1'),
    };
    const title = decodeURIComponent(query.title ?? '');
    const theme = themeByKey((query.theme as ThemeKey) || 'clothing');
    this.setData({
      displayTitle: title || '精选好物',
      pageStyle: pageBackgroundStyle(theme.accent),
    });
    this.prepare();
  },

  async prepare() {
    if (!this.context) return;
    this.setData({ loadState: 'loading', errorMessage: '' });
    try {
      const api = getGateway();
      const res = await api.postRedirectPrepare(this.context);
      const landingUrl = (res.landingUrl ?? '').trim();
      if (!landingUrl) {
        this.setData({
          loadState: 'error',
          errorMessage: '购买链接暂时不可用，请稍后再试',
        });
        trackEvent('redirect_prepare_empty_url', { guide_card_id: this.context.guideCardId });
        return;
      }
      this.setData({ loadState: 'success', landingUrl });
      trackEvent('redirect_prepare_success', { guide_card_id: this.context.guideCardId });
    } catch (err) {
      if (this.isOffline(err)) {
        this.setData({ loadState: 'offline' });
        return;
      }
      const message =
        err instanceof GatewayBusinessError ? err.message : '暂时无法生成链接，请稍后重试';
      this.setData({ loadState: 'error', errorMessage: message });
    }
  },

  onRetry() {
    this.prepare();
  },

  onCopy() {
    if (!this.data.landingUrl) return;
    wx.setClipboardData({
      data: this.data.landingUrl,
      success: () => wx.showToast({ title: '已复制', icon: 'success' }),
    });
  },

  onOpen() {
    const url = this.data.landingUrl;
    if (!url) return;
    wx.navigateTo({
      url: `/pages/web-outbound/web-outbound?url=${encodeURIComponent(url)}`,
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
