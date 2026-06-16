import { BRAND_NAME, BRAND_FEED_END_HINT } from '../../config/brand';
import { loadTokenPair, setLastTheme } from '../../services/auth/storage';
import { getGateway } from '../../services/gateway/runtime';
import { THEMES, nextTheme, themeByKey, type ThemeKey } from '../../utils/theme';
import { trackEvent } from '../../utils/analytics';
import { setBrandNavigationTitle } from '../../utils/navigation';
import { GatewayBusinessError } from '../../utils/errors';
import { guideDetailPath } from '../../utils/guide-route';
import {
  appendThemeFeedPage,
  getCachedThemeFeed,
  hasCachedThemeFeed,
  refreshThemeFeed,
  waitForColdPrefetch,
  type DisplayCard,
  type ThemeFeedLoadState,
  type ThemeFeedSnapshot,
} from '../../services/feed/theme-feed-cache';

const THEME_SWIPE = 54;

Page({
  data: {
    brandName: BRAND_NAME,
    brandEndHint: BRAND_FEED_END_HINT,
    themes: THEMES,
    selectedTheme: 'clothing' as ThemeKey,
    themeMeta: themeByKey('clothing'),
    loadState: 'loading' as ThemeFeedLoadState | 'loading',
    errorMessage: '',
    cards: [] as DisplayCard[],
    swiperCurrent: 0,
    endHint: false,
    nextCursor: null as string | null,
    hasMore: false,
    loadingMore: false,
    showBackToTop: false,
    favoriteLabel: '收藏',
    favorited: false,
    refreshing: false,
  },

  _loadingMore: false,
  _themeTouchX: 0,
  _themeTouchY: 0,

  onLoad() {
    const app = getApp<IAppOption>();
    const theme = (app.globalData.lastTheme || 'clothing') as ThemeKey;
    this.setData({ selectedTheme: theme, themeMeta: themeByKey(theme) });
    void this.bootstrapTheme(theme);
  },

  onShow() {
    setBrandNavigationTitle();
    trackEvent('home_page_show');
    const app = getApp<IAppOption>();
    const pending = app.globalData.pendingHomeFavoriteGuide;
    void this.syncFavoriteState().then(() => {
      const card = this.currentCard();
      if (pending && loadTokenPair()?.accessToken && card?.guideCardId === pending) {
        app.globalData.pendingHomeFavoriteGuide = null;
        void this.onFavorite();
      }
    });
  },

  currentCard(): DisplayCard | undefined {
    const { cards, swiperCurrent } = this.data;
    return cards[swiperCurrent];
  },

  async bootstrapTheme(theme: ThemeKey) {
    if (hasCachedThemeFeed(theme)) {
      this.applySnapshot(theme, getCachedThemeFeed(theme)!);
      return;
    }
    this.setData({ loadState: 'loading' });
    await waitForColdPrefetch();
    const cached = getCachedThemeFeed(theme);
    if (cached) {
      this.applySnapshot(theme, cached);
      return;
    }
    const snapshot = await refreshThemeFeed(theme);
    this.applySnapshot(theme, snapshot);
  },

  applySnapshot(theme: ThemeKey, snapshot: ThemeFeedSnapshot) {
    const cards = snapshot.cards;
    this.setData({
      selectedTheme: theme,
      themeMeta: themeByKey(theme),
      loadState: snapshot.loadState,
      errorMessage: snapshot.errorMessage,
      cards,
      swiperCurrent: 0,
      endHint: cards.length > 0 && cards.length === 1,
      nextCursor: snapshot.nextCursor,
      hasMore: snapshot.hasMore,
      showBackToTop: false,
      favoriteLabel: '收藏',
      favorited: false,
      refreshing: false,
    });
    void this.syncFavoriteState();
  },

  goMe() {
    wx.navigateTo({ url: '/pages/me/me' });
  },

  onSearchTap() {
    wx.showToast({ title: '搜索功能即将上线', icon: 'none', duration: 1500 });
  },

  onThemeTap(e: WechatMiniprogram.TouchEvent) {
    const theme = e.currentTarget.dataset.theme as ThemeKey;
    this.switchTheme(theme);
  },

  onThemeTouchStart(e: WechatMiniprogram.TouchEvent) {
    this._themeTouchX = e.touches[0].clientX;
    this._themeTouchY = e.touches[0].clientY;
  },

  onThemeTouchEnd(e: WechatMiniprogram.TouchEvent) {
    const dx = e.changedTouches[0].clientX - this._themeTouchX;
    const dy = e.changedTouches[0].clientY - this._themeTouchY;
    if (Math.abs(dx) < THEME_SWIPE || Math.abs(dx) <= Math.abs(dy)) return;
    const dir = dx < 0 ? 1 : -1;
    const next = nextTheme(this.data.selectedTheme, dir);
    if (next !== this.data.selectedTheme) this.switchTheme(next);
  },

  switchTheme(theme: ThemeKey) {
    const isReselect = theme === this.data.selectedTheme;
    const app = getApp<IAppOption>();
    app.globalData.lastTheme = theme;
    setLastTheme(theme);
    trackEvent('home_theme_switch', { theme, reselect: isReselect });

    const cached = getCachedThemeFeed(theme);
    if (cached) {
      this.applySnapshot(theme, cached);
      return;
    }
    void this.bootstrapTheme(theme);
  },

  exploreNextTheme() {
    const next = nextTheme(this.data.selectedTheme, 1);
    if (next === this.data.selectedTheme) {
      this.switchTheme(nextTheme(this.data.selectedTheme, -1));
      return;
    }
    this.switchTheme(next);
  },

  onRetry() {
    void this.refreshCurrentTheme();
  },

  async refreshCurrentTheme() {
    const theme = this.data.selectedTheme;
    this.setData({ loadState: 'loading', endHint: false, refreshing: false });
    try {
      const snapshot = await refreshThemeFeed(theme);
      this.applySnapshot(theme, snapshot);
      trackEvent('home_feed_refresh', { theme });
    } catch {
      /* applySnapshot 已写入 error/offline */
    }
  },

  async onPullRefresh() {
    if (this.data.swiperCurrent !== 0 || this.data.refreshing) return;
    this.setData({ refreshing: true });
    const theme = this.data.selectedTheme;
    try {
      const snapshot = await refreshThemeFeed(theme);
      this.applySnapshot(theme, snapshot);
      trackEvent('home_feed_refresh', { theme, source: 'pull' });
    } catch {
      wx.showToast({ title: '刷新失败', icon: 'none' });
    } finally {
      this.setData({ refreshing: false });
    }
  },

  onSwiperChange(e: WechatMiniprogram.SwiperChange) {
    const { current, source } = e.detail;
    if (source === '' || current === this.data.swiperCurrent) return;

    const prev = this.data.swiperCurrent;
    const { cards } = this.data;
    this.setData({
      swiperCurrent: current,
      showBackToTop: current > 0,
      endHint: cards.length > 0 && current >= cards.length - 1,
    });
    trackEvent('home_card_swipe', { direction: current > prev ? 'next' : 'prev' });
    void this.maybeLoadMore(current);
    void this.syncFavoriteState();
  },

  onBackToTop() {
    if (!this.data.cards.length) return;
    this.setData({
      swiperCurrent: 0,
      showBackToTop: false,
      endHint: this.data.cards.length === 1,
    });
    trackEvent('home_back_to_top');
    void this.syncFavoriteState();
  },

  cardAt(e: WechatMiniprogram.TouchEvent): DisplayCard | undefined {
    const index = Number(e.currentTarget.dataset.index ?? this.data.swiperCurrent);
    return this.data.cards[index];
  },

  openDetail(e: WechatMiniprogram.TouchEvent) {
    const card = this.cardAt(e);
    if (!card?.guideCardId) return;
    wx.navigateTo({
      url: guideDetailPath({
        guideCardId: card.guideCardId,
        recommendationId: card.recommendationId,
        scene: card.scene,
        itemRank: card.itemRank,
        title: card.title,
        reason: card.reason,
        theme: this.data.selectedTheme,
      }),
    });
  },

  async syncFavoriteState() {
    const card = this.currentCard();
    if (!card?.guideCardId || !loadTokenPair()?.accessToken) {
      this.setData({ favorited: false, favoriteLabel: '收藏' });
      return;
    }
    try {
      const res = await getGateway().listFavorites({ limit: 100 });
      const hit = res.items.find((item) => item.guideCardId === card.guideCardId);
      this.setData({
        favorited: Boolean(hit),
        favoriteLabel: hit ? '已收藏' : '收藏',
      });
    } catch {
      /* 收藏态非关键路径 */
    }
  },

  async onFavorite(e: WechatMiniprogram.TouchEvent) {
    const card = this.cardAt(e);
    if (!card?.guideCardId) return;
    if (!loadTokenPair()?.accessToken) {
      const app = getApp<IAppOption>();
      app.globalData.pendingHomeFavoriteGuide = card.guideCardId;
      wx.navigateTo({ url: '/pages/login/login' });
      return;
    }
    try {
      const res = await getGateway().addFavorite(card.guideCardId);
      this.setData({
        favorited: true,
        favoriteLabel: res.alreadyFavorited ? '已收藏' : '收藏成功',
      });
      trackEvent('home_favorite_success', { guide_card_id: card.guideCardId });
      wx.showToast({ title: this.data.favoriteLabel, icon: 'none' });
    } catch (err) {
      if (err instanceof GatewayBusinessError && err.code === 20001) {
        const app = getApp<IAppOption>();
        app.globalData.pendingHomeFavoriteGuide = card.guideCardId;
        wx.navigateTo({ url: '/pages/login/login' });
        return;
      }
      wx.showToast({ title: '收藏失败', icon: 'none' });
    }
  },

  async maybeLoadMore(index: number) {
    const { cards, nextCursor, hasMore, selectedTheme } = this.data;
    if (!hasMore || !nextCursor || this._loadingMore) return;
    if (index < cards.length - 2) return;
    this._loadingMore = true;
    this.setData({ loadingMore: true });
    try {
      const prevLen = cards.length;
      const patch = await appendThemeFeedPage(selectedTheme, nextCursor, cards);
      this.setData({
        cards: patch.cards,
        nextCursor: patch.nextCursor,
        hasMore: patch.hasMore,
        endHint: this.data.swiperCurrent >= patch.cards.length - 1,
      });
      if (patch.cards.length > prevLen) {
        trackEvent('home_feed_load_more', {
          theme: selectedTheme,
          count: patch.cards.length - prevLen,
        });
      }
    } catch {
      /* 静默失败，用户可点刷新重试 */
    } finally {
      this._loadingMore = false;
      this.setData({ loadingMore: false });
    }
  },
});
