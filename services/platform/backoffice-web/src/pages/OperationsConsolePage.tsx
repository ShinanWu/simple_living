import { useEffect, useRef, useState } from "react";
import type { GatewayApiClient } from "../gateway/client";
import type {
  ApiResponse,
  BackofficeContentItem,
  BackofficeContentStatus,
  BackofficePartner,
  BackofficeReviewItem,
  BackofficeReviewStatus,
  ContentRevision,
  ResourceKind,
  ReviewState,
  VisibilityVerdict,
  VisibilityState,
} from "../gateway/types";
import "./OperationsConsolePage.css";

type BackofficeTab = "affiliate" | "content" | "governance";
type LoadState = "loading" | "success" | "empty" | "error";
type ContentView = "list" | "create" | "edit" | "revision";

const THEME_OPTIONS = ["clothing", "food", "housing", "transport"] as const;
const RESOURCE_KIND_OPTIONS: ResourceKind[] = ["guide_card", "editorial_content", "topic", "ranking_list"];

const RESOURCE_KIND_LABELS: Record<ResourceKind, string> = {
  guide_card: "导购卡片",
  editorial_content: "图文攻略",
  topic: "专题",
  ranking_list: "榜单",
};

const STATUS_LABELS: Record<BackofficeContentStatus, string> = {
  draft: "草稿",
  in_review: "审核中",
  published: "已发布",
  scheduled: "定时发布",
  offline: "已下架",
  archived: "已归档",
};

const VISIBILITY_STATE_LABELS: Record<VisibilityVerdict["state"], string> = {
  published: "允许展示",
  restricted: "限制展示",
  unpublished: "不可见",
};

function getActionsForStatus(status: BackofficeContentStatus) {
  switch (status) {
    case "draft":
      return { canEdit: true, canSubmitReview: true, canDelete: true };
    case "in_review":
      return { canViewReview: true };
    case "published":
      return { canOffline: true, canSubmitRevision: true, canRollback: true };
    case "scheduled":
      return { canCancelPublish: true, canEditEffectiveTime: true };
    case "offline":
      return { canPublish: true, canArchive: true };
    case "archived":
      return { canView: true };
  }
}

interface OperationLogItem {
  id: string;
  action: string;
  target: string;
  result: "success" | "error";
  requestId: string;
  message: string;
  at: string;
}

interface ContentFormState {
  resource_kind: ResourceKind;
  title: string;
  subtitle: string;
  summary: string;
  theme_ids: string[];
  cover_url: string;
  landing_url: string;
  external_item_id: string;
  affiliate_refs: { channel: string; external_item_id: string }[];
  selling_points: string[];
  commercial_disclosure_required: boolean;
  initial_status: BackofficeContentStatus;
  effective_from: string;
  effective_to: string;
}

const EMPTY_CONTENT_FORM: ContentFormState = {
  resource_kind: "guide_card",
  title: "",
  subtitle: "",
  summary: "",
  theme_ids: [],
  cover_url: "",
  landing_url: "",
  external_item_id: "",
  affiliate_refs: [],
  selling_points: [""],
  commercial_disclosure_required: false,
  initial_status: "draft",
  effective_from: "",
  effective_to: "",
};

interface ContentDetailState {
  content: BackofficeContentItem;
  revision_history: ContentRevision[];
  review_state?: ReviewState;
  visibility_verdict?: VisibilityVerdict;
}

interface OperationsConsolePageProps {
  api: GatewayApiClient;
  onLogout?: () => void;
}

export function OperationsConsolePage({ api, onLogout }: OperationsConsolePageProps) {
  const [tab, setTab] = useState<BackofficeTab>("content");
  const [partners, setPartners] = useState<BackofficePartner[]>([]);
  const [contents, setContents] = useState<BackofficeContentItem[]>([]);
  const [reviews, setReviews] = useState<BackofficeReviewItem[]>([]);
  const [partnerName, setPartnerName] = useState("");
  const [partnerStatus, setPartnerStatus] = useState<BackofficePartner["status"]>("draft");
  const [partnerChannelCode, setPartnerChannelCode] = useState("");
  const [filterTheme, setFilterTheme] = useState("all");
  const [filterContentStatus, setFilterContentStatus] = useState("all");
  const [filterResourceKind, setFilterResourceKind] = useState("all");
  const [filterReviewStatus, setFilterReviewStatus] = useState("all");
  const [partnerState, setPartnerState] = useState<LoadState>("loading");
  const [contentState, setContentState] = useState<LoadState>("loading");
  const [reviewState, setReviewState] = useState<LoadState>("loading");
  const [banner, setBanner] = useState<{ type: "success" | "error"; text: string } | null>(null);
  const [logs, setLogs] = useState<OperationLogItem[]>([]);
  const [contentView, setContentView] = useState<ContentView>("list");
  const [editingContent, setEditingContent] = useState<BackofficeContentItem | null>(null);
  const [contentForm, setContentForm] = useState<ContentFormState>(EMPTY_CONTENT_FORM);
  const [detailOpen, setDetailOpen] = useState(false);
  const [detailData, setDetailData] = useState<ContentDetailState | null>(null);
  const [detailLoading, setDetailLoading] = useState(false);
  const [rollbackTargetRevision, setRollbackTargetRevision] = useState<number | null>(null);
  const [rollbackSummary, setRollbackSummary] = useState("");
  const [visibilityContentId, setVisibilityContentId] = useState("");
  const [visibilityState, setVisibilityState] = useState<VisibilityState>("published");
  const [visibilityReasonCode, setVisibilityReasonCode] = useState("");
  const [coverPreviewUrl, setCoverPreviewUrl] = useState<string | null>(null);
  const [coverUploadError, setCoverUploadError] = useState<string | null>(null);
  const coverFileRef = useRef<HTMLInputElement>(null);

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

  const clearBanner = () => setBanner(null);

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

  const openContentDetail = async (contentId: string) => {
    setDetailLoading(true);
    setDetailOpen(true);
    setDetailData(null);
    const resp = await api.getBackofficeContentDetail(contentId);
    if (resp.success && resp.data) {
      setDetailData(resp.data);
    } else {
      setBanner({ type: "error", text: `内容详情加载失败：${resp.message}` });
    }
    setDetailLoading(false);
  };

  const closeContentDetail = () => {
    setDetailOpen(false);
    setDetailData(null);
    setRollbackTargetRevision(null);
    setRollbackSummary("");
  };

  const handleRollback = async () => {
    if (!detailData || rollbackTargetRevision === null) return;
    const resp = await api.rollbackBackofficeContent({
      content_id: detailData.content.content_id,
      target_revision: rollbackTargetRevision,
      change_summary: rollbackSummary.trim() || undefined,
    });
    appendLog("content.rollback", detailData.content.content_id, resp, "content_rollback");
    if (!resp.success) {
      setBanner({ type: "error", text: `版本回滚失败：${resp.message}` });
      return;
    }
    setBanner({ type: "success", text: `已回滚到版本 ${rollbackTargetRevision}` });
    closeContentDetail();
    await loadContents();
  };

  const handlePublish = async (contentId: string, revision: number) => {
    const resp = await api.publishBackofficeContent({ content_id: contentId, revision });
    appendLog("content.publish", contentId, resp, "content_publish");
    if (!resp.success) {
      setBanner({ type: "error", text: `发布失败：${resp.message}` });
      return;
    }
    setBanner({ type: "success", text: `内容已发布` });
    await loadContents();
  };

  const handleSubmitReview = async (contentId: string, revision: number) => {
    const resp = await api.submitBackofficeContentReview({
      content_id: contentId,
      revision,
      change_summary: undefined,
    });
    appendLog("content.submit_review", contentId, resp, "content_submit_review");
    if (!resp.success) {
      setBanner({ type: "error", text: `提交审核失败：${resp.message}` });
      return;
    }
    setBanner({ type: "success", text: `已提交审核` });
    await loadContents();
  };

  const handleStatusChange = async (contentId: string, status: Exclude<BackofficeContentStatus, "in_review">) => {
    const resp = await api.patchBackofficeContentStatus({ content_id: contentId, status });
    appendLog("content.status_changed", contentId, resp, "content_status_change");
    if (!resp.success) {
      setBanner({ type: "error", text: `状态更新失败：${resp.message}` });
      return;
    }
    setBanner({ type: "success", text: `状态已更新为 ${STATUS_LABELS[status]}` });
    await loadContents();
  };

  const handleDeleteContent = async (contentId: string) => {
    const resp = await api.patchBackofficeContentStatus({ content_id: contentId, status: "archived" });
    appendLog("content.delete", contentId, resp, "content_delete");
    if (!resp.success) {
      setBanner({ type: "error", text: `删除失败：${resp.message}` });
      return;
    }
    setBanner({ type: "success", text: `内容已删除` });
    await loadContents();
  };

  const handleCoverFileChange = (files: FileList | null) => {
    setCoverUploadError(null);
    if (!files || files.length === 0) return;
    const file = files[0];
    if (!file.type.startsWith("image/")) {
      setCoverUploadError("请上传图片文件");
      return;
    }
    if (file.size > 5 * 1024 * 1024) {
      setCoverUploadError("图片大小不能超过 5MB");
      return;
    }
    const reader = new FileReader();
    reader.onload = (e) => {
      const dataUrl = e.target?.result as string;
      setCoverPreviewUrl(dataUrl);
      updateFormField("cover_url", dataUrl);
    };
    reader.onerror = () => setCoverUploadError("图片读取失败");
    reader.readAsDataURL(file);
  };

  const openCreateView = () => {
    setEditingContent(null);
    setContentForm(EMPTY_CONTENT_FORM);
    setContentView("create");
  };

  const openEditView = (item: BackofficeContentItem, isRevision = false) => {
    setEditingContent(item);
    setContentForm({
      resource_kind: item.resource_kind,
      title: item.title,
      subtitle: item.subtitle ?? "",
      summary: item.summary ?? "",
      theme_ids: item.theme_ids,
      cover_url: item.cover_media?.url ?? "",
      landing_url: item.landing_url ?? "",
      external_item_id: item.external_item_id ?? "",
      affiliate_refs: item.affiliate_refs?.map((r) => ({ channel: r.channel, external_item_id: r.external_item_id })) ?? [],
      selling_points: item.selling_points?.length ? item.selling_points : [""],
      commercial_disclosure_required: item.commercial_disclosure_required ?? false,
      initial_status: item.content_status,
      effective_from: item.effective_from ?? "",
      effective_to: item.effective_to ?? "",
    });
    if (isRevision) {
      setContentView("revision");
    } else {
      setContentView("edit");
    }
  };

  const openRevisionView = (item: BackofficeContentItem) => {
    openEditView(item, true);
  };

  const closeContentForm = () => {
    setContentView("list");
    setEditingContent(null);
    setContentForm(EMPTY_CONTENT_FORM);
    setCoverPreviewUrl(null);
    setCoverUploadError(null);
  };

  const updateFormField = <K extends keyof ContentFormState>(key: K, value: ContentFormState[K]) => {
    setContentForm((prev) => ({ ...prev, [key]: value }));
  };

  const addAffiliateRef = () => {
    setContentForm((prev) => ({
      ...prev,
      affiliate_refs: [...prev.affiliate_refs, { channel: "PDD", external_item_id: "" }],
    }));
  };

  const updateAffiliateRef = (index: number, field: "channel" | "external_item_id", value: string) => {
    setContentForm((prev) => ({
      ...prev,
      affiliate_refs: prev.affiliate_refs.map((ref, i) =>
        i === index ? { ...ref, [field]: value } : ref,
      ),
    }));
  };

  const removeAffiliateRef = (index: number) => {
    setContentForm((prev) => ({
      ...prev,
      affiliate_refs: prev.affiliate_refs.filter((_, i) => i !== index),
    }));
  };

  const addSellingPoint = () => {
    setContentForm((prev) => ({ ...prev, selling_points: [...prev.selling_points, ""] }));
  };

  const updateSellingPoint = (index: number, value: string) => {
    setContentForm((prev) => ({
      ...prev,
      selling_points: prev.selling_points.map((sp, i) => (i === index ? value : sp)),
    }));
  };

  const removeSellingPoint = (index: number) => {
    setContentForm((prev) => ({
      ...prev,
      selling_points: prev.selling_points.filter((_, i) => i !== index),
    }));
  };

  const handleToggleTheme = (theme: string) => {
    setContentForm((prev) => {
      const exists = prev.theme_ids.includes(theme);
      return {
        ...prev,
        theme_ids: exists ? prev.theme_ids.filter((t) => t !== theme) : [...prev.theme_ids, theme],
      };
    });
  };

  const validateContentForm = (): string | null => {
    const title = contentForm.title.trim();
    if (!title) return "标题必填。";
    if (contentForm.resource_kind === "guide_card" && !contentForm.landing_url.trim()) {
      return "导购卡片必须填写落地页链接。";
    }
    if (contentForm.landing_url.trim() && !/^https?:\/\/.+/i.test(contentForm.landing_url.trim())) {
      return "链接格式不正确，需以 http:// 或 https:// 开头。";
    }
    return null;
  };

  const handleSaveContent = async (publishNow: boolean) => {
    const validationError = validateContentForm();
    if (validationError) {
      setBanner({ type: "error", text: validationError });
      return;
    }

    const body = {
      resource_kind: contentForm.resource_kind,
      title: contentForm.title.trim(),
      subtitle: contentForm.subtitle.trim() || undefined,
      summary: contentForm.summary.trim() || undefined,
      theme_ids: contentForm.theme_ids.length > 0 ? contentForm.theme_ids : undefined,
      cover_media: contentForm.cover_url.trim()
        ? { url: contentForm.cover_url.trim(), type: "image" as const }
        : undefined,
      landing_url: contentForm.landing_url.trim() || undefined,
      external_item_id: contentForm.external_item_id.trim() || undefined,
      affiliate_refs: contentForm.affiliate_refs
        .filter((r) => r.external_item_id.trim())
        .map((r) => ({ channel: r.channel, external_item_id: r.external_item_id.trim() })),
      selling_points: contentForm.selling_points.filter((sp) => sp.trim()).length > 0
        ? contentForm.selling_points.filter((sp) => sp.trim())
        : undefined,
      commercial_disclosure_required: contentForm.commercial_disclosure_required || undefined,
      effective_from: contentForm.effective_from || undefined,
      effective_to: contentForm.effective_to || undefined,
    };

    if (editingContent) {
      const resp = await api.updateBackofficeContent({
        content_id: editingContent.content_id,
        revision: editingContent.revision,
        ...body,
      });
      appendLog("content.update", editingContent.content_id, resp, "content_update");
      if (!resp.success) {
        setBanner({ type: "error", text: `更新失败：${resp.message}` });
        return;
      }
      if (publishNow) {
        await handlePublish(editingContent.content_id, editingContent.revision + 1);
      }
      setBanner({ type: "success", text: publishNow ? "内容已更新并发布" : "内容已更新" });
    } else {
      const resp = await api.postBackofficeContentItem({
        ...body,
        initial_status: publishNow ? "published" : "draft",
      });
      appendLog("content.create", contentForm.title.trim(), resp, "content_create");
      if (!resp.success) {
        setBanner({ type: "error", text: `创建失败：${resp.message}` });
        return;
      }
      setBanner({ type: "success", text: publishNow ? "内容已创建并发布" : "内容已创建" });
    }
    closeContentForm();
    await loadContents();
  };

  const handleSaveContentForRevision = async (submitForReview: boolean) => {
    const validationError = validateContentForm();
    if (validationError) {
      setBanner({ type: "error", text: validationError });
      return;
    }

    if (!editingContent) {
      setBanner({ type: "error", text: "修订操作需要指定目标内容。" });
      return;
    }

    const body = {
      resource_kind: contentForm.resource_kind,
      title: contentForm.title.trim(),
      subtitle: contentForm.subtitle.trim() || undefined,
      summary: contentForm.summary.trim() || undefined,
      theme_ids: contentForm.theme_ids.length > 0 ? contentForm.theme_ids : undefined,
      cover_media: contentForm.cover_url.trim()
        ? { url: contentForm.cover_url.trim(), type: "image" as const }
        : undefined,
      landing_url: contentForm.landing_url.trim() || undefined,
      external_item_id: contentForm.external_item_id.trim() || undefined,
      affiliate_refs: contentForm.affiliate_refs
        .filter((r) => r.external_item_id.trim())
        .map((r) => ({ channel: r.channel, external_item_id: r.external_item_id.trim() })),
      selling_points: contentForm.selling_points.filter((sp) => sp.trim()).length > 0
        ? contentForm.selling_points.filter((sp) => sp.trim())
        : undefined,
      commercial_disclosure_required: contentForm.commercial_disclosure_required || undefined,
      effective_from: contentForm.effective_from || undefined,
      effective_to: contentForm.effective_to || undefined,
    };

    const updateResp = await api.updateBackofficeContent({
      content_id: editingContent.content_id,
      revision: editingContent.revision,
      ...body,
    });
    appendLog("content.revision.save", editingContent.content_id, updateResp, "content_revision");
    if (!updateResp.success) {
      setBanner({ type: "error", text: `保存修订失败：${updateResp.message}` });
      return;
    }

    if (submitForReview) {
      const reviewResp = await api.submitBackofficeContentReview({
        content_id: editingContent.content_id,
        revision: editingContent.revision + 1,
      });
      appendLog("content.revision.submit", editingContent.content_id, reviewResp, "content_revision_review");
      if (!reviewResp.success) {
        setBanner({ type: "error", text: `提交审核失败：${reviewResp.message}` });
        return;
      }
      setBanner({ type: "success", text: "修订已保存并提交审核" });
    } else {
      setBanner({ type: "success", text: "修订已保存" });
    }

    closeContentForm();
    await loadContents();
  };

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

  const updateReview = async (
    reviewId: string,
    status: Exclude<BackofficeReviewStatus, "pending" | "in_review" | "completed">,
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
    await loadContents();
  };

  const refreshContentDetail = async (contentId: string) => {
    const resp = await api.getBackofficeContentDetail(contentId);
    if (resp.success && resp.data) {
      setDetailData(resp.data);
    }
  };

  const handleSetVisibility = async (
    contentId: string,
    state: VisibilityState,
    reasonCode?: string,
    options?: { confirmMessage?: string; refreshDetail?: boolean },
  ) => {
    if (options?.confirmMessage && !window.confirm(options.confirmMessage)) {
      return;
    }
    const resp = await api.setBackofficeGovernanceVisibility({
      content_id: contentId,
      state,
      reason_code: reasonCode?.trim() || undefined,
    });
    appendLog("visibility.set", contentId, resp, "visibility_set");
    if (!resp.success) {
      setBanner({ type: "error", text: `可见性裁决失败：${resp.message}` });
      return;
    }
    setBanner({
      type: "success",
      text: `内容 ${contentId} 可见性已设为 ${VISIBILITY_STATE_LABELS[state]}`,
    });
    if (options?.refreshDetail) {
      await refreshContentDetail(contentId);
    }
    await loadContents();
  };

  const submitGovernanceVisibility = async () => {
    const contentId = visibilityContentId.trim();
    if (!contentId) {
      setBanner({ type: "error", text: "请填写内容 ID" });
      return;
    }
    const confirmMessage =
      visibilityState === "published"
        ? undefined
        : `确认将 ${contentId} 设为「${VISIBILITY_STATE_LABELS[visibilityState]}」？该操作会影响 C 端可见性。`;
    await handleSetVisibility(contentId, visibilityState, visibilityReasonCode, { confirmMessage });
    setVisibilityContentId("");
    setVisibilityReasonCode("");
  };

  const filteredContents = contents.filter((item) => {
    if (filterResourceKind !== "all" && item.resource_kind !== filterResourceKind) return false;
    if (filterTheme !== "all" && !item.theme_ids.includes(filterTheme)) return false;
    if (filterContentStatus !== "all" && item.content_status !== filterContentStatus) return false;
    return true;
  });

  const filteredReviews = reviews.filter((item) => {
    if (filterReviewStatus !== "all" && item.status !== filterReviewStatus) return false;
    return true;
  });

  const publishedCount = contents.filter((item) => item.content_status === "published").length;
  const draftCount = contents.filter((item) => item.content_status === "draft").length;
  const inReviewCount = contents.filter((item) => item.content_status === "in_review").length;
  const pendingReviewCount = reviews.filter((item) => item.status === "pending").length;
  const activePartnerCount = partners.filter((item) => item.status === "active").length;

  const isActionable = (status: BackofficeReviewStatus) =>
    status === "pending" || status === "in_review";

  return (
    <main className="ops-shell">
      <section className="ops-hero">
        <div>
          <p className="ops-kicker">ShaoTang Operations</p>
          <h1>运营管理台</h1>
          <p className="ops-subtitle">
            通过真实 gateway 管理内容生命周期、联盟伙伴和审核动作；内容主数据写入 backoffice-backend PostgreSQL。
          </p>
        </div>
        <div className="ops-hero-actions">
          <button className="secondary-button" onClick={() => void Promise.all([loadPartners(), loadContents(), loadReviews()])}>
            刷新全部
          </button>
          {onLogout ? (
            <button
              className="secondary-button"
              type="button"
              onClick={() => {
                void import("../services/auth/storage").then(({ clearBackofficeSession }) => {
                  clearBackofficeSession();
                  onLogout();
                });
              }}
            >
              退出登录
            </button>
          ) : null}
        </div>
      </section>

      {banner && (
        <p className="ops-alert ops-alert-banner" role="status">
          <span className={`ops-alert-icon ${banner.type === "error" ? "ops-alert-icon-error" : "ops-alert-icon-ok"}`}>
            {banner.type === "error" ? "\u2717" : "\u2713"}
          </span>
          {banner.text}
          <button className="ops-alert-dismiss" onClick={clearBanner} aria-label="关闭提示">&times;</button>
        </p>
      )}

      <section className="ops-metrics" aria-label="运营概览">
        <article className="metric-card">
          <span>已发布内容</span>
          <strong>{publishedCount}</strong>
          <small>{draftCount} 个草稿，{inReviewCount} 个审核中</small>
        </article>
        <article className="metric-card">
          <span>审核待办</span>
          <strong>{pendingReviewCount}</strong>
          <small>审核通过后仍需显式发布</small>
        </article>
        <article className="metric-card">
          <span>活跃伙伴</span>
          <strong>{activePartnerCount}</strong>
          <small>{partners.length} 个伙伴记录</small>
        </article>
      </section>

      <section className="ops-workspace">
        <div className="ops-main-card">
          <div className="ops-tabs" role="tablist" aria-label="运营模块">
            <button className={tab === "content" ? "active" : ""} onClick={() => { setTab("content"); setContentView("list"); }}>
              内容管理
            </button>
            <button className={tab === "governance" ? "active" : ""} onClick={() => setTab("governance")}>
              治理审核
            </button>
            <button className={tab === "affiliate" ? "active" : ""} onClick={() => setTab("affiliate")}>
              联盟管理
            </button>
          </div>

          {tab === "affiliate" && (
            <section className="ops-panel">
              <div className="panel-heading">
                <div>
                  <h2>联盟伙伴</h2>
                  <p>维护伙伴基础信息；转链规格由 backoffice-backend 导出供 tracking-server 读取。</p>
                </div>
                <button className="secondary-button" onClick={() => void loadPartners()}>刷新</button>
              </div>
              <div className="inline-form">
                <label>
                  伙伴名称
                  <input value={partnerName} onChange={(event) => setPartnerName(event.target.value)} />
                </label>
                <label>
                  状态
                  <select
                    value={partnerStatus}
                    onChange={(event) => setPartnerStatus(event.target.value as BackofficePartner["status"])}
                  >
                    <option value="draft">draft</option>
                    <option value="active">active</option>
                    <option value="disabled">disabled</option>
                  </select>
                </label>
                <label>
                  渠道编码
                  <input value={partnerChannelCode} onChange={(event) => setPartnerChannelCode(event.target.value)} />
                </label>
                <button className="primary-button" onClick={() => void addPartner()}>新增伙伴</button>
              </div>
              {partnerState !== "success" ? (
                <StateMessage state={partnerState} emptyText="暂无伙伴。" />
              ) : (
                <div className="ops-table-wrap">
                  <table className="ops-table">
                    <thead>
                      <tr><th>伙伴</th><th>状态</th><th>渠道</th></tr>
                    </thead>
                    <tbody>
                      {partners.map((partner) => (
                        <tr key={partner.partner_id}>
                          <td>{partner.display_name}<small>{partner.partner_id}</small></td>
                          <td><StatusBadge status={partner.status} /></td>
                          <td>{partner.primary_channel_code}</td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
              )}
            </section>
          )}

          {tab === "content" && contentView === "list" && (
            <section className="ops-panel">
              <div className="panel-heading">
                <div>
                  <h2>内容管理</h2>
                  <p>管理内容生命周期：创建、编辑、审核提交、发布、下架、归档与版本回滚。</p>
                </div>
                <div style={{ display: "flex", gap: 8 }}>
                  <button className="primary-button" onClick={openCreateView}>新增内容</button>
                  <button className="secondary-button" onClick={() => void loadContents()}>刷新</button>
                </div>
              </div>

              <div className="toolbar">
                <select value={filterResourceKind} onChange={(event) => setFilterResourceKind(event.target.value)} aria-label="资源类型过滤">
                  <option value="all">全部类型</option>
                  {RESOURCE_KIND_OPTIONS.map((k) => (
                    <option key={k} value={k}>{RESOURCE_KIND_LABELS[k]}</option>
                  ))}
                </select>
                <select value={filterTheme} onChange={(event) => setFilterTheme(event.target.value)} aria-label="内容主题过滤">
                  <option value="all">全部主题</option>
                  {THEME_OPTIONS.map((t) => <option key={t} value={t}>{t}</option>)}
                </select>
                <select value={filterContentStatus} onChange={(event) => setFilterContentStatus(event.target.value)} aria-label="内容状态过滤">
                  <option value="all">全部状态</option>
                  <option value="draft">draft</option>
                  <option value="in_review">in_review</option>
                  <option value="published">published</option>
                  <option value="scheduled">scheduled</option>
                  <option value="offline">offline</option>
                  <option value="archived">archived</option>
                </select>
              </div>

              {contentState !== "success" ? (
                <StateMessage state={contentState} emptyText="暂无内容。" />
              ) : (
                <div className="ops-table-wrap">
                  <table className="ops-table">
                    <thead>
                      <tr>
                        <th>内容</th>
                        <th>类型</th>
                        <th>主题</th>
                        <th>状态</th>
                        <th>版本</th>
                        <th>更新时间</th>
                        <th>操作</th>
                      </tr>
                    </thead>
                    <tbody>
                      {filteredContents.map((item) => {
                        const actions = getActionsForStatus(item.content_status);
                        return (
                          <tr key={item.content_id}>
                            <td className="cell-primary">
                              {item.title}
                              <small>{item.subtitle || item.summary || item.content_id}</small>
                            </td>
                            <td>{RESOURCE_KIND_LABELS[item.resource_kind]}</td>
                            <td>
                              {item.theme_ids.length > 0
                                ? item.theme_ids.join(", ")
                                : "-"}
                            </td>
                            <td><StatusBadge status={item.content_status} /></td>
                            <td>
                              <small>rev:{item.revision}</small>
                              {item.published_revision > 0 && (
                                <small style={{ display: "block" }}>pub:{item.published_revision}</small>
                              )}
                            </td>
                            <td><small>{new Date(item.updated_at).toLocaleDateString()}</small></td>
                            <td>
                              <div className="action-group">
                                <button className="text-button" onClick={() => void openContentDetail(item.content_id)}>
                                  详情
                                </button>
                                {actions.canEdit && (
                                  <button className="text-button" onClick={() => openEditView(item)}>
                                    编辑
                                  </button>
                                )}
                                {actions.canSubmitReview && (
                                  <button className="text-button" onClick={() => void handleSubmitReview(item.content_id, item.revision)}>
                                    提交审核
                                  </button>
                                )}
                                {actions.canSubmitRevision && (
                                  <button className="text-button" onClick={() => openRevisionView(item)}>
                                    提交修订
                                  </button>
                                )}
                                {actions.canPublish && (
                                  <button className="text-button" onClick={() => void handlePublish(item.content_id, item.revision)}>
                                    发布
                                  </button>
                                )}
                                {actions.canOffline && (
                                  <button className="text-button" onClick={() => void handleStatusChange(item.content_id, "offline")}>
                                    下架
                                  </button>
                                )}
                                {actions.canRollback && (
                                  <button className="text-button" onClick={() => void openContentDetail(item.content_id)}>
                                    回滚
                                  </button>
                                )}
                                {actions.canArchive && (
                                  <button className="text-button" onClick={() => void handleStatusChange(item.content_id, "archived")}>
                                    归档
                                  </button>
                                )}
                                {actions.canDelete && (
                                  <button className="danger-button" onClick={() => void handleDeleteContent(item.content_id)}>
                                    删除
                                  </button>
                                )}
                              </div>
                            </td>
                          </tr>
                        );
                      })}
                    </tbody>
                  </table>
                </div>
              )}
            </section>
          )}

          {tab === "content" && (contentView === "create" || contentView === "edit" || contentView === "revision") && (
            <section className="ops-panel">
              <div className="panel-heading">
                <div>
                  <h2>{contentView === "create" ? "新增内容" : contentView === "revision" ? "提交修订" : "编辑内容"}</h2>
                  {editingContent && contentView === "revision" && (
                    <p>
                      基于 {RESOURCE_KIND_LABELS[editingContent.resource_kind]}：{editingContent.title}
                      （版本 {editingContent.revision}）创建新版本，保存后将进入审核流程
                    </p>
                  )}
                  {editingContent && contentView === "edit" && (
                    <p>
                      编辑 {RESOURCE_KIND_LABELS[editingContent.resource_kind]}：{editingContent.title}
                      （版本 {editingContent.revision}）
                    </p>
                  )}
                  {!editingContent && contentView === "create" && <p>填写内容基本信息</p>}
                </div>
                <button className="secondary-button" onClick={closeContentForm}>返回列表</button>
              </div>

              <div className="content-form">
                <div className="form-section">
                  <h3>基本信息</h3>
                  <div className="form-grid">
                    <label className="form-field">
                      资源类型
                      <select
                        value={contentForm.resource_kind}
                        onChange={(e) => updateFormField("resource_kind", e.target.value as ResourceKind)}
                        disabled={contentView === "edit"}
                      >
                        {RESOURCE_KIND_OPTIONS.map((k) => (
                          <option key={k} value={k}>{RESOURCE_KIND_LABELS[k]}</option>
                        ))}
                      </select>
                    </label>
                    <label className="form-field">
                      标题 <span className="required">*</span>
                      <input
                        value={contentForm.title}
                        onChange={(e) => updateFormField("title", e.target.value)}
                        placeholder="请输入标题"
                      />
                    </label>
                    <label className="form-field">
                      副标题
                      <input
                        value={contentForm.subtitle}
                        onChange={(e) => updateFormField("subtitle", e.target.value)}
                        placeholder="可选"
                      />
                    </label>
                    <label className="form-field full-width">
                      推荐摘要
                      <textarea
                        rows={3}
                        value={contentForm.summary}
                        onChange={(e) => updateFormField("summary", e.target.value)}
                        placeholder="简短的推荐描述"
                      />
                    </label>
                    <label className="form-field full-width">
                      主题
                      <div className="theme-selector">
                        {THEME_OPTIONS.map((theme) => (
                          <button
                            key={theme}
                            className={`theme-chip ${contentForm.theme_ids.includes(theme) ? "active" : ""}`}
                            onClick={() => handleToggleTheme(theme)}
                            type="button"
                          >
                            {theme}
                          </button>
                        ))}
                      </div>
                    </label>
                    <label className="form-field full-width">
                      封面图
                      <div className="cover-uploader">
                        <input
                          ref={coverFileRef}
                          type="file"
                          accept="image/*"
                          className="cover-file-input"
                          onChange={(e) => handleCoverFileChange(e.target.files)}
                        />
                        {coverPreviewUrl || (contentForm.cover_url && !contentForm.cover_url.startsWith("data:")) ? (
                          <div className="cover-preview-area">
                            <img
                              src={coverPreviewUrl || contentForm.cover_url}
                              alt="封面预览"
                              className="cover-preview-img"
                            />
                            <button
                              type="button"
                              className="cover-clear-btn"
                              onClick={() => {
                                setCoverPreviewUrl(null);
                                if (coverFileRef.current) coverFileRef.current.value = "";
                                updateFormField("cover_url", "");
                              }}
                            >
                              移除
                            </button>
                          </div>
                        ) : (
                          <div className="cover-drop-zone" onClick={() => coverFileRef.current?.click()}>
                            <span>点击或拖拽上传图片</span>
                          </div>
                        )}
                        {coverUploadError && <span className="cover-error">{coverUploadError}</span>}
                      </div>
                      <div className="cover-url-hint">
                        <span>或直接粘贴链接：</span>
                        <input
                          value={contentForm.cover_url && !contentForm.cover_url.startsWith("data:") ? contentForm.cover_url : ""}
                          onChange={(e) => {
                            setCoverPreviewUrl(null);
                            updateFormField("cover_url", e.target.value);
                          }}
                          placeholder="https://..."
                        />
                      </div>
                    </label>
                  </div>
                </div>

                {contentForm.resource_kind === "guide_card" && (
                  <div className="form-section">
                    <h3>导购卡片专属字段</h3>
                    <div className="form-grid">
                      <label className="form-field">
                        落地页链接 <span className="required">*</span>
                        <input
                          value={contentForm.landing_url}
                          onChange={(e) => updateFormField("landing_url", e.target.value)}
                          placeholder="https://..."
                        />
                      </label>
                      <label className="form-field">
                        外部商品ID
                        <input
                          value={contentForm.external_item_id}
                          onChange={(e) => updateFormField("external_item_id", e.target.value)}
                          placeholder="可选"
                        />
                      </label>
                      <div className="form-field full-width">
                        <label>联盟渠道引用</label>
                        {contentForm.affiliate_refs.map((ref, index) => (
                          <div key={index} className="affiliate-row">
                            <select
                              value={ref.channel}
                              onChange={(e) => updateAffiliateRef(index, "channel", e.target.value)}
                            >
                              <option value="PDD">拼多多</option>
                              <option value="DOUYIN">抖音</option>
                              <option value="JD">京东</option>
                              <option value="TAOBAO">淘宝</option>
                            </select>
                            <input
                              value={ref.external_item_id}
                              onChange={(e) => updateAffiliateRef(index, "external_item_id", e.target.value)}
                              placeholder="渠道侧商品ID"
                            />
                            <button className="text-button" onClick={() => removeAffiliateRef(index)} type="button">
                              移除
                            </button>
                          </div>
                        ))}
                        <button className="text-button" onClick={addAffiliateRef} type="button">
                          + 添加渠道
                        </button>
                      </div>
                      <div className="form-field full-width">
                        <label>卖点</label>
                        {contentForm.selling_points.map((sp, index) => (
                          <div key={index} className="selling-point-row">
                            <input
                              value={sp}
                              onChange={(e) => updateSellingPoint(index, e.target.value)}
                              placeholder={`卖点 ${index + 1}`}
                            />
                            {contentForm.selling_points.length > 1 && (
                              <button className="text-button" onClick={() => removeSellingPoint(index)} type="button">
                                移除
                              </button>
                            )}
                          </div>
                        ))}
                        <button className="text-button" onClick={addSellingPoint} type="button">
                          + 添加卖点
                        </button>
                      </div>
                      <label className="form-field checkbox-field">
                        <input
                          type="checkbox"
                          checked={contentForm.commercial_disclosure_required}
                          onChange={(e) => updateFormField("commercial_disclosure_required", e.target.checked)}
                        />
                        需要商业披露
                      </label>
                    </div>
                  </div>
                )}

                {contentForm.resource_kind === "editorial_content" && (
                  <div className="form-section">
                    <h3>图文攻略专属字段</h3>
                    <div className="form-grid">
                      <label className="form-field full-width">
                        内容块（暂为 textarea，富文本编辑器为后续版本目标）
                        <textarea
                          rows={8}
                          value=""
                          placeholder="内容块列表（段落/图片/嵌入卡片），后续版本接入富文本编辑器"
                          readOnly
                          className="readonly-textarea"
                        />
                      </label>
                      <label className="form-field">
                        作者名
                        <input placeholder="可选" />
                      </label>
                    </div>
                  </div>
                )}

                {contentForm.resource_kind === "topic" && (
                  <div className="form-section">
                    <h3>专题专属字段</h3>
                    <div className="form-grid">
                      <label className="form-field full-width">
                        专题模块
                        <textarea rows={4} value="" placeholder="专题模块列表（后续版本实现）" readOnly className="readonly-textarea" />
                      </label>
                      <label className="form-field">
                        SEO 路径
                        <input placeholder="/topic/xxx" />
                      </label>
                    </div>
                  </div>
                )}

                {contentForm.resource_kind === "ranking_list" && (
                  <div className="form-section">
                    <h3>榜单专属字段</h3>
                    <div className="form-grid">
                      <label className="form-field full-width">
                        榜单条目
                        <textarea rows={4} value="" placeholder="榜单条目（rank + card_id/content_id + blurb），后续版本实现" readOnly className="readonly-textarea" />
                      </label>
                      <label className="form-field full-width">
                        规则说明
                        <textarea rows={3} placeholder="榜单规则说明" />
                      </label>
                      <label className="form-field">
                        更新节奏
                        <select>
                          <option value="daily">每日</option>
                          <option value="weekly">每周</option>
                          <option value="monthly">每月</option>
                        </select>
                      </label>
                    </div>
                  </div>
                )}

                <div className="form-section">
                  <h3>生效时间</h3>
                  <div className="form-grid">
                    <label className="form-field">
                      生效时间
                      <input
                        type="datetime-local"
                        value={contentForm.effective_from}
                        onChange={(e) => updateFormField("effective_from", e.target.value)}
                      />
                    </label>
                    <label className="form-field">
                      失效时间
                      <input
                        type="datetime-local"
                        value={contentForm.effective_to}
                        onChange={(e) => updateFormField("effective_to", e.target.value)}
                      />
                    </label>
                  </div>
                </div>

                <div className="form-actions">
                  <button className="secondary-button" onClick={closeContentForm}>
                    取消
                  </button>
                  {contentView === "revision" ? (
                    <>
                      <button className="secondary-button" onClick={() => void handleSaveContentForRevision(false)}>
                        保存修订
                      </button>
                      <button className="primary-button" onClick={() => void handleSaveContentForRevision(true)}>
                        保存并提交审核
                      </button>
                    </>
                  ) : (
                    <>
                      <button className="secondary-button" onClick={() => void handleSaveContent(false)}>
                        {contentView === "create" ? "保存草稿" : "保存修改"}
                      </button>
                      <button className="primary-button" onClick={() => void handleSaveContent(true)}>
                        {contentView === "create" ? "保存并发布" : "保存并发布"}
                      </button>
                    </>
                  )}
                </div>
              </div>
            </section>
          )}

          {tab === "governance" && (
            <section className="ops-panel">
              <div className="panel-heading">
                <div>
                  <h2>治理审核</h2>
                  <p>审核通过不会自动发布，发布仍由内容运营显式触发。</p>
                </div>
                <button className="secondary-button" onClick={() => void loadReviews()}>刷新</button>
              </div>
              <div className="toolbar">
                <select value={filterReviewStatus} onChange={(event) => setFilterReviewStatus(event.target.value)} aria-label="审核状态过滤">
                  <option value="all">全部状态</option>
                  <option value="pending">pending</option>
                  <option value="in_review">in_review</option>
                  <option value="approved">approved</option>
                  <option value="rejected">rejected</option>
                  <option value="needs_info">needs_info</option>
                  <option value="completed">completed</option>
                </select>
              </div>
              {reviewState !== "success" ? (
                <StateMessage state={reviewState} emptyText="暂无审核单。" />
              ) : (
                <div className="ops-table-wrap">
                  <table className="ops-table">
                    <thead>
                      <tr><th>审核单</th><th>对象</th><th>类型</th><th>状态</th><th>入审原因</th><th>操作</th></tr>
                    </thead>
                    <tbody>
                      {filteredReviews.map((item) => (
                        <tr key={item.review_id}>
                          <td>{item.review_id}</td>
                          <td>{item.subject_id}</td>
                          <td>{item.resource_kind ? RESOURCE_KIND_LABELS[item.resource_kind] : "-"}</td>
                          <td><StatusBadge status={item.status} /></td>
                          <td><small>{item.enqueue_reason || "-"}</small></td>
                          <td>
                            {isActionable(item.status) ? (
                              <div className="action-group">
                                <button className="text-button" onClick={() => void updateReview(item.review_id, "approved")}>通过</button>
                                <button className="danger-button" onClick={() => void updateReview(item.review_id, "rejected")}>拒绝</button>
                              </div>
                            ) : "已处理"}
                          </td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
              )}
              <div className="visibility-panel">
                <div className="panel-heading visibility-panel-heading">
                  <div>
                    <h3>可见性裁决</h3>
                    <p>C 端可见须同时满足「内容已发布」且裁决为「允许展示」。</p>
                  </div>
                </div>
                <div className="visibility-form">
                  <label className="form-field">
                    内容 ID
                    <input
                      value={visibilityContentId}
                      onChange={(event) => setVisibilityContentId(event.target.value)}
                      placeholder="guide_card_1001"
                    />
                  </label>
                  <label className="form-field">
                    裁决状态
                    <select
                      value={visibilityState}
                      onChange={(event) => setVisibilityState(event.target.value as VisibilityState)}
                      aria-label="可见性裁决状态"
                    >
                      <option value="published">允许展示 (published)</option>
                      <option value="restricted">限制展示 (restricted)</option>
                      <option value="unpublished">不可见 (unpublished)</option>
                    </select>
                  </label>
                  <label className="form-field">
                    原因码（可选）
                    <input
                      value={visibilityReasonCode}
                      onChange={(event) => setVisibilityReasonCode(event.target.value)}
                      placeholder="manual_ops / policy_violation"
                    />
                  </label>
                  <button className="primary-button" type="button" onClick={() => void submitGovernanceVisibility()}>
                    提交可见性裁决
                  </button>
                </div>
              </div>
            </section>
          )}
        </div>

        <aside className="audit-card">
          <h2>最近操作</h2>
          <p>记录最近 12 次运营写操作与 gateway request_id。</p>
          {logs.length === 0 ? (
            <p className="empty-copy">暂无操作记录</p>
          ) : (
            <ul className="audit-list">
              {logs.slice(0, 12).map((log) => (
                <li key={log.id}>
                  <div>
                    <strong>{log.action}</strong>
                    <StatusBadge status={log.result} />
                  </div>
                  <p>{log.target}</p>
                  <small>{log.message}</small>
                  <small>request_id: {log.requestId}</small>
                  <time>{log.at}</time>
                </li>
              ))}
            </ul>
          )}
        </aside>
      </section>

      {detailOpen && (
        <div className="detail-overlay" onClick={closeContentDetail}>
          <div className="detail-drawer" onClick={(e) => e.stopPropagation()}>
            <div className="detail-header">
              <h2>内容详情</h2>
              <button className="close-btn" onClick={closeContentDetail}>&times;</button>
            </div>
            {detailLoading ? (
              <p className="state-copy">加载中...</p>
            ) : detailData ? (
              <div className="detail-body">
                <div className="detail-section">
                  <h3>基本信息</h3>
                  <dl className="detail-dl">
                    <dt>内容ID</dt><dd>{detailData.content.content_id}</dd>
                    <dt>资源类型</dt><dd>{RESOURCE_KIND_LABELS[detailData.content.resource_kind]}</dd>
                    <dt>标题</dt><dd>{detailData.content.title}</dd>
                    <dt>副标题</dt><dd>{detailData.content.subtitle || "-"}</dd>
                    <dt>主题</dt><dd>{detailData.content.theme_ids.join(", ") || "-"}</dd>
                    <dt>状态</dt><dd><StatusBadge status={detailData.content.content_status} /></dd>
                    <dt>当前版本</dt><dd>rev:{detailData.content.revision} / pub:{detailData.content.published_revision}</dd>
                    <dt>摘要</dt><dd>{detailData.content.summary || "-"}</dd>
                    <dt>落地页</dt><dd>{detailData.content.landing_url || "-"}</dd>
                    <dt>外部商品ID</dt><dd>{detailData.content.external_item_id || "-"}</dd>
                    <dt>更新时间</dt><dd>{new Date(detailData.content.updated_at).toLocaleString()}</dd>
                  </dl>
                </div>

                <div className="detail-section">
                  <h3>版本历史</h3>
                  {detailData.revision_history.length === 0 ? (
                    <p className="empty-copy">暂无版本历史</p>
                  ) : (
                    <table className="ops-table">
                      <thead>
                        <tr><th>版本</th><th>变更说明</th><th>创建者</th><th>时间</th><th>操作</th></tr>
                      </thead>
                      <tbody>
                        {detailData.revision_history.map((rev) => (
                          <tr key={rev.revision}>
                            <td>{rev.revision}</td>
                            <td><small>{rev.change_summary || "-"}</small></td>
                            <td>{rev.created_by}</td>
                            <td><small>{new Date(rev.created_at).toLocaleString()}</small></td>
                            <td>
                              {(detailData.content.content_status === "published" || detailData.content.content_status === "offline") && rev.revision < detailData.content.revision && (
                                <button
                                  className="text-button"
                                  onClick={() => setRollbackTargetRevision(rev.revision)}
                                >
                                  回滚到此版本
                                </button>
                              )}
                            </td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  )}
                </div>

                <div className="detail-section">
                  <h3>可见性裁决</h3>
                  {detailData.visibility_verdict ? (
                    <dl className="detail-dl">
                      <dt>裁决状态</dt><dd><StatusBadge status={detailData.visibility_verdict.state} /></dd>
                      <dt>来源</dt><dd>{detailData.visibility_verdict.source}</dd>
                      <dt>原因码</dt><dd>{detailData.visibility_verdict.reason_code || "-"}</dd>
                      <dt>生效时间</dt><dd>{detailData.visibility_verdict.effective_from ? new Date(detailData.visibility_verdict.effective_from).toLocaleString() : "-"}</dd>
                    </dl>
                  ) : (
                    <p className="empty-copy">暂无可见性裁决记录</p>
                  )}
                  <div className="action-group visibility-actions">
                    <button
                      className="text-button"
                      type="button"
                      onClick={() =>
                        void handleSetVisibility(detailData.content.content_id, "published", undefined, {
                          refreshDetail: true,
                        })
                      }
                    >
                      允许展示
                    </button>
                    <button
                      className="secondary-button"
                      type="button"
                      onClick={() =>
                        void handleSetVisibility(
                          detailData.content.content_id,
                          "restricted",
                          "manual_ops",
                          {
                            refreshDetail: true,
                            confirmMessage: `确认限制展示 ${detailData.content.content_id}？`,
                          },
                        )
                      }
                    >
                      限制展示
                    </button>
                    <button
                      className="danger-button"
                      type="button"
                      onClick={() =>
                        void handleSetVisibility(
                          detailData.content.content_id,
                          "unpublished",
                          "manual_ops",
                          {
                            refreshDetail: true,
                            confirmMessage: `确认将 ${detailData.content.content_id} 设为不可见？`,
                          },
                        )
                      }
                    >
                      紧急不可见
                    </button>
                  </div>
                </div>

                {detailData.review_state && (
                  <div className="detail-section">
                    <h3>审核状态</h3>
                    <dl className="detail-dl">
                      <dt>审核单ID</dt><dd>{detailData.review_state.review_id}</dd>
                      <dt>状态</dt><dd><StatusBadge status={detailData.review_state.status} /></dd>
                      <dt>提交时间</dt><dd>{new Date(detailData.review_state.submitted_at).toLocaleString()}</dd>
                      <dt>审核员</dt><dd>{detailData.review_state.reviewer_id || "-"}</dd>
                      <dt>审核意见</dt><dd>{detailData.review_state.comment || "-"}</dd>
                    </dl>
                  </div>
                )}

                {rollbackTargetRevision !== null && (
                  <div className="detail-section rollback-section">
                    <h3>版本回滚确认</h3>
                    <p>确认回滚到版本 {rollbackTargetRevision}？</p>
                    <label className="form-field">
                      变更说明
                      <input
                        value={rollbackSummary}
                        onChange={(e) => setRollbackSummary(e.target.value)}
                        placeholder="说明回滚原因"
                      />
                    </label>
                    <div className="form-actions">
                      <button className="secondary-button" onClick={() => setRollbackTargetRevision(null)}>取消</button>
                      <button className="primary-button" onClick={() => void handleRollback()}>确认回滚</button>
                    </div>
                  </div>
                )}
              </div>
            ) : (
              <p className="state-copy error">详情加载失败</p>
            )}
          </div>
        </div>
      )}
    </main>
  );
}

function StateMessage({ state, emptyText }: { state: LoadState; emptyText: string }) {
  if (state === "loading") return <p className="state-copy">加载中...</p>;
  if (state === "error") return <p className="state-copy error">加载失败，请重试。</p>;
  return <p className="state-copy">{emptyText}</p>;
}

function StatusBadge({ status }: { status: string }) {
  return <span className={`status-badge status-${status}`}>{status}</span>;
}
