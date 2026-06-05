import type { ThemeKey } from '../../utils/theme';

export interface Pagination {
  cursor?: string | null;
  limit: number;
}

export interface FeedItemContext {
  guideCardId: string;
  recommendationId: string;
  scene: string;
  itemRank: number;
}

export interface HomeCard {
  id: string;
  guideCardId: string;
  recommendationId: string;
  scene: string;
  itemRank: number;
  title: string;
  reason: string;
  coverUrl?: string;
}

export interface HomeFeedResponse {
  cards: HomeCard[];
  nextCursor: string | null;
}

export interface GuideDetailResponse {
  guideCardId: string;
  title: string;
  summary: string;
  subtitle?: string;
  coverUrl?: string;
  galleryUrls: string[];
  isCommercial: boolean;
  disclosureText?: string;
}

export interface RedirectPrepareResponse {
  landingUrl: string;
  clickId?: string;
}

export interface MeSummaryResponse {
  isLoggedIn: boolean;
  displayName?: string;
  avatarUrl?: string;
  favoritesCount: number;
  historyCount: number;
  consentGranted: boolean;
}

export interface AuthTokenPair {
  accessToken: string;
  refreshToken: string;
  expiresInSeconds?: number;
  sessionId?: string;
}

export interface FavoriteItem {
  favoriteId: string;
  guideCardId: string;
  favoritedAt: string;
}

export interface HistoryItem {
  guideCardId: string;
  lastSeenAt: string;
}

export interface GatewayAPI {
  getHomeFeed(theme: ThemeKey, pagination: Pagination): Promise<HomeFeedResponse>;
  getGuideDetail(guideCardId: string): Promise<GuideDetailResponse>;
  postRedirectPrepare(context: FeedItemContext): Promise<RedirectPrepareResponse>;
  getMeSummary(): Promise<MeSummaryResponse>;
  issueTokenWithPhone(phoneE164: string, otpCode: string, verificationId: string): Promise<AuthTokenPair>;
  issueTokenWithWeChat(providerSubject: string, authorizationCode: string): Promise<AuthTokenPair>;
  refreshAuthTokens(): Promise<AuthTokenPair>;
  logoutSession(revokeAllDevices: boolean): Promise<void>;
  listFavorites(pagination: Pagination): Promise<{ items: FavoriteItem[]; nextCursor: string | null }>;
  addFavorite(guideCardId: string): Promise<{ favoriteId: string; alreadyFavorited: boolean }>;
  listHistory(pagination: Pagination): Promise<{ items: HistoryItem[]; nextCursor: string | null }>;
  recordHistoryEvent(guideCardId: string, sourceSurface: string): Promise<void>;
}

export interface ApiEnvelope<T> {
  success: boolean;
  code: number;
  message: string;
  data: T | null;
}
