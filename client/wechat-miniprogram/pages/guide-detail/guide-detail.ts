import type { IAppOption } from '../../app';
import { loadTokenPair } from '../../services/auth/storage';
import { getGateway } from '../../services/gateway/runtime';
import type { FeedItemContext } from '../../services/gateway/types';
import { galleryForTheme, themeByKey, type ThemeKey } from '../../utils/theme';
import { GatewayBusinessError } from '../../utils/errors';
import { trackEvent } from '../../utils/analytics';

type LoadState = 'loading' | 'success' | 'error' | 'offline';

Page({
  data: {
    loadState: 'loading' as LoadState,
    errorMessage: '',
    previewTitle: '',
    previewReasonDisplay: '',
    detail: {
      title: '',
      summary: '',
      isCommercial: false,
    },
    gallerySlides: [] as Array<{ url?: string; emoji?: string; caption: string; bg: string }>,
    galleryIndex: 0,
    favoriteLabel: '收藏',
    favorited: false,
  },

  context: null as FeedItemContext | null,
  theme: 'clothing' as ThemeKey,

  onLoad(query: Record<string, string | undefined>) {
    const guideCardId = query.guide_card_id ?? '';
    this.context = {
      guideCardId,
      recommendationId: query.recommendation_id ?? '',
      scene: query.scene ?? 'home_feed',
      itemRank: Number(query.item_rank ?? '1'),
    };
    this.theme = (query.theme as ThemeKey) || 'clothing';
    const reason = decodeURIComponent(query.reason ?? '-');
    this.setData({
      previewTitle: decodeURIComponent(query.title ?? ''),
      previewReasonDisplay: reason === '-' ? '为你精选的轻量推荐' : reason,
    });
    this.buildGalleryFallback();
    this.loadDetail(guideCardId);
  },

  buildGalleryFallback() {
    const meta = themeByKey(this.theme);
    const slides = galleryForTheme(this.theme).map((g) => ({
      emoji: g.emoji,
      caption: g.caption,
      bg: `linear-gradient(135deg, ${meta.accent}33, #fff)`,
    }));
    this.setData({ gallerySlides: slides });
  },

  async loadDetail(guideCardId: string) {
    this.setData({ loadState: 'loading' });
    try {
      const api = getGateway();
      const detail = await api.getGuideDetail(guideCardId);
      const meta = themeByKey(this.theme);
      let slides = galleryForTheme(this.theme).map((g, i) => ({
        url: detail.galleryUrls[i] || detail.coverUrl,
        emoji: g.emoji,
        caption: g.caption,
        bg: `linear-gradient(135deg, ${meta.accent}33, #fff)`,
      }));
      if (detail.coverUrl && !detail.galleryUrls.length) {
        slides = [{ url: detail.coverUrl, caption: detail.title, bg: slides[0]?.bg ?? '#fff' }];
      }
      await api.recordHistoryEvent(guideCardId, 'detail').catch(() => undefined);
      this.setData({
        loadState: 'success',
        detail,
        gallerySlides: slides,
      });
    } catch (err) {
      if (this.isOffline(err)) {
        this.setData({ loadState: 'offline' });
        return;
      }
      const message =
        err instanceof GatewayBusinessError ? err.message : '加载失败，请稍后重试';
      this.setData({ loadState: 'error', errorMessage: message });
    }
  },

  onRetry() {
    if (this.context) this.loadDetail(this.context.guideCardId);
  },

  onGalleryChange(e: WechatMiniprogram.SwiperChange) {
    this.setData({ galleryIndex: e.detail.current });
  },

  async onFavorite() {
    if (!this.context) return;
    if (!loadTokenPair()?.accessToken) {
      const app = getApp<IAppOption>();
      app.globalData.pendingLoginAction = `favorite:${this.context.guideCardId}`;
      wx.navigateTo({
        url: `/pages/login/login?redirect=favorite&guide_card_id=${encodeURIComponent(this.context.guideCardId)}`,
      });
      return;
    }
    try {
      const api = getGateway();
      const res = await api.addFavorite(this.context.guideCardId);
      this.setData({
        favorited: true,
        favoriteLabel: res.alreadyFavorited ? '已收藏' : '收藏成功',
      });
      trackEvent('guide_detail_favorite_success', { guide_card_id: this.context.guideCardId });
      wx.showToast({ title: this.data.favoriteLabel, icon: 'none' });
    } catch (err) {
      if (err instanceof GatewayBusinessError && err.code === 20001) {
        wx.navigateTo({ url: '/pages/login/login' });
        return;
      }
      wx.showToast({ title: '收藏失败', icon: 'none' });
    }
  },

  goBuy() {
    if (!this.context) return;
    const c = this.context;
    const q = [
      `guide_card_id=${encodeURIComponent(c.guideCardId)}`,
      `recommendation_id=${encodeURIComponent(c.recommendationId)}`,
      `scene=${encodeURIComponent(c.scene)}`,
      `item_rank=${c.itemRank}`,
    ].join('&');
    wx.navigateTo({ url: `/pages/redirect-prepare/redirect-prepare?${q}` });
  },

  isOffline(err: unknown): boolean {
    return Boolean(err && typeof err === 'object' && (err as { kind?: string }).kind === 'offline');
  },
});
