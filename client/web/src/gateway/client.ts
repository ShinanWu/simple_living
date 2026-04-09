import type {
  ApiResponse,
  GuideDetailData,
  GuideDetailQuery,
  HomeFeedData,
  HomeFeedQuery,
  MeSummaryData,
  RedirectPrepareBody,
  RedirectPrepareData,
} from "./types";

export interface GatewayApiClient {
  getHomeFeed(query: HomeFeedQuery): Promise<ApiResponse<HomeFeedData>>;
  getGuideDetail(query: GuideDetailQuery): Promise<ApiResponse<GuideDetailData>>;
  postRedirectPrepare(
    body: RedirectPrepareBody,
  ): Promise<ApiResponse<RedirectPrepareData>>;
  getMeSummary(): Promise<ApiResponse<MeSummaryData>>;
}

const jsonHeaders = {
  "Content-Type": "application/json",
  "X-Client-Platform": "web",
  "X-Client-Version": "0.1.0",
};

export class HttpGatewayApiClient implements GatewayApiClient {
  constructor(private readonly baseUrl = "") {}

  async getHomeFeed(query: HomeFeedQuery): Promise<ApiResponse<HomeFeedData>> {
    const params = new URLSearchParams();
    params.set("theme", query.theme);
    if (query.cursor) params.set("cursor", query.cursor);
    if (typeof query.limit === "number") params.set("limit", String(query.limit));
    return this.request<ApiResponse<HomeFeedData>>(
      `/api/v2/pages/home_feed?${params.toString()}`,
      { method: "GET", headers: jsonHeaders },
    );
  }

  async getGuideDetail(
    query: GuideDetailQuery,
  ): Promise<ApiResponse<GuideDetailData>> {
    const params = new URLSearchParams();
    params.set("guide_card_id", query.guide_card_id);
    if (typeof query.include_related === "boolean") {
      params.set("include_related", String(query.include_related));
    }
    return this.request<ApiResponse<GuideDetailData>>(
      `/api/v2/pages/guide_detail?${params.toString()}`,
      { method: "GET", headers: jsonHeaders },
    );
  }

  async postRedirectPrepare(
    body: RedirectPrepareBody,
  ): Promise<ApiResponse<RedirectPrepareData>> {
    return this.request<ApiResponse<RedirectPrepareData>>(
      "/api/v2/pages/redirect_prepare",
      { method: "POST", headers: jsonHeaders, body: JSON.stringify(body) },
    );
  }

  async getMeSummary(): Promise<ApiResponse<MeSummaryData>> {
    return this.request<ApiResponse<MeSummaryData>>("/api/v2/pages/me_summary", {
      method: "GET",
      headers: jsonHeaders,
    });
  }

  private async request<T>(path: string, init: RequestInit): Promise<T> {
    const response = await fetch(`${this.baseUrl}${path}`, init);
    return (await response.json()) as T;
  }
}
