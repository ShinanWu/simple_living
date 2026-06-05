import { BRAND_NAME, BRAND_FEED_END_HINT } from '../../config/brand';
import type { HomeCard } from '../../services/gateway/types';
import { getGateway } from '../../services/gateway/runtime';
import { setLastTheme } from '../../services/auth/storage';
import { THEMES, nextTheme, themeByKey, type ThemeKey } from '../../utils/theme';
import { GatewayBusinessError } from '../../utils/errors';
import { trackEvent } from '../../utils/analytics';
import { setBrandNavigationTitle } from '../../utils/navigation';

const NEXT_THRESHOLD = 90;
const REFRESH_HINT = 28;
const REFRESH_TRIGGER = 92;
const THEME_SWIPE = 54;

type LoadState = 'loading' | 'success' | 'empty' | 'error' | 'offline';

Page({
  data: {
    brandName: BRAND_NAME,
    brandEndHint: BRAND_FEED_END_HINT,
    themes: THEMES,
    selectedTheme: 'clothing' as ThemeKey,
    themeMeta: themeByKey('clothing'),
    loadState: 'loading' as LoadState,
    errorMessage: '',
    cards: [] as Array<HomeCard & { reasonDisplay: string }>,
    cardIndex: 0,
    currentCard: {} as HomeCard & { reasonDisplay: string },
    cardOffset: 0,
    pullHint: '',
    endHint: false,
  },

  _loadToken: 0,
  _isCardDragging: false,
  _touchStartY: 0,
  _touchStartX: 0,
  _themeTouchX: 0,
  _themeTouchY: 0,
  _isRefreshing: false,

  onLoad() {
    const app = getApp<IAppOption>();
    const theme = (app.globalData.lastTheme || 'clothing') as ThemeKey;
    this.setData({ selectedTheme: theme, themeMeta: themeByKey(theme) });
    this.loadFeed(theme);
  },

  onShow() {
    setBrandNavigationTitle();
    trackEvent('home_page_show');
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
    this.setData({
      selectedTheme: theme,
      themeMeta: themeByKey(theme),
      cardIndex: 0,
      endHint: false,
    });
    trackEvent('home_theme_switch', { theme, reselect: isReselect });
    this.loadFeed(theme);
  },

  onRetry() {
    this.loadFeed(this.data.selectedTheme);
  },

  async loadFeed(theme: ThemeKey) {
    const token = ++this._loadToken;
    this.setData({ loadState: 'loading', pullHint: '', endHint: false });
    const api = getGateway();
    try {
      const res = await api.getHomeFeed(theme, { limit: 20 });
      if (token !== this._loadToken) return;
      const cards = res.cards.map((c) => ({
        ...c,
        reasonDisplay: c.reason === '-' ? '为你精选的轻量推荐' : c.reason,
      }));
      if (!cards.length) {
        this.setData({ loadState: 'empty', cards: [], currentCard: {} as never });
        return;
      }
      this.setData({
        loadState: 'success',
        cards,
        cardIndex: 0,
        currentCard: cards[0],
        cardOffset: 0,
      });
    } catch (err) {
      if (token !== this._loadToken) return;
      if (this.isOffline(err)) {
        this.setData({ loadState: 'offline' });
        return;
      }
      const message =
        err instanceof GatewayBusinessError ? err.message : '加载失败，请稍后重试';
      this.setData({ loadState: 'error', errorMessage: message });
    } finally {
      this._isRefreshing = false;
    }
  },

  onCardTouchStart(e: WechatMiniprogram.TouchEvent) {
    this._touchStartY = e.touches[0].clientY;
    this._touchStartX = e.touches[0].clientX;
    this._isCardDragging = false;
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

    if (dy > 0 && isFirst) {
      const pull = Math.min(dy, REFRESH_TRIGGER + 24);
      offset = pull * 0.35;
      if (pull >= REFRESH_HINT) {
        pullHint = pull >= REFRESH_TRIGGER ? '松手刷新' : '继续下滑刷新';
      }
    } else if (dy < 0 && isLast) {
      offset = Math.max(dy * 0.35, -120);
      endHint = true;
    } else if (dy > 0 && !isFirst) {
      offset = Math.min(dy, 280);
    } else if (dy < 0 && !isLast) {
      offset = Math.max(dy, -280);
    }

    this.setData({ cardOffset: offset, pullHint, endHint });
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
    const shouldRefresh =
      cardIndex === 0 && (dy > REFRESH_TRIGGER || e.changedTouches[0].clientY - this._touchStartY > REFRESH_TRIGGER);

    if (shouldNext && cardIndex + 1 < cards.length) {
      const nextIndex = cardIndex + 1;
      this.setData({
        cardIndex: nextIndex,
        currentCard: cards[nextIndex],
        endHint: nextIndex >= cards.length - 1,
      });
      trackEvent('home_card_swipe', { direction: 'next' });
    } else if (shouldPrev && cardIndex > 0) {
      const prevIndex = cardIndex - 1;
      this.setData({ cardIndex: prevIndex, currentCard: cards[prevIndex], endHint: false });
      trackEvent('home_card_swipe', { direction: 'prev' });
    } else if (shouldRefresh && !this._isRefreshing) {
      this._isRefreshing = true;
      trackEvent('home_feed_refresh', { theme: this.data.selectedTheme });
      this.loadFeed(this.data.selectedTheme);
    }

    setTimeout(() => {
      this._isCardDragging = false;
    }, 120);
    this.resetCardMotion();
  },

  resetCardMotion() {
    this.setData({ cardOffset: 0, pullHint: '' });
  },

  openDetail() {
    if (this._isCardDragging) return;
    const card = this.data.currentCard;
    if (!card?.guideCardId) return;
    const q = [
      `guide_card_id=${encodeURIComponent(card.guideCardId)}`,
      `recommendation_id=${encodeURIComponent(card.recommendationId)}`,
      `scene=${encodeURIComponent(card.scene)}`,
      `item_rank=${card.itemRank}`,
      `title=${encodeURIComponent(card.title)}`,
      `reason=${encodeURIComponent(card.reason)}`,
      `theme=${this.data.selectedTheme}`,
    ].join('&');
    wx.navigateTo({ url: `/pages/guide-detail/guide-detail?${q}` });
  },

  isOffline(err: unknown): boolean {
    return Boolean(err && typeof err === 'object' && (err as { kind?: string }).kind === 'offline');
  },
});
