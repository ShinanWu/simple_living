import { BRAND_DEFAULT_USER } from '../../config/brand';
import type { ThemeKey } from '../../utils/theme';
import { galleryForTheme } from '../../utils/theme';
import { GatewayBusinessError } from '../../utils/errors';
import type {
  AuthTokenPair,
  FavoriteItem,
  FeedItemContext,
  GatewayAPI,
  GuideDetailResponse,
  HistoryItem,
  HomeFeedResponse,
  MeSummaryResponse,
  Pagination,
  RedirectPrepareResponse,
} from './types';

function delay(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

export class MockGatewayAPI implements GatewayAPI {
  private loggedIn = false;
  private mockRefresh = '';
  private favorites: FavoriteItem[] = [];
  private history: HistoryItem[] = [];
  simulateOffline = false;

  async getHomeFeed(theme: ThemeKey, _pagination: Pagination): Promise<HomeFeedResponse> {
    await delay(180);
    if (this.simulateOffline) throw { kind: 'offline', errMsg: 'request:fail' };
    const recId = `rec_${theme}_page1`;
    return {
      cards: [
        {
          id: `${recId}-1`,
          guideCardId: `guide_${theme}_001`,
          recommendationId: recId,
          scene: 'home_feed',
          itemRank: 1,
          title: `${theme}主题精选 1`,
          reason: '基于主题与基础偏好生成',
        },
        {
          id: `${recId}-2`,
          guideCardId: `guide_${theme}_002`,
          recommendationId: recId,
          scene: 'home_feed',
          itemRank: 2,
          title: `${theme}主题精选 2`,
          reason: '结合历史行为做轻量排序',
        },
      ],
      nextCursor: null,
    };
  }

  async getGuideDetail(guideCardId: string): Promise<GuideDetailResponse> {
    await delay(140);
    const themeGuess = (guideCardId.split('_')[1] ?? 'clothing') as ThemeKey;
    const gallery = galleryForTheme(themeGuess);
    return {
      guideCardId,
      title: '导购详情（Mock）',
      summary: '用于承接详情页布局与字段映射。完整内容可在联调环境从 gateway 拉取。',
      subtitle: '三件够用一周',
      galleryUrls: [],
      isCommercial: true,
      disclosureText: '含商业合作推广',
      coverUrl: undefined,
    };
  }

  async postRedirectPrepare(context: FeedItemContext): Promise<RedirectPrepareResponse> {
    await delay(120);
    await this.ensureGuest();
    return {
      landingUrl: `https://example.com/landing/${context.guideCardId}?rec=${context.recommendationId}`,
      clickId: `click_mock_${Date.now()}`,
    };
  }

  async getMeSummary(): Promise<MeSummaryResponse> {
    await delay(120);
    await this.ensureGuest();
    return {
      isLoggedIn: this.loggedIn,
      displayName: this.loggedIn ? BRAND_DEFAULT_USER : undefined,
      favoritesCount: this.loggedIn ? this.favorites.length : 0,
      historyCount: this.history.length,
      consentGranted: true,
    };
  }

  async issueTokenWithPhone(
    _phoneE164: string,
    _otpCode: string,
    _verificationId: string
  ): Promise<AuthTokenPair> {
    await delay(150);
    if (this.simulateOffline) throw { kind: 'offline' };
    this.loggedIn = true;
    this.mockRefresh = 'mock_rt_phone';
    return this.pair();
  }

  async issueTokenWithWeChat(
    providerSubject: string,
    authorizationCode: string
  ): Promise<AuthTokenPair> {
    await delay(150);
    if (this.simulateOffline) throw { kind: 'offline' };
    if (!authorizationCode) {
      throw new GatewayBusinessError(20006, 'invalid credentials');
    }
    this.loggedIn = true;
    this.mockRefresh = 'mock_rt_wx';
    if (providerSubject && !providerSubject.startsWith('wx_code_')) {
      /* keep openid */
    }
    return this.pair();
  }

  async refreshAuthTokens(): Promise<AuthTokenPair> {
    await delay(80);
    if (!this.mockRefresh) throw new GatewayBusinessError(20003, 'refresh invalid');
    this.mockRefresh = 'mock_rt_rotated';
    return this.pair();
  }

  async logoutSession(_revokeAllDevices: boolean): Promise<void> {
    await delay(80);
    this.loggedIn = false;
    this.mockRefresh = '';
  }

  async listFavorites(_pagination: Pagination): Promise<{ items: FavoriteItem[]; nextCursor: string | null }> {
    await delay(100);
    if (!this.loggedIn) throw new GatewayBusinessError(20001, 'auth required');
    return { items: this.favorites, nextCursor: null };
  }

  async addFavorite(guideCardId: string): Promise<{ favoriteId: string; alreadyFavorited: boolean }> {
    await delay(100);
    if (!this.loggedIn) throw new GatewayBusinessError(20001, 'auth required');
    const existing = this.favorites.find((f) => f.guideCardId === guideCardId);
    if (existing) return { favoriteId: existing.favoriteId, alreadyFavorited: true };
    const favoriteId = `fav_mock_${this.favorites.length + 1}`;
    this.favorites.push({
      favoriteId,
      guideCardId,
      favoritedAt: new Date().toISOString(),
    });
    return { favoriteId, alreadyFavorited: false };
  }

  async listHistory(_pagination: Pagination): Promise<{ items: HistoryItem[]; nextCursor: string | null }> {
    await delay(100);
    return { items: this.history, nextCursor: null };
  }

  async recordHistoryEvent(guideCardId: string, _sourceSurface: string): Promise<void> {
    const existing = this.history.find((h) => h.guideCardId === guideCardId);
    if (existing) {
      existing.lastSeenAt = new Date().toISOString();
    } else {
      this.history.unshift({ guideCardId, lastSeenAt: new Date().toISOString() });
    }
  }

  private pair(): AuthTokenPair {
    return {
      accessToken: 'mock_at',
      refreshToken: this.mockRefresh,
      expiresInSeconds: 3600,
      sessionId: 'sess_mock',
    };
  }

  private async ensureGuest(): Promise<void> {
    /* no-op for mock */
  }
}
