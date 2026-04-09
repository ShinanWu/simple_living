import type { GatewayApiClient } from "./client";
import type {
  ApiResponse,
  GuideDetailData,
  GuideDetailQuery,
  HomeFeedData,
  HomeFeedItem,
  HomeFeedQuery,
  MeSummaryData,
  RedirectPrepareBody,
  RedirectPrepareData,
} from "./types";

const delay = (ms = 280): Promise<void> =>
  new Promise((resolve) => setTimeout(resolve, ms));

const feedByTheme: Record<HomeFeedQuery["theme"], HomeFeedItem[]> = {
  clothing: [
    {
      recommendation_id: "rec_clothing_01",
      scene: "home_feed",
      rank: 1,
      guide_card_id: "guide_card_1001",
      reason_tags: ["适合通勤", "近期热门"],
    },
    {
      recommendation_id: "rec_clothing_01",
      scene: "home_feed",
      rank: 2,
      guide_card_id: "guide_card_1018",
      reason_tags: ["性价比高"],
    },
  ],
  food: [],
  housing: [],
  transport: [],
};

const makeMeta = () => ({
  request_id: `req_${Date.now()}`,
  server_time_ms: Date.now(),
});

export class FakeGatewayRepository implements GatewayApiClient {
  async getHomeFeed(query: HomeFeedQuery): Promise<ApiResponse<HomeFeedData>> {
    await delay();
    if (query.theme === "transport") {
      return { success: false, code: 90003, message: "offline", meta: makeMeta() };
    }
    if (query.theme === "housing") {
      return { success: false, code: 90002, message: "upstream timeout", meta: makeMeta() };
    }
    const items = feedByTheme[query.theme];
    return {
      success: true,
      code: 0,
      message: "ok",
      data: {
        items,
        pagination: { next_cursor: "", has_more: false, limit: query.limit ?? 20 },
      },
      meta: makeMeta(),
    };
  }

  async getGuideDetail(
    query: GuideDetailQuery,
  ): Promise<ApiResponse<GuideDetailData>> {
    await delay();
    if (query.guide_card_id === "guide_card_missing") {
      return { success: false, code: 30001, message: "not found", meta: makeMeta() };
    }
    return {
      success: true,
      code: 0,
      message: "ok",
      data: {
        guide: {
          guide_card_id: query.guide_card_id,
          schema_version: 1,
          title: "通勤穿搭清单",
          theme: "clothing",
          published_at: "2026-03-28T08:00:00Z",
          updated_at: "2026-03-28T09:00:00Z",
          commercial_disclosure: { is_commercial: false },
        },
        disclosures: { disclosure_text: "部分内容包含商业合作" },
      },
      meta: makeMeta(),
    };
  }

  async postRedirectPrepare(
    body: RedirectPrepareBody,
  ): Promise<ApiResponse<RedirectPrepareData>> {
    await delay(240);
    if (body.guide_card_id === "guide_card_rate_limit") {
      return { success: false, code: 10005, message: "rate limited", meta: makeMeta() };
    }
    return {
      success: true,
      code: 0,
      message: "ok",
      data: {
        landing_url: "https://track.example.com/r/abc",
        click_id: "click_01",
      },
      meta: makeMeta(),
    };
  }

  async getMeSummary(): Promise<ApiResponse<MeSummaryData>> {
    await delay();
    return {
      success: true,
      code: 0,
      message: "ok",
      data: {
        profile: { user_id: "guest_01", is_guest: true },
        counts: { favorites_count: 2, history_count: 8 },
        consent: {
          personalization_allowed: true,
          consent_version: "2026-03-v1",
          updated_at: "2026-03-28T12:00:00Z",
        },
      },
      meta: makeMeta(),
    };
  }
}
