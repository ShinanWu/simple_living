import { useEffect, useState } from "react";
import type { GatewayApiClient } from "../gateway/client";
import type {
  ApiResponse,
  BackofficeContentItem,
  BackofficePartner,
  BackofficeReviewItem,
  BackofficeReviewStatus,
} from "../gateway/types";

type BackofficeTab = "affiliate" | "content" | "governance";
type LoadState = "loading" | "success" | "empty" | "error";

interface OperationLogItem {
  id: string;
  action: string;
  target: string;
  result: "success" | "error";
  requestId: string;
  message: string;
  at: string;
}

interface OperationsConsolePageProps {
  api: GatewayApiClient;
}

export function OperationsConsolePage({ api }: OperationsConsolePageProps) {
  const [tab, setTab] = useState<BackofficeTab>("affiliate");
  const [partners, setPartners] = useState<BackofficePartner[]>([]);
  const [contents, setContents] = useState<BackofficeContentItem[]>([]);
  const [reviews, setReviews] = useState<BackofficeReviewItem[]>([]);
  const [partnerName, setPartnerName] = useState("");
  const [partnerStatus, setPartnerStatus] = useState<BackofficePartner["status"]>("draft");
  const [partnerChannelCode, setPartnerChannelCode] = useState("");
  const [filterTheme, setFilterTheme] = useState("all");
  const [filterContentStatus, setFilterContentStatus] = useState("all");
  const [filterReviewStatus, setFilterReviewStatus] = useState("all");
  const [partnerState, setPartnerState] = useState<LoadState>("loading");
  const [contentState, setContentState] = useState<LoadState>("loading");
  const [reviewState, setReviewState] = useState<LoadState>("loading");
  const [banner, setBanner] = useState<{ type: "success" | "error"; text: string } | null>(null);
  const [logs, setLogs] = useState<OperationLogItem[]>([]);

  const appendLog = (
    action: string,
    target: string,
    resp: ApiResponse<unknown>,
    fallbackReqId: string,
  ) => {
    setLogs((prev) => [
      {
        id: `${Date.now()}_${Math.random().toString(36).slice(2, 8)}`,
        action,
        target,
        result: resp.success ? "success" : "error",
        requestId: resp.meta?.request_id ?? fallbackReqId,
        message: resp.message,
        at: new Date().toLocaleString(),
      },
      ...prev,
    ]);
  };

  const loadPartners = async () => {
    setPartnerState("loading");
    const resp = await api.getBackofficePartners();
    const items = resp.data?.items ?? [];
    if (!resp.success) {
      setPartnerState("error");
      setBanner({ type: "error", text: `伙伴列表加载失败：${resp.message}` });
      return;
    }
    setPartners(items);
    setPartnerState(items.length === 0 ? "empty" : "success");
  };

  const loadContents = async () => {
    setContentState("loading");
    const resp = await api.getBackofficeContents();
    const items = resp.data?.items ?? [];
    if (!resp.success) {
      setContentState("error");
      setBanner({ type: "error", text: `内容列表加载失败：${resp.message}` });
      return;
    }
    setContents(items);
    setContentState(items.length === 0 ? "empty" : "success");
  };

  const loadReviews = async () => {
    setReviewState("loading");
    const resp = await api.getBackofficeReviews();
    const items = resp.data?.items ?? [];
    if (!resp.success) {
      setReviewState("error");
      setBanner({ type: "error", text: `审核队列加载失败：${resp.message}` });
      return;
    }
    setReviews(items);
    setReviewState(items.length === 0 ? "empty" : "success");
  };

  useEffect(() => {
    void Promise.all([loadPartners(), loadContents(), loadReviews()]);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [api]);

  const addPartner = async () => {
    const normalized = partnerName.trim();
    if (!normalized) return;
    const partnerId = normalized.toLowerCase().replace(/\s+/g, "_");
    if (!/^[a-z0-9_]+$/.test(partnerId)) {
      setBanner({ type: "error", text: "伙伴 ID 仅允许小写字母、数字、下划线。" });
      return;
    }
    const resp = await api.postBackofficePartner({
      partner_id: partnerId,
      display_name: normalized,
      status: partnerStatus,
      primary_channel_code: partnerChannelCode.trim() || partnerId,
    });
    appendLog("partner.create", partnerId, resp, "partner_create");
    if (!resp.success) {
      setBanner({ type: "error", text: `新增伙伴失败：${resp.message}` });
      return;
    }
    setPartners(resp.data?.items ?? []);
    setPartnerState((resp.data?.items?.length ?? 0) > 0 ? "success" : "empty");
    setBanner({ type: "success", text: `伙伴 ${normalized} 新增成功` });
    setPartnerName("");
    setPartnerChannelCode("");
    setPartnerStatus("draft");
  };

  const switchContentStatus = async (item: BackofficeContentItem) => {
    const nextStatus = item.status === "published" ? "draft" : "published";
    const resp = await api.patchBackofficeContentStatus({
      content_id: item.content_id,
      status: nextStatus,
    });
    appendLog("content.status.update", item.content_id, resp, "content_update");
    if (!resp.success) {
      setBanner({ type: "error", text: `内容状态更新失败：${resp.message}` });
      return;
    }
    setContents(resp.data?.items ?? []);
    setContentState((resp.data?.items?.length ?? 0) > 0 ? "success" : "empty");
    setBanner({ type: "success", text: `内容 ${item.content_id} 已更新为 ${nextStatus}` });
  };

  const updateReview = async (
    reviewId: string,
    status: Exclude<BackofficeReviewStatus, "pending">,
  ) => {
    const resp = await api.patchBackofficeReview({ review_id: reviewId, status });
    appendLog("review.status.update", reviewId, resp, "review_update");
    if (!resp.success) {
      setBanner({ type: "error", text: `审核状态更新失败：${resp.message}` });
      return;
    }
    setReviews(resp.data?.items ?? []);
    setReviewState((resp.data?.items?.length ?? 0) > 0 ? "success" : "empty");
    setBanner({ type: "success", text: `审核单 ${reviewId} 已更新为 ${status}` });
  };

  const filteredContents = contents.filter((item) => {
    if (filterTheme !== "all" && item.theme !== filterTheme) return false;
    if (filterContentStatus !== "all" && item.status !== filterContentStatus) return false;
    return true;
  });

  const filteredReviews = reviews.filter((item) => {
    if (filterReviewStatus !== "all" && item.status !== filterReviewStatus) return false;
    return true;
  });

  return (
    <section style={{ display: "grid", gap: 16, gridTemplateColumns: "2fr 1fr" }}>
      <div>
      <h2>运营管理平台（最小版）</h2>
      <p style={{ color: "#555" }}>覆盖 affiliate / content / governance 三域运营动作</p>
      {banner && (
        <p
          style={{
            border: "1px solid",
            borderColor: banner.type === "success" ? "#2e7d32" : "#d32f2f",
            background: banner.type === "success" ? "#e8f5e9" : "#ffebee",
            color: banner.type === "success" ? "#1b5e20" : "#b71c1c",
            padding: "8px 10px",
            borderRadius: 6,
          }}
        >
          {banner.text}
        </p>
      )}
      <div style={{ display: "flex", gap: 8, marginBottom: 12 }}>
        <button disabled={tab === "affiliate"} onClick={() => setTab("affiliate")}>
          联盟管理
        </button>
        <button disabled={tab === "content"} onClick={() => setTab("content")}>
          内容管理
        </button>
        <button disabled={tab === "governance"} onClick={() => setTab("governance")}>
          治理审核
        </button>
      </div>

      {tab === "affiliate" && (
        <div>
          <h3>伙伴列表</h3>
          <div style={{ display: "flex", gap: 8, marginBottom: 8 }}>
            <input
              aria-label="新增伙伴"
              placeholder="输入伙伴名称"
              value={partnerName}
              onChange={(event) => setPartnerName(event.target.value)}
            />
            <select
              aria-label="伙伴状态"
              value={partnerStatus}
              onChange={(event) =>
                setPartnerStatus(event.target.value as BackofficePartner["status"])
              }
            >
              <option value="draft">draft</option>
              <option value="active">active</option>
              <option value="disabled">disabled</option>
            </select>
            <input
              aria-label="渠道编码"
              placeholder="渠道编码（可选）"
              value={partnerChannelCode}
              onChange={(event) => setPartnerChannelCode(event.target.value)}
            />
            <button onClick={() => void addPartner()}>新增</button>
            <button onClick={() => void loadPartners()}>刷新</button>
          </div>
          {partnerState === "loading" && <p>伙伴列表加载中...</p>}
          {partnerState === "error" && <p>伙伴列表加载失败，请重试。</p>}
          {partnerState === "empty" && <p>暂无伙伴。</p>}
          <ul>
            {partners.map((partner) => (
              <li key={partner.partner_id}>
                {partner.display_name}（{partner.status}/{partner.primary_channel_code}）
              </li>
            ))}
          </ul>
        </div>
      )}

      {tab === "content" && (
        <div>
          <h3>内容发布列表</h3>
          <div style={{ display: "flex", gap: 8, marginBottom: 8 }}>
            <select
              aria-label="内容主题过滤"
              value={filterTheme}
              onChange={(event) => setFilterTheme(event.target.value)}
            >
              <option value="all">全部主题</option>
              <option value="clothing">clothing</option>
              <option value="food">food</option>
              <option value="housing">housing</option>
              <option value="transport">transport</option>
            </select>
            <select
              aria-label="内容状态过滤"
              value={filterContentStatus}
              onChange={(event) => setFilterContentStatus(event.target.value)}
            >
              <option value="all">全部状态</option>
              <option value="draft">draft</option>
              <option value="published">published</option>
            </select>
            <button onClick={() => void loadContents()}>刷新</button>
          </div>
          {contentState === "loading" && <p>内容列表加载中...</p>}
          {contentState === "error" && <p>内容列表加载失败，请重试。</p>}
          {contentState === "empty" && <p>暂无内容。</p>}
          <ul>
            {filteredContents.map((item) => (
              <li key={item.content_id} style={{ marginBottom: 8 }}>
                <span>
                  {item.title}（{item.theme}/{item.status}）
                </span>
                <button
                  style={{ marginLeft: 8 }}
                  onClick={() => void switchContentStatus(item)}
                >
                  {item.status === "published" ? "下架" : "发布"}
                </button>
              </li>
            ))}
          </ul>
        </div>
      )}

      {tab === "governance" && (
        <div>
          <h3>审核队列</h3>
          <div style={{ display: "flex", gap: 8, marginBottom: 8 }}>
            <select
              aria-label="审核状态过滤"
              value={filterReviewStatus}
              onChange={(event) => setFilterReviewStatus(event.target.value)}
            >
              <option value="all">全部状态</option>
              <option value="pending">pending</option>
              <option value="approved">approved</option>
              <option value="rejected">rejected</option>
            </select>
            <button onClick={() => void loadReviews()}>刷新</button>
          </div>
          {reviewState === "loading" && <p>审核队列加载中...</p>}
          {reviewState === "error" && <p>审核队列加载失败，请重试。</p>}
          {reviewState === "empty" && <p>暂无审核单。</p>}
          <ul>
            {filteredReviews.map((item) => (
              <li key={item.review_id} style={{ marginBottom: 8 }}>
                {item.review_id} / {item.subject_id} / {item.status}
                {item.status === "pending" && (
                  <>
                    <button
                      style={{ marginLeft: 8 }}
                      onClick={() => void updateReview(item.review_id, "approved")}
                    >
                      通过
                    </button>
                    <button
                      style={{ marginLeft: 6 }}
                      onClick={() => void updateReview(item.review_id, "rejected")}
                    >
                      拒绝
                    </button>
                  </>
                )}
              </li>
            ))}
          </ul>
        </div>
      )}
      </div>
      <aside style={{ border: "1px solid #eee", borderRadius: 8, padding: 12 }}>
        <h3 style={{ marginTop: 0 }}>最近操作日志</h3>
        {logs.length === 0 ? (
          <p style={{ color: "#666" }}>暂无操作记录</p>
        ) : (
          <ul style={{ listStyle: "none", padding: 0, margin: 0, display: "grid", gap: 10 }}>
            {logs.slice(0, 12).map((log) => (
              <li key={log.id} style={{ borderBottom: "1px dashed #ddd", paddingBottom: 8 }}>
                <div>
                  <strong>{log.action}</strong> / {log.target}
                </div>
                <div>
                  结果：{log.result} / request_id: {log.requestId}
                </div>
                <div>{log.message}</div>
                <div style={{ color: "#777", fontSize: 12 }}>{log.at}</div>
              </li>
            ))}
          </ul>
        )}
      </aside>
    </section>
  );
}
