import type { IAppOption } from '../../app';
import { loadTokenPair } from '../../services/auth/storage';
import { getGateway } from '../../services/gateway/runtime';
import type { FeedItemContext } from '../../services/gateway/types';
import { galleryForTheme, themeByKey, type ThemeKey } from '../../utils/theme';
import { pageBackgroundStyle } from '../../utils/theme-surface';
import { GatewayBusinessError } from '../../utils/errors';
import { trackEvent } from '../../utils/analytics';
import { guideDetailPath, redirectPreparePath } from '../../utils/guide-route';

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
    pageStyle: '',
  },

  context: null as FeedItemContext | null,
  theme: 'clothing' as ThemeKey,
  _pendingFavorite: false,

  onLoad(query: Record<string, string | undefined>) {
    const guideCardId = query.guide_card_id ?? '';
    this.context = {
      guideCardId,
      recommendationId: query.recommendation_id ?? '',
      scene: query.scene ?? 'home_feed',
      itemRank: Number(query.item_rank ?? '1'),
    };
    this.theme = (query.theme as ThemeKey) || 'clothing';
    this._pendingFavorite = query.action === 'favorite';
    const meta = themeByKey(this.theme);
    const reason = decodeURIComponent(query.reason ?? '-');
    this.setData({
      previewTitle: decodeURIComponent(query.title ?? ''),
      previewReasonDisplay: reason === '-' ? '为你精选的轻量推荐' : reason,
      pageStyle: pageBackgroundStyle(meta.accent),
    });
    this.buildGalleryFallback();
    this.loadDetail(guideCardId);
  },

  onShow() {
    void this.syncFavoriteState().then(() => {
      if (this._pendingFavorite && loadTokenPair()?.accessToken) {
        this._pendingFavorite = false;
        void this.onFavorite();
      }
    });
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
      void this.syncFavoriteState();
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

  async syncFavoriteState() {
    if (!this.context || !loadTokenPair()?.accessToken) return;
    try {
      const res = await getGateway().listFavorites({ limit: 100 });
      const hit = res.items.find((item) => item.guideCardId === this.context!.guideCardId);
      if (hit) {
        this.setData({ favorited: true, favoriteLabel: '已收藏' });
      }
    } catch {
      /* 收藏态非关键路径 */
    }
  },

  onRetry() {
    if (this.context) this.loadDetail(this.context.guideCardId);
  },

  onGalleryChange(e: WechatMiniprogram.SwiperChange) {
    this.setData({ galleryIndex: e.detail.current });
  },

  loginReturnPath(): string {
    if (!this.context) return '/pages/guide-detail/guide-detail';
    return guideDetailPath({
      guideCardId: this.context.guideCardId,
      recommendationId: this.context.recommendationId,
      scene: this.context.scene,
      itemRank: this.context.itemRank,
      title: this.data.detail.title || this.data.previewTitle,
      reason: this.data.previewReasonDisplay,
      theme: this.theme,
      action: 'favorite',
    });
  },

  async onFavorite() {
    if (!this.context) return;
    if (!loadTokenPair()?.accessToken) {
      const app = getApp<IAppOption>();
      app.globalData.pendingLoginAction = `favorite:${this.context.guideCardId}`;
      const returnUrl = encodeURIComponent(this.loginReturnPath());
      wx.navigateTo({
        url: `/pages/login/login?return_url=${returnUrl}`,
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
        const returnUrl = encodeURIComponent(this.loginReturnPath());
        wx.navigateTo({ url: `/pages/login/login?return_url=${returnUrl}` });
        return;
      }
      wx.showToast({ title: '收藏失败', icon: 'none' });
    }
  },

  goBuy() {
    if (!this.context) return;
    const title = this.data.detail.title || this.data.previewTitle;
    wx.navigateTo({ url: redirectPreparePath(this.context, title) });
  },

  isOffline(err: unknown): boolean {
    return Boolean(err && typeof err === 'object' && (err as { kind?: string }).kind === 'offline');
  },
});
