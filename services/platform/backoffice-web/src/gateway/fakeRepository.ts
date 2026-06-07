import type { GatewayApiClient } from "./client";
import type {
  ApiResponse,
  BackofficeContentItem,
  BackofficeContentsData,
  BackofficeContentDetailData,
  BackofficeCreateContentBody,
  BackofficeUpdateContentBody,
  BackofficeSubmitReviewBody,
  BackofficePublishContentBody,
  BackofficeRollbackContentBody,
  BackofficeCreatePartnerBody,
  BackofficePartner,
  BackofficePartnersData,
  BackofficeReviewsData,
  BackofficeUpdateContentStatusBody,
  BackofficeUpdateReviewBody,
  BackofficeSetVisibilityBody,
  BackofficeSetVisibilityData,
  BackofficeLoginData,
  VisibilityState,
} from "./types";

const delay = (ms = 280): Promise<void> =>
  new Promise((resolve) => setTimeout(resolve, ms));

const makeMeta = () => ({
  request_id: `req_${Date.now()}`,
  server_time_ms: Date.now(),
});

let contentSeq = 100;
const now = () => new Date().toISOString();

export class FakeGatewayRepository implements GatewayApiClient {
  async loginBackoffice(accessToken: string): Promise<ApiResponse<BackofficeLoginData>> {
    await delay();
    if (accessToken !== "test-token") {
      return { success: false, code: 20004, message: "Invalid credentials" };
    }
    return {
      success: true,
      code: 0,
      message: "OK",
      data: { token: "test-token", role: "backoffice_admin" },
      meta: makeMeta(),
    };
  }

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
      content_id: "guide_card_1001",
      resource_kind: "guide_card",
      title: "通勤衣橱升级专题",
      subtitle: "高性价比通勤穿搭",
      theme_ids: ["clothing"],
      tag_ids: ["commute"],
      content_status: "published",
      revision: 1,
      published_revision: 1,
      summary: "精选通勤穿搭推荐",
      cover_media: { url: "https://example.com/cover1.jpg", type: "image" },
      landing_url: "https://example.com/commute",
      external_item_id: "tmall_item_001",
      affiliate_refs: [{ channel: "PDD", external_item_id: "pdd_001" }],
      selling_points: ["百搭", "高性价比"],
      commercial_disclosure_required: true,
      created_at: now(),
      updated_at: now(),
      created_by: "ops_admin",
    },
    {
      content_id: "ranking_list_1002",
      resource_kind: "ranking_list",
      title: "高性价比早餐榜",
      subtitle: "",
      theme_ids: ["food"],
      tag_ids: [],
      content_status: "draft",
      revision: 1,
      published_revision: 0,
      summary: "早餐性价比排行",
      created_at: now(),
      updated_at: now(),
      created_by: "ops_admin",
    },
  ];

  private visibilityByContentId = new Map<string, VisibilityState>([
    ["guide_card_1001", "published"],
  ]);

  private reviews: BackofficeReviewsData["items"] = [
    {
      review_id: "rev_9001",
      subject_id: "guide_card_1001",
      resource_kind: "guide_card",
      status: "pending",
      priority: 0,
      enqueue_reason: "首次提交审核",
      enqueued_at: now(),
    },
    {
      review_id: "rev_9002",
      subject_id: "ranking_list_1002",
      resource_kind: "ranking_list",
      status: "approved",
      priority: 0,
      enqueue_reason: "运营主动提交",
      enqueued_at: now(),
      reviewer_id: "reviewer_01",
      comment: "内容合规",
      decided_at: now(),
    },
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

  async getBackofficeContentDetail(
    contentId: string,
  ): Promise<ApiResponse<BackofficeContentDetailData>> {
    await delay(180);
    const content = this.contents.find((c) => c.content_id === contentId);
    if (!content) {
      return {
        success: false,
        code: 404,
        message: `content ${contentId} not found`,
        meta: makeMeta(),
      };
    }
    const revisions = Array.from({ length: content.revision }, (_, i) => ({
      revision: i + 1,
      created_at: now(),
      created_by: content.created_by ?? "ops_admin",
      change_summary: i === 0 ? "初始创建" : `修订版本 ${i + 1}`,
    }));
    const reviewState = this.reviews.find(
      (r) => r.subject_id === contentId,
    );
    return {
      success: true,
      code: 0,
      message: "ok",
      data: {
        content,
        revision_history: revisions,
        review_state: reviewState
          ? {
              review_id: reviewState.review_id,
              status: reviewState.status,
              submitted_at: reviewState.enqueued_at,
              reviewer_id: reviewState.reviewer_id,
              comment: reviewState.comment,
            }
          : undefined,
        visibility_verdict: this.visibilityByContentId.has(contentId)
          ? {
              state: this.visibilityByContentId.get(contentId)!,
              source: "manual_ops" as const,
              reason_code: "manual_ops",
              effective_from: now(),
              version: 1,
            }
          : content.content_status === "published"
            ? {
                state: "published" as const,
                source: "review" as const,
                effective_from: now(),
                version: 1,
              }
            : undefined,
      },
      meta: makeMeta(),
    };
  }

  async postBackofficeContentItem(
    body: BackofficeCreateContentBody,
  ): Promise<ApiResponse<BackofficeContentsData>> {
    await delay(220);
    contentSeq += 1;
    const content: BackofficeContentItem = {
      content_id: `${body.resource_kind}_${contentSeq}`,
      resource_kind: body.resource_kind,
      title: body.title,
      subtitle: body.subtitle,
      theme_ids: body.theme_ids ?? [],
      tag_ids: body.tag_ids,
      content_status: body.initial_status,
      revision: 1,
      published_revision: body.initial_status === "published" ? 1 : 0,
      summary: body.summary,
      cover_media: body.cover_media,
      landing_url: body.landing_url,
      external_item_id: body.external_item_id,
      affiliate_refs: body.affiliate_refs,
      selling_points: body.selling_points,
      commercial_disclosure_required: body.commercial_disclosure_required,
      effective_from: body.effective_from,
      effective_to: body.effective_to,
      created_at: now(),
      updated_at: now(),
      created_by: "ops_admin",
    };
    this.contents = [content, ...this.contents];
    return {
      success: true,
      code: 0,
      message: "ok",
      data: { items: this.contents },
      meta: makeMeta(),
    };
  }

  async updateBackofficeContent(
    body: BackofficeUpdateContentBody,
  ): Promise<ApiResponse<BackofficeContentsData>> {
    await delay(220);
    this.contents = this.contents.map((item) => {
      if (item.content_id !== body.content_id) return item;
      return {
        ...item,
        title: body.title ?? item.title,
        subtitle: body.subtitle ?? item.subtitle,
        summary: body.summary ?? item.summary,
        theme_ids: body.theme_ids ?? item.theme_ids,
        tag_ids: body.tag_ids ?? item.tag_ids,
        cover_media: body.cover_media ?? item.cover_media,
        landing_url: body.landing_url ?? item.landing_url,
        external_item_id: body.external_item_id ?? item.external_item_id,
        affiliate_refs: body.affiliate_refs ?? item.affiliate_refs,
        selling_points: body.selling_points ?? item.selling_points,
        commercial_disclosure_required:
          body.commercial_disclosure_required ??
          item.commercial_disclosure_required,
        effective_from: body.effective_from ?? item.effective_from,
        effective_to: body.effective_to ?? item.effective_to,
        revision: item.revision + 1,
        updated_at: now(),
        updated_by: "ops_admin",
      };
    });
    return {
      success: true,
      code: 0,
      message: "ok",
      data: { items: this.contents },
      meta: makeMeta(),
    };
  }

  async submitBackofficeContentReview(
    body: BackofficeSubmitReviewBody,
  ): Promise<ApiResponse<BackofficeContentsData>> {
    await delay(220);
    this.contents = this.contents.map((item) => {
      if (item.content_id !== body.content_id) return item;
      return { ...item, content_status: "in_review" as const, updated_at: now() };
    });
    const review = {
      review_id: `rev_${Date.now()}`,
      subject_id: body.content_id,
      resource_kind: "guide_card" as const,
      status: "pending" as const,
      priority: 0,
      enqueue_reason: body.change_summary ?? "提交审核",
      enqueued_at: now(),
    };
    this.reviews = [review, ...this.reviews];
    return {
      success: true,
      code: 0,
      message: "ok",
      data: { items: this.contents },
      meta: makeMeta(),
    };
  }

  async publishBackofficeContent(
    body: BackofficePublishContentBody,
  ): Promise<ApiResponse<BackofficeContentsData>> {
    await delay(220);
    this.contents = this.contents.map((item) => {
      if (item.content_id !== body.content_id) return item;
      return {
        ...item,
        content_status: "published" as const,
        published_revision: body.revision,
        updated_at: now(),
      };
    });
    return {
      success: true,
      code: 0,
      message: "ok",
      data: { items: this.contents },
      meta: makeMeta(),
    };
  }

  async rollbackBackofficeContent(
    body: BackofficeRollbackContentBody,
  ): Promise<ApiResponse<BackofficeContentsData>> {
    await delay(220);
    this.contents = this.contents.map((item) => {
      if (item.content_id !== body.content_id) return item;
      return {
        ...item,
        revision: body.target_revision,
        updated_at: now(),
        updated_by: "ops_admin",
      };
    });
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
      item.content_id === body.content_id
        ? { ...item, content_status: body.status, updated_at: now() }
        : item,
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
      item.review_id === body.review_id
        ? { ...item, status: body.status, decided_at: now() }
        : item,
    );
    this.contents = this.contents.map((item) => {
      const review = this.reviews.find((r) => r.subject_id === item.content_id);
      if (!review || review.review_id !== body.review_id) return item;
      if (body.status === "rejected") {
        return { ...item, content_status: "draft" as const };
      }
      return item;
    });
    return {
      success: true,
      code: 0,
      message: "ok",
      data: { items: this.reviews },
      meta: makeMeta(),
    };
  }

  async setBackofficeGovernanceVisibility(
    body: BackofficeSetVisibilityBody,
  ): Promise<ApiResponse<BackofficeSetVisibilityData>> {
    await delay(200);
    if (!this.contents.some((item) => item.content_id === body.content_id)) {
      return {
        success: false,
        code: 10003,
        message: "content not found",
        meta: makeMeta(),
      };
    }
    this.visibilityByContentId.set(body.content_id, body.state);
    return {
      success: true,
      code: 0,
      message: "ok",
      data: {
        content_id: body.content_id,
        visibility_state: body.state,
      },
      meta: makeMeta(),
    };
  }
}
