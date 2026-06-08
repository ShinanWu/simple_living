import { BRAND_NAME, BRAND_FEED_END_HINT } from '../../config/brand';
import { setLastTheme } from '../../services/auth/storage';
import { THEMES, nextTheme, themeByKey, type ThemeKey } from '../../utils/theme';
import { trackEvent } from '../../utils/analytics';
import { setBrandNavigationTitle } from '../../utils/navigation';
import {
  feedContextFromCard,
  guideDetailPath,
  redirectPreparePath,
} from '../../utils/guide-route';
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

const NEXT_THRESHOLD = 90;
const REFRESH_HINT = 28;
const REFRESH_TRIGGER = 92;
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
    cardIndex: 0,
    currentCard: {} as DisplayCard,
    cardOffset: 0,
    pullHint: '',
    endHint: false,
    nextCursor: null as string | null,
    hasMore: false,
    loadingMore: false,
    peekCard: null as DisplayCard | null,
    peekOffset: 0,
    showBackToTop: false,
  },

  _loadingMore: false,
  _isCardDragging: false,
  _touchStartY: 0,
  _touchStartX: 0,
  _themeTouchX: 0,
  _themeTouchY: 0,
  _isRefreshing: false,
  _refreshArmed: false,

  onLoad() {
    const app = getApp<IAppOption>();
    const theme = (app.globalData.lastTheme || 'clothing') as ThemeKey;
    this.setData({ selectedTheme: theme, themeMeta: themeByKey(theme) });
    void this.bootstrapTheme(theme);
  },

  onShow() {
    setBrandNavigationTitle();
    trackEvent('home_page_show');
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
    const cardIndex = 0;
    const cards = snapshot.cards;
    this.setData({
      selectedTheme: theme,
      themeMeta: themeByKey(theme),
      loadState: snapshot.loadState,
      errorMessage: snapshot.errorMessage,
      cards,
      cardIndex,
      currentCard: cards[0] ?? ({} as DisplayCard),
      cardOffset: 0,
      pullHint: '',
      endHint: cards.length > 0 && cardIndex >= cards.length - 1,
      nextCursor: snapshot.nextCursor,
      hasMore: snapshot.hasMore,
      peekCard: null,
      peekOffset: 0,
      showBackToTop: false,
    });
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
    this.setData({ loadState: 'loading', pullHint: '', endHint: false });
    this._isRefreshing = true;
    try {
      const snapshot = await refreshThemeFeed(theme);
      this.applySnapshot(theme, snapshot);
      trackEvent('home_feed_refresh', { theme });
    } finally {
      this._isRefreshing = false;
    }
  },

  onBackToTop() {
    const { cards } = this.data;
    if (!cards.length) return;
    this.setData({
      cardIndex: 0,
      currentCard: cards[0],
      endHint: cards.length === 1,
      showBackToTop: false,
      cardOffset: 0,
      pullHint: '',
      peekCard: null,
      peekOffset: 0,
    });
    trackEvent('home_back_to_top');
  },

  onCardTouchStart(e: WechatMiniprogram.TouchEvent) {
    this._touchStartY = e.touches[0].clientY;
    this._touchStartX = e.touches[0].clientX;
    this._isCardDragging = false;
    this._refreshArmed = false;
  },

  onCardTouchMove(e: WechatMiniprogram.TouchEvent) {
    const dy = e.touches[0].clientY - this._touchStartY;
    const dx = e.touches[0].clientX - this._touchStartX;
    if (Math.abs(dy) > 10 || Math.abs(dx) > 10) this._isCardDragging = true;
    if (Math.abs(dx) > Math.abs(dy) && Math.abs(dx) > THEME_SWIPE) return;
    if (Math.abs(dy) <= Math.abs(dx)) return;

    const { cardIndex, cards } = this.data;
    const isFirst = cardIndex === 0;
    const isLast = cardIndex >= cards.length - 1;
    let offset = 0;
    let pullHint = '';
    let endHint = false;
    let peekCard: DisplayCard | null = null;
    let peekOffset = 0;

    if (dy > 0 && isFirst) {
      const pull = Math.min(dy, REFRESH_TRIGGER + 24);
      offset = pull * 0.35;
      if (pull >= REFRESH_HINT) {
        pullHint = pull >= REFRESH_TRIGGER ? '松手刷新' : '继续下滑刷新';
      }
      this._refreshArmed = pull >= REFRESH_TRIGGER;
    } else if (dy < 0 && isLast) {
      offset = Math.max(dy * 0.35, -120);
      endHint = true;
      this._refreshArmed = false;
    } else if (dy > 0 && !isFirst) {
      offset = Math.min(dy, 280);
      peekCard = cards[cardIndex - 1];
      peekOffset = offset - 420;
      this._refreshArmed = false;
    } else if (dy < 0 && !isLast) {
      offset = Math.max(dy, -280);
      peekCard = cards[cardIndex + 1];
      peekOffset = offset + 420;
      this._refreshArmed = false;
    } else {
      this._refreshArmed = false;
    }

    this.setData({ cardOffset: offset, pullHint, endHint, peekCard, peekOffset });
  },

  onCardTouchEnd(e: WechatMiniprogram.TouchEvent) {
    const dy = e.changedTouches[0].clientY - this._touchStartY;
    const dx = e.changedTouches[0].clientX - this._touchStartX;

    if (Math.abs(dx) > Math.abs(dy) && Math.abs(dx) > THEME_SWIPE) {
      const dir = dx < 0 ? 1 : -1;
      const next = nextTheme(this.data.selectedTheme, dir);
      if (next !== this.data.selectedTheme) this.switchTheme(next);
      this.resetCardMotion();
      return;
    }

    const { cardIndex, cards } = this.data;
    const shouldNext = dy < -NEXT_THRESHOLD;
    const shouldPrev = dy > NEXT_THRESHOLD;
    const shouldRefresh = cardIndex === 0 && this._refreshArmed && dy >= REFRESH_TRIGGER;

    if (shouldNext && cardIndex + 1 < cards.length) {
      const nextIndex = cardIndex + 1;
      this.setData({
        cardIndex: nextIndex,
        currentCard: cards[nextIndex],
        endHint: nextIndex >= cards.length - 1,
        showBackToTop: nextIndex > 0,
      });
      trackEvent('home_card_swipe', { direction: 'next' });
      void this.maybeLoadMore(nextIndex);
    } else if (shouldPrev && cardIndex > 0) {
      const prevIndex = cardIndex - 1;
      this.setData({
        cardIndex: prevIndex,
        currentCard: cards[prevIndex],
        endHint: false,
        showBackToTop: prevIndex > 0,
      });
      trackEvent('home_card_swipe', { direction: 'prev' });
    } else if (shouldRefresh && !this._isRefreshing) {
      void this.refreshCurrentTheme();
    }

    setTimeout(() => {
      this._isCardDragging = false;
    }, 120);
    this.resetCardMotion();
  },

  resetCardMotion() {
    this.setData({ cardOffset: 0, pullHint: '', peekCard: null, peekOffset: 0 });
    this._refreshArmed = false;
  },

  async maybeLoadMore(cardIndex: number) {
    const { cards, nextCursor, hasMore, selectedTheme } = this.data;
    if (!hasMore || !nextCursor || this._loadingMore) return;
    if (cardIndex < cards.length - 2) return;
    this._loadingMore = true;
    this.setData({ loadingMore: true });
    try {
      const prevLen = cards.length;
      const patch = await appendThemeFeedPage(selectedTheme, nextCursor, cards);
      this.setData({
        cards: patch.cards,
        nextCursor: patch.nextCursor,
        hasMore: patch.hasMore,
        endHint: this.data.cardIndex >= patch.cards.length - 1,
      });
      if (patch.cards.length > prevLen) {
        trackEvent('home_feed_load_more', {
          theme: selectedTheme,
          count: patch.cards.length - prevLen,
        });
      }
    } catch {
      /* 静默失败，用户可下拉刷新重试 */
    } finally {
      this._loadingMore = false;
      this.setData({ loadingMore: false });
    }
  },

  openDetail() {
    if (this._isCardDragging) return;
    const card = this.data.currentCard;
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

  goBuy() {
    if (this._isCardDragging) return;
    const card = this.data.currentCard;
    if (!card?.guideCardId) return;
    wx.navigateTo({ url: redirectPreparePath(feedContextFromCard(card), card.title) });
  },
});
