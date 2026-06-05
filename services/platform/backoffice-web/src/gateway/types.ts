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

export type BackofficePartnerStatus = "draft" | "active" | "disabled";

export interface BackofficePartner {
  partner_id: string;
  display_name: string;
  status: BackofficePartnerStatus;
  primary_channel_code: string;
}

export interface BackofficePartnersData {
  items: BackofficePartner[];
}

export interface BackofficeCreatePartnerBody {
  partner_id: string;
  display_name: string;
  status: BackofficePartnerStatus;
  primary_channel_code: string;
}

export type ResourceKind = "guide_card" | "editorial_content" | "topic" | "ranking_list";

export type BackofficeContentStatus =
  | "draft"
  | "in_review"
  | "published"
  | "scheduled"
  | "offline"
  | "archived";

export interface MediaRef {
  media_id?: string;
  url: string;
  type: "image" | "video" | "icon";
  width?: number;
  height?: number;
}

export interface AffiliateRef {
  channel: string;
  external_item_id: string;
  external_shop_id?: string;
  payload?: Record<string, string>;
}

export interface BackofficeContentItem {
  content_id: string;
  resource_kind: ResourceKind;
  title: string;
  subtitle?: string;
  theme_ids: string[];
  tag_ids?: string[];
  content_status: BackofficeContentStatus;
  revision: number;
  published_revision: number;
  summary?: string;
  cover_media?: MediaRef;
  landing_url?: string;
  external_item_id?: string;
  affiliate_refs?: AffiliateRef[];
  selling_points?: string[];
  commercial_disclosure_required?: boolean;
  effective_from?: string;
  effective_to?: string;
  created_at: string;
  updated_at: string;
  created_by?: string;
  updated_by?: string;
}

export interface ContentRevision {
  revision: number;
  created_at: string;
  created_by: string;
  change_summary: string;
}

export interface ReviewState {
  review_id: string;
  status: BackofficeReviewStatus;
  submitted_at: string;
  reviewer_id?: string;
  comment?: string;
}

export interface VisibilityVerdict {
  state: "published" | "unpublished" | "restricted";
  reason_code?: string;
  source: "review" | "manual_ops" | "policy" | "system";
  effective_from?: string;
  version: number;
}

export interface BackofficeContentDetailData {
  content: BackofficeContentItem;
  revision_history: ContentRevision[];
  review_state?: ReviewState;
  visibility_verdict?: VisibilityVerdict;
}

export interface BackofficeContentsData {
  items: BackofficeContentItem[];
}

export interface BackofficeCreateContentBody {
  resource_kind: ResourceKind;
  title: string;
  subtitle?: string;
  summary?: string;
  theme_ids?: string[];
  tag_ids?: string[];
  cover_media?: MediaRef;
  landing_url?: string;
  external_item_id?: string;
  affiliate_refs?: AffiliateRef[];
  selling_points?: string[];
  commercial_disclosure_required?: boolean;
  effective_from?: string;
  effective_to?: string;
  initial_status: BackofficeContentStatus;
}

export interface BackofficeUpdateContentBody {
  content_id: string;
  revision: number;
  title?: string;
  subtitle?: string;
  summary?: string;
  theme_ids?: string[];
  tag_ids?: string[];
  cover_media?: MediaRef;
  landing_url?: string;
  external_item_id?: string;
  affiliate_refs?: AffiliateRef[];
  selling_points?: string[];
  commercial_disclosure_required?: boolean;
  effective_from?: string;
  effective_to?: string;
  change_summary?: string;
}

export interface BackofficeUpdateContentStatusBody {
  content_id: string;
  status: Exclude<BackofficeContentStatus, "in_review">;
}

export interface BackofficeSubmitReviewBody {
  content_id: string;
  revision: number;
  change_summary?: string;
}

export interface BackofficePublishContentBody {
  content_id: string;
  revision: number;
}

export interface BackofficeRollbackContentBody {
  content_id: string;
  target_revision: number;
  change_summary?: string;
}

export type BackofficeReviewStatus =
  | "pending"
  | "in_review"
  | "approved"
  | "rejected"
  | "needs_info"
  | "completed";

export interface BackofficeReviewItem {
  review_id: string;
  subject_id: string;
  resource_kind?: ResourceKind;
  status: BackofficeReviewStatus;
  priority?: number;
  enqueue_reason?: string;
  enqueued_at: string;
  reviewer_id?: string;
  comment?: string;
  decided_at?: string;
}

export interface BackofficeReviewsData {
  items: BackofficeReviewItem[];
}

export interface BackofficeUpdateReviewBody {
  review_id: string;
  status: Exclude<BackofficeReviewStatus, "pending" | "in_review" | "completed">;
  comment?: string;
}
