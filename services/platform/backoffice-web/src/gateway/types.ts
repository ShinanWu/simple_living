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

export type BackofficeContentStatus = "draft" | "published";

export interface BackofficeContentItem {
  content_id: string;
  title: string;
  theme: string;
  status: BackofficeContentStatus;
}

export interface BackofficeContentsData {
  items: BackofficeContentItem[];
}

export interface BackofficeUpdateContentStatusBody {
  content_id: string;
  status: BackofficeContentStatus;
}

export type BackofficeReviewStatus = "pending" | "approved" | "rejected";

export interface BackofficeReviewItem {
  review_id: string;
  subject_id: string;
  status: BackofficeReviewStatus;
}

export interface BackofficeReviewsData {
  items: BackofficeReviewItem[];
}

export interface BackofficeUpdateReviewBody {
  review_id: string;
  status: Exclude<BackofficeReviewStatus, "pending">;
}
