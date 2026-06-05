import type {
  ApiResponse,
  BackofficeCreateContentBody,
  BackofficeContentsData,
  BackofficeContentItem,
  BackofficeCreatePartnerBody,
  BackofficePartnersData,
  BackofficeReviewsData,
  BackofficeUpdateContentStatusBody,
  BackofficeUpdateReviewBody,
  BackofficeUpdateContentBody,
  BackofficeContentDetailData,
  BackofficeSubmitReviewBody,
  BackofficePublishContentBody,
  BackofficeRollbackContentBody,
} from "./types";

export interface GatewayApiClient {
  getBackofficePartners(): Promise<ApiResponse<BackofficePartnersData>>;
  postBackofficePartner(
    body: BackofficeCreatePartnerBody,
  ): Promise<ApiResponse<BackofficePartnersData>>;
  getBackofficeContents(): Promise<ApiResponse<BackofficeContentsData>>;
  postBackofficeContentItem(
    body: BackofficeCreateContentBody,
  ): Promise<ApiResponse<BackofficeContentsData>>;
  patchBackofficeContentStatus(
    body: BackofficeUpdateContentStatusBody,
  ): Promise<ApiResponse<BackofficeContentsData>>;
  getBackofficeContentDetail(
    contentId: string,
  ): Promise<ApiResponse<BackofficeContentDetailData>>;
  updateBackofficeContent(
    body: BackofficeUpdateContentBody,
  ): Promise<ApiResponse<BackofficeContentsData>>;
  submitBackofficeContentReview(
    body: BackofficeSubmitReviewBody,
  ): Promise<ApiResponse<BackofficeContentsData>>;
  publishBackofficeContent(
    body: BackofficePublishContentBody,
  ): Promise<ApiResponse<BackofficeContentsData>>;
  rollbackBackofficeContent(
    body: BackofficeRollbackContentBody,
  ): Promise<ApiResponse<BackofficeContentsData>>;
  getBackofficeReviews(): Promise<ApiResponse<BackofficeReviewsData>>;
  patchBackofficeReview(
    body: BackofficeUpdateReviewBody,
  ): Promise<ApiResponse<BackofficeReviewsData>>;
}

const jsonHeaders = {
  "Content-Type": "application/json",
};

function normalizeContentItem(raw: Record<string, unknown>): BackofficeContentItem {
  return {
    content_id: (raw.content_id as string) ?? "",
    resource_kind: (raw.resource_kind as BackofficeContentItem["resource_kind"]) ?? "guide_card",
    title: (raw.title as string) ?? "",
    subtitle: (raw.subtitle as string) ?? undefined,
    theme_ids: Array.isArray(raw.theme_ids)
      ? raw.theme_ids
      : raw.theme
        ? [raw.theme as string]
        : [],
    tag_ids: (raw.tag_ids as string[]) ?? undefined,
    content_status: ((raw.content_status ?? raw.status) as BackofficeContentItem["content_status"]) ?? "draft",
    revision: (raw.revision as number) ?? 1,
    published_revision: (raw.published_revision as number) ?? (raw.status === "published" ? 1 : 0),
    summary: (raw.summary as string) ?? undefined,
    cover_media: (raw.cover_media as BackofficeContentItem["cover_media"]) ?? undefined,
    landing_url: (raw.landing_url as string) ?? undefined,
    external_item_id: (raw.external_item_id as string) ?? undefined,
    affiliate_refs: (raw.affiliate_refs as BackofficeContentItem["affiliate_refs"]) ?? undefined,
    selling_points: (raw.selling_points as string[]) ?? undefined,
    commercial_disclosure_required: (raw.commercial_disclosure_required as boolean) ?? undefined,
    effective_from: (raw.effective_from as string) ?? undefined,
    effective_to: (raw.effective_to as string) ?? undefined,
    created_at: (raw.created_at as string) ?? new Date().toISOString(),
    updated_at: (raw.updated_at as string) ?? new Date().toISOString(),
    created_by: (raw.created_by as string) ?? undefined,
    updated_by: (raw.updated_by as string) ?? undefined,
  };
}

export class HttpGatewayApiClient implements GatewayApiClient {
  constructor(private readonly baseUrl = "") {}

  async getBackofficePartners(): Promise<ApiResponse<BackofficePartnersData>> {
    return this.request<ApiResponse<BackofficePartnersData>>("/api/v2/backoffice/affiliate/partners", {
      method: "POST",
      headers: jsonHeaders,
      body: "{}",
    });
  }

  async postBackofficePartner(
    body: BackofficeCreatePartnerBody,
  ): Promise<ApiResponse<BackofficePartnersData>> {
    return this.request<ApiResponse<BackofficePartnersData>>("/api/v2/backoffice/affiliate/partners/add", {
      method: "POST",
      headers: jsonHeaders,
      body: JSON.stringify(body),
    });
  }

  async getBackofficeContents(): Promise<ApiResponse<BackofficeContentsData>> {
    const resp = await this.request<ApiResponse<BackofficeContentsData>>("/api/v2/backoffice/content/items", {
      method: "POST",
      headers: jsonHeaders,
      body: "{}",
    });
    if (resp.data?.items) {
      resp.data.items = resp.data.items.map((item) =>
        normalizeContentItem(item as unknown as Record<string, unknown>),
      );
    }
    return resp;
  }

  async postBackofficeContentItem(
    body: BackofficeCreateContentBody,
  ): Promise<ApiResponse<BackofficeContentsData>> {
    return this.request<ApiResponse<BackofficeContentsData>>("/api/v2/backoffice/content/items/add", {
      method: "POST",
      headers: jsonHeaders,
      body: JSON.stringify(body),
    });
  }

  async patchBackofficeContentStatus(
    body: BackofficeUpdateContentStatusBody,
  ): Promise<ApiResponse<BackofficeContentsData>> {
    return this.request<ApiResponse<BackofficeContentsData>>("/api/v2/backoffice/content/items/status", {
      method: "POST",
      headers: jsonHeaders,
      body: JSON.stringify(body),
    });
  }

  async getBackofficeContentDetail(
    contentId: string,
  ): Promise<ApiResponse<BackofficeContentDetailData>> {
    return this.request<ApiResponse<BackofficeContentDetailData>>("/api/v2/backoffice/content/items/detail", {
      method: "POST",
      headers: jsonHeaders,
      body: JSON.stringify({ content_id: contentId }),
    });
  }

  async updateBackofficeContent(
    body: BackofficeUpdateContentBody,
  ): Promise<ApiResponse<BackofficeContentsData>> {
    return this.request<ApiResponse<BackofficeContentsData>>("/api/v2/backoffice/content/items/update", {
      method: "POST",
      headers: jsonHeaders,
      body: JSON.stringify(body),
    });
  }

  async submitBackofficeContentReview(
    body: BackofficeSubmitReviewBody,
  ): Promise<ApiResponse<BackofficeContentsData>> {
    return this.request<ApiResponse<BackofficeContentsData>>("/api/v2/backoffice/content/items/submit-review", {
      method: "POST",
      headers: jsonHeaders,
      body: JSON.stringify(body),
    });
  }

  async publishBackofficeContent(
    body: BackofficePublishContentBody,
  ): Promise<ApiResponse<BackofficeContentsData>> {
    return this.request<ApiResponse<BackofficeContentsData>>("/api/v2/backoffice/content/items/publish", {
      method: "POST",
      headers: jsonHeaders,
      body: JSON.stringify(body),
    });
  }

  async rollbackBackofficeContent(
    body: BackofficeRollbackContentBody,
  ): Promise<ApiResponse<BackofficeContentsData>> {
    return this.request<ApiResponse<BackofficeContentsData>>("/api/v2/backoffice/content/items/rollback", {
      method: "POST",
      headers: jsonHeaders,
      body: JSON.stringify(body),
    });
  }

  async getBackofficeReviews(): Promise<ApiResponse<BackofficeReviewsData>> {
    return this.request<ApiResponse<BackofficeReviewsData>>("/api/v2/backoffice/governance/reviews", {
      method: "POST",
      headers: jsonHeaders,
      body: "{}",
    });
  }

  async patchBackofficeReview(
    body: BackofficeUpdateReviewBody,
  ): Promise<ApiResponse<BackofficeReviewsData>> {
    return this.request<ApiResponse<BackofficeReviewsData>>("/api/v2/backoffice/governance/reviews/status", {
      method: "POST",
      headers: jsonHeaders,
      body: JSON.stringify(body),
    });
  }

  private async request<T>(path: string, init: RequestInit): Promise<T> {
    try {
      const response = await fetch(`${this.baseUrl}${path}`, init);
      if (!response.ok) {
        const text = await response.text();
        if (text.includes("Fail to find method")) {
          return { success: false, code: -2, message: `该功能尚未实现：${path}` } as T;
        }
        return { success: false, code: response.status, message: `请求失败: ${text || response.statusText}` } as T;
      }
      return (await response.json()) as T;
    } catch {
      return { success: false, code: -1, message: "网络请求失败" } as T;
    }
  }
}
