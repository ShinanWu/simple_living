import type { GatewayApiClient } from "./client";
import type {
  ApiResponse,
  BackofficeContentItem,
  BackofficeContentsData,
  BackofficeCreatePartnerBody,
  BackofficePartner,
  BackofficePartnersData,
  BackofficeReviewsData,
  BackofficeUpdateContentStatusBody,
  BackofficeUpdateReviewBody,
} from "./types";

const delay = (ms = 280): Promise<void> =>
  new Promise((resolve) => setTimeout(resolve, ms));

const makeMeta = () => ({
  request_id: `req_${Date.now()}`,
  server_time_ms: Date.now(),
});

export class FakeGatewayRepository implements GatewayApiClient {
  private partners: BackofficePartner[] = [
    {
      partner_id: "pdd",
      display_name: "拼多多联盟",
      status: "active",
      primary_channel_code: "pdd",
    },
    {
      partner_id: "douyin",
      display_name: "抖音电商",
      status: "draft",
      primary_channel_code: "douyin",
    },
  ];

  private contents: BackofficeContentItem[] = [
    {
      content_id: "topic_1001",
      title: "通勤衣橱升级专题",
      theme: "clothing",
      status: "published",
    },
    {
      content_id: "rank_1002",
      title: "高性价比早餐榜",
      theme: "food",
      status: "draft",
    },
  ];

  private reviews: BackofficeReviewsData["items"] = [
    { review_id: "rev_9001", subject_id: "guide_card_1001", status: "pending" },
    { review_id: "rev_9002", subject_id: "topic_1001", status: "approved" },
  ];

  async getBackofficePartners(): Promise<ApiResponse<BackofficePartnersData>> {
    await delay(180);
    return {
      success: true,
      code: 0,
      message: "ok",
      data: { items: this.partners },
      meta: makeMeta(),
    };
  }

  async postBackofficePartner(
    body: BackofficeCreatePartnerBody,
  ): Promise<ApiResponse<BackofficePartnersData>> {
    await delay(220);
    this.partners = [{ ...body }, ...this.partners];
    return {
      success: true,
      code: 0,
      message: "ok",
      data: { items: this.partners },
      meta: makeMeta(),
    };
  }

  async getBackofficeContents(): Promise<ApiResponse<BackofficeContentsData>> {
    await delay(180);
    return {
      success: true,
      code: 0,
      message: "ok",
      data: { items: this.contents },
      meta: makeMeta(),
    };
  }

  async patchBackofficeContentStatus(
    body: BackofficeUpdateContentStatusBody,
  ): Promise<ApiResponse<BackofficeContentsData>> {
    await delay(220);
    this.contents = this.contents.map((item) =>
      item.content_id === body.content_id ? { ...item, status: body.status } : item,
    );
    return {
      success: true,
      code: 0,
      message: "ok",
      data: { items: this.contents },
      meta: makeMeta(),
    };
  }

  async getBackofficeReviews(): Promise<ApiResponse<BackofficeReviewsData>> {
    await delay(180);
    return {
      success: true,
      code: 0,
      message: "ok",
      data: { items: this.reviews },
      meta: makeMeta(),
    };
  }

  async patchBackofficeReview(
    body: BackofficeUpdateReviewBody,
  ): Promise<ApiResponse<BackofficeReviewsData>> {
    await delay(220);
    this.reviews = this.reviews.map((item) =>
      item.review_id === body.review_id ? { ...item, status: body.status } : item,
    );
    return {
      success: true,
      code: 0,
      message: "ok",
      data: { items: this.reviews },
      meta: makeMeta(),
    };
  }
}
