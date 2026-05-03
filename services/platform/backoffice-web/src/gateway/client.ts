import type {
  ApiResponse,
  BackofficeContentsData,
  BackofficeCreatePartnerBody,
  BackofficePartnersData,
  BackofficeReviewsData,
  BackofficeUpdateContentStatusBody,
  BackofficeUpdateReviewBody,
} from "./types";

export interface GatewayApiClient {
  getBackofficePartners(): Promise<ApiResponse<BackofficePartnersData>>;
  postBackofficePartner(
    body: BackofficeCreatePartnerBody,
  ): Promise<ApiResponse<BackofficePartnersData>>;
  getBackofficeContents(): Promise<ApiResponse<BackofficeContentsData>>;
  patchBackofficeContentStatus(
    body: BackofficeUpdateContentStatusBody,
  ): Promise<ApiResponse<BackofficeContentsData>>;
  getBackofficeReviews(): Promise<ApiResponse<BackofficeReviewsData>>;
  patchBackofficeReview(
    body: BackofficeUpdateReviewBody,
  ): Promise<ApiResponse<BackofficeReviewsData>>;
}

const jsonHeaders = {
  "Content-Type": "application/json",
};

export class HttpGatewayApiClient implements GatewayApiClient {
  constructor(private readonly baseUrl = "") {}

  async getBackofficePartners(): Promise<ApiResponse<BackofficePartnersData>> {
    return this.request<ApiResponse<BackofficePartnersData>>("/api/v2/backoffice/affiliate/partners", {
      method: "GET",
      headers: jsonHeaders,
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
    return this.request<ApiResponse<BackofficeContentsData>>("/api/v2/backoffice/content/items", {
      method: "GET",
      headers: jsonHeaders,
    });
  }

  async patchBackofficeContentStatus(
    body: BackofficeUpdateContentStatusBody,
  ): Promise<ApiResponse<BackofficeContentsData>> {
    return this.request<ApiResponse<BackofficeContentsData>>("/api/v2/backoffice/content/items/status", {
      method: "PATCH",
      headers: jsonHeaders,
      body: JSON.stringify(body),
    });
  }

  async getBackofficeReviews(): Promise<ApiResponse<BackofficeReviewsData>> {
    return this.request<ApiResponse<BackofficeReviewsData>>("/api/v2/backoffice/governance/reviews", {
      method: "GET",
      headers: jsonHeaders,
    });
  }

  async patchBackofficeReview(
    body: BackofficeUpdateReviewBody,
  ): Promise<ApiResponse<BackofficeReviewsData>> {
    return this.request<ApiResponse<BackofficeReviewsData>>("/api/v2/backoffice/governance/reviews/status", {
      method: "PATCH",
      headers: jsonHeaders,
      body: JSON.stringify(body),
    });
  }

  private async request<T>(path: string, init: RequestInit): Promise<T> {
    const response = await fetch(`${this.baseUrl}${path}`, init);
    return (await response.json()) as T;
  }
}
