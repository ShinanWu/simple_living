/**
 * Gateway HTTP 客户端。契约以 services/gateway/api.md 为准。
 */
import { APP_VERSION, CLIENT_PLATFORM, resolvedDeviceId } from '../../config/env';
import {
  clearAuth,
  clearGuestSession,
  getGuestSessionId,
  loadTokenPair,
  saveTokenPair,
  setGuestSessionId,
  type TokenPair,
} from '../auth/storage';
import { GatewayBusinessError } from '../../utils/errors';
import type { ThemeKey } from '../../utils/theme';
import { parseEnvelope } from './parse-envelope';
import type {
  ApiEnvelope,
  AuthTokenPair,
  FavoriteItem,
  FeedItemContext,
  GatewayAPI,
  GuideDetailResponse,
  HistoryItem,
  HomeCard,
  HomeFeedResponse,
  MeSummaryResponse,
  Pagination,
  RedirectPrepareResponse,
} from './types';

type AuthMode = 'none' | 'bearer' | 'bearerOrGuest';

let refreshInFlight: Promise<AuthTokenPair> | null = null;

export class HttpGatewayAPI implements GatewayAPI {
  constructor(private readonly baseUrl: string) {}

  async getHomeFeed(theme: ThemeKey, pagination: Pagination): Promise<HomeFeedResponse> {
    const query = buildQuery({
      theme,
      limit: pagination.limit,
      cursor: pagination.cursor ?? undefined,
    });
    const data = await this.request<{
      items: Array<{
        recommendation_id: string;
        scene: string;
        rank: number;
        guide_card_id: string;
        reason_text: string;
        guide_card: {
          guide_card_id: string;
          title: string;
          summary?: string;
          subtitle?: string;
          cover_url?: string;
          theme?: string;
        };
      }>;
      pagination?: { next_cursor?: string | null; has_more?: boolean };
    }>('GET', `/api/v2/pages/home_feed${query}`, null, 'bearerOrGuest');

    const cards: HomeCard[] = data.items.map((item) => {
      const reason = item.reason_text.trim() || '-';
      return {
        id: `${item.recommendation_id}-${item.rank}`,
        guideCardId: item.guide_card_id,
        recommendationId: item.recommendation_id,
        scene: item.scene,
        itemRank: item.rank,
        title: item.guide_card.title,
        reason: reason || '-',
        coverUrl: item.guide_card.cover_url,
      };
    });
    return {
      cards,
      nextCursor: data.pagination?.next_cursor ?? null,
      hasMore: Boolean(data.pagination?.has_more),
    };
  }

  async getGuideDetail(guideCardId: string): Promise<GuideDetailResponse> {
    const query = buildQuery({
      guide_card_id: guideCardId,
    });
    const data = await this.request<{
      guide_card: {
        guide_card_id: string;
        title: string;
        summary?: string;
        subtitle?: string;
        cover_media?: { url?: string };
        commercial_disclosure: {
          is_commercial: boolean;
          disclosure_text_key?: string;
        };
      };
    }>('GET', `/api/v2/pages/guide_detail${query}`, null, 'bearerOrGuest');

    const card = data.guide_card;
    if (!card) {
      throw new GatewayBusinessError(30001, 'guide not found');
    }
    const coverUrl = card.cover_media?.url;
    const summary = card.summary ?? card.subtitle ?? '';
    return {
      guideCardId: card.guide_card_id,
      title: card.title,
      summary: summary || '暂无摘要',
      subtitle: card.subtitle,
      coverUrl,
      galleryUrls: coverUrl ? [coverUrl] : [],
      isCommercial: card.commercial_disclosure.is_commercial,
      disclosureText: card.commercial_disclosure.disclosure_text_key,
    };
  }

  async postRedirectPrepare(context: FeedItemContext): Promise<RedirectPrepareResponse> {
    await this.ensureGuestSession();
    const data = await this.request<{ landing_url: string; click_id?: string }>(
      'POST',
      '/api/v2/pages/redirect_prepare',
      {
        guide_card_id: context.guideCardId,
        recommendation_id: context.recommendationId,
        scene: context.scene,
        item_rank: context.itemRank,
      },
      'bearerOrGuest'
    );
    return { landingUrl: data.landing_url, clickId: data.click_id };
  }

  async getMeSummary(): Promise<MeSummaryResponse> {
    await this.ensureGuestSession();
    const data = await this.request<{
      profile: { user_id?: string | null; is_guest?: boolean; display_name?: string; avatar_url?: string };
      counts: { favorites_count: number; history_count: number };
      consent?: { personalization_allowed?: boolean };
    }>('GET', '/api/v2/pages/me_summary', null, 'bearerOrGuest');

    const isGuest = data.profile.is_guest ?? true;
    return {
      isLoggedIn: !isGuest,
      displayName: data.profile.display_name,
      avatarUrl: data.profile.avatar_url,
      favoritesCount: data.counts.favorites_count,
      historyCount: data.counts.history_count,
      consentGranted: data.consent?.personalization_allowed ?? false,
    };
  }

  async issueTokenWithPhone(
    phoneE164: string,
    otpCode: string,
    verificationId: string
  ): Promise<AuthTokenPair> {
    return this.postIssueToken({
      account_proof: {
        phone_otp: { phone_e164: phoneE164, otp_code: otpCode, verification_id: verificationId },
      },
      client_platform: CLIENT_PLATFORM,
      device_id: resolvedDeviceId(),
      app_version: APP_VERSION,
    });
  }

  async issueTokenWithWeChat(providerSubject: string, authorizationCode: string): Promise<AuthTokenPair> {
    return this.postIssueToken({
      account_proof: {
        oauth: {
          provider: 'wechat',
          provider_subject: providerSubject,
          authorization_code: authorizationCode,
        },
      },
      client_platform: CLIENT_PLATFORM,
      device_id: resolvedDeviceId(),
      app_version: APP_VERSION,
    });
  }

  async refreshAuthTokens(): Promise<AuthTokenPair> {
    const pair = loadTokenPair();
    if (!pair?.refreshToken) throw new GatewayBusinessError(20003, 'no refresh token');
    if (refreshInFlight) return refreshInFlight;

    refreshInFlight = (async () => {
      try {
        const data = await this.request<{
          access_token: string;
          refresh_token: string;
          expires_in?: number;
          session_id?: string;
        }>(
          'POST',
          '/api/v2/auth/token/refresh',
          {
            refresh_token: pair.refreshToken,
            request_context: {
              client_platform: CLIENT_PLATFORM,
              app_version: APP_VERSION,
              device_id: resolvedDeviceId(),
            },
          },
          'none'
        );
        const next = mapTokenPair(data);
        saveTokenPair(next);
        return next;
      } finally {
        refreshInFlight = null;
      }
    })();

    return refreshInFlight;
  }

  async logoutSession(revokeAllDevices: boolean): Promise<void> {
    try {
      const pair = loadTokenPair();
      if (pair?.accessToken) {
        await this.request<{ revoked?: boolean }>(
          'DELETE',
          '/api/v2/auth/session',
          { revoke_scope: revokeAllDevices ? 'all_user_sessions' : 'single_session' },
          'bearerOrGuest'
        );
      }
    } finally {
      clearAuth();
      clearGuestSession();
    }
  }

  async listFavorites(pagination: Pagination): Promise<{ items: FavoriteItem[]; nextCursor: string | null }> {
    const query = buildQuery({
      limit: pagination.limit,
      cursor: pagination.cursor ?? undefined,
    });
    const data = await this.request<{
      items: Array<{
        favorite_id: string;
        guide_card_id: string;
        favorited_at: string;
      }>;
      pagination?: { next_cursor?: string | null };
    }>('GET', `/api/v2/me/favorites${query}`, null, 'bearer');
    return {
      items: data.items.map((i) => ({
        favoriteId: i.favorite_id,
        guideCardId: i.guide_card_id,
        favoritedAt: i.favorited_at,
      })),
      nextCursor: data.pagination?.next_cursor ?? null,
    };
  }

  async addFavorite(guideCardId: string): Promise<{ favoriteId: string; alreadyFavorited: boolean }> {
    const data = await this.request<{ favorite_id: string; already_favorited: boolean }>(
      'POST',
      '/api/v2/me/favorites',
      { guide_card_id: guideCardId },
      'bearer'
    );
    return { favoriteId: data.favorite_id, alreadyFavorited: data.already_favorited };
  }

  async removeFavorite(favoriteId: string): Promise<void> {
    await this.request<{ removed: boolean }>(
      'DELETE',
      `/api/v2/me/favorites/${encodeURIComponent(favoriteId)}`,
      null,
      'bearer'
    );
  }

  async listHistory(pagination: Pagination): Promise<{ items: HistoryItem[]; nextCursor: string | null }> {
    const query = buildQuery({
      limit: pagination.limit,
      cursor: pagination.cursor ?? undefined,
    });
    const data = await this.request<{
      items: Array<{
        content_ref: { type: string; guide_card_id: string };
        last_seen_at: string;
      }>;
      pagination?: { next_cursor?: string | null };
    }>('GET', `/api/v2/me/history${query}`, null, 'bearerOrGuest');
    return {
      items: data.items.map((i) => ({
        guideCardId: i.content_ref.guide_card_id,
        lastSeenAt: i.last_seen_at,
      })),
      nextCursor: data.pagination?.next_cursor ?? null,
    };
  }

  async recordHistoryEvent(guideCardId: string, sourceSurface: string): Promise<void> {
    await this.ensureGuestSession();
    await this.request<{ recorded: boolean }>(
      'POST',
      '/api/v2/me/history/events',
      {
        content_ref: { type: 'guide_card', guide_card_id: guideCardId },
        source_surface: sourceSurface,
      },
      'bearerOrGuest'
    );
  }

  async clearHistory(): Promise<void> {
    await this.request<{ removed_count: number }>(
      'DELETE',
      '/api/v2/me/history',
      { scope: 'all' },
      'bearerOrGuest'
    );
  }

  private async postIssueToken(body: Record<string, unknown>): Promise<AuthTokenPair> {
    const data = await this.request<{
      access_token: string;
      refresh_token: string;
      expires_in?: number;
      session_id?: string;
    }>('POST', '/api/v2/auth/token/issue', body, 'none');
    const pair = mapTokenPair(data);
    saveTokenPair(pair);
    return pair;
  }

  private async ensureGuestSession(): Promise<void> {
    if (loadTokenPair()?.accessToken) return;
    if (getGuestSessionId()) return;

    const data = await this.request<{ session_id: string }>(
      'POST',
      '/api/v2/guest/session',
      {
        device_id: resolvedDeviceId(),
        client_platform: CLIENT_PLATFORM,
        app_version: APP_VERSION,
      },
      'none'
    );
    setGuestSessionId(data.session_id);
  }

  private async request<T>(
    method: 'GET' | 'POST' | 'DELETE',
    path: string,
    body: Record<string, unknown> | null,
    auth: AuthMode,
    retried = false
  ): Promise<T> {
    const url = `${this.baseUrl}${path}`;
    const headers: Record<string, string> = {
      Accept: 'application/json',
    };
    if (body !== null) headers['Content-Type'] = 'application/json';

    const pair = loadTokenPair();
    if (auth === 'bearer') {
      if (pair?.accessToken) headers.Authorization = `Bearer ${pair.accessToken}`;
    } else if (auth === 'bearerOrGuest') {
      if (pair?.accessToken) headers.Authorization = `Bearer ${pair.accessToken}`;
      else {
        const guest = getGuestSessionId();
        if (guest) headers['X-Guest-Session-Id'] = guest;
      }
    }

    const envelope = await this.wxRequest<T>(url, method, headers, body);
    if (envelope.success && envelope.code === 0 && envelope.data !== null) {
      return envelope.data;
    }

    if (!retried && envelope.code === 20002 && auth !== 'none' && pair?.refreshToken) {
      try {
        await this.refreshAuthTokens();
        return this.request<T>(method, path, body, auth, true);
      } catch {
        clearAuth();
        throw new GatewayBusinessError(20003, envelope.message);
      }
    }

    throw new GatewayBusinessError(envelope.code, envelope.message);
  }

  private wxRequest<T>(
    url: string,
    method: string,
    headers: Record<string, string>,
    body: Record<string, unknown> | null
  ): Promise<ApiEnvelope<T>> {
    return new Promise((resolve, reject) => {
      wx.request({
        url,
        method: method as WechatMiniprogram.RequestOption['method'],
        header: headers,
        data: body ?? undefined,
        success: (res) => {
          if (res.statusCode >= 500) {
            reject(new GatewayBusinessError(90001, `http ${res.statusCode}`));
            return;
          }
          try {
            const envelope = parseEnvelope<T>(res.data);
            resolve(envelope);
          } catch (e) {
            reject(e);
          }
        },
        fail: (err) => {
          reject({ kind: 'offline', errMsg: err.errMsg });
        },
      });
    });
  }
}

function buildQuery(params: Record<string, string | number | undefined>): string {
  const parts: string[] = [];
  for (const [key, value] of Object.entries(params)) {
    if (value === undefined) continue;
    parts.push(`${encodeURIComponent(key)}=${encodeURIComponent(String(value))}`);
  }
  return parts.length ? `?${parts.join('&')}` : '';
}

function mapTokenPair(data: {
  access_token: string;
  refresh_token: string;
  expires_in?: number;
  session_id?: string;
}): TokenPair {
  return {
    accessToken: data.access_token,
    refreshToken: data.refresh_token,
    expiresInSeconds: data.expires_in,
    sessionId: data.session_id,
  };
}
