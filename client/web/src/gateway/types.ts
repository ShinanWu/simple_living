import type { Theme } from "../types/theme";

export interface ResponseMeta {
  request_id: string;
  trace_id?: string;
  server_time_ms: number;
}

export interface ApiResponse<TData> {
  success: boolean;
  code: number;
  message: string;
  data?: TData;
  meta?: ResponseMeta;
}

export interface Pagination {
  next_cursor: string;
  has_more: boolean;
  limit: number;
}

export interface HomeFeedItem {
  recommendation_id: string;
  scene: string;
  rank: number;
  guide_card_id: string;
  reason_tags: string[];
}

export interface HomeFeedData {
  items: HomeFeedItem[];
  pagination: Pagination;
}

export interface HomeFeedQuery {
  cursor?: string;
  limit?: number;
  theme: Theme;
}

export interface GuideCard {
  guide_card_id: string;
  schema_version: number;
  title: string;
  theme: Theme;
  published_at: string;
  updated_at: string;
  commercial_disclosure: {
    is_commercial: boolean;
  };
}

export interface GuideDetailData {
  guide: GuideCard;
  disclosures?: Record<string, string>;
  related?: HomeFeedItem[];
}

export interface GuideDetailQuery {
  guide_card_id: string;
  include_related?: boolean;
}

export interface RedirectPrepareBody {
  guide_card_id: string;
  recommendation_id?: string;
  scene?: string;
  item_rank?: number;
  preferred_channel_code?: string;
}

export interface RedirectPrepareData {
  landing_url: string;
  click_id?: string;
  expires_at?: string;
  attribution?: Record<string, string>;
}

export interface MeSummaryData {
  profile: {
    user_id: string;
    is_guest: boolean;
  };
  counts: {
    favorites_count: number;
    history_count: number;
  };
  consent: {
    personalization_allowed: boolean;
    consent_version: string;
    updated_at: string;
  };
}
