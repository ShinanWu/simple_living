import { useEffect, useState } from "react";
import { StateView } from "../components/StateView";
import type { GatewayApiClient } from "../gateway/client";
import type { MeSummaryData } from "../gateway/types";
import type { UiState } from "../types/ui-state";

interface MeSummaryPageProps {
  api: GatewayApiClient;
}

export function MeSummaryPage({ api }: MeSummaryPageProps) {
  const [state, setState] = useState<UiState>("loading");
  const [data, setData] = useState<MeSummaryData | null>(null);

  const load = async () => {
    setState("loading");
    const response = await api.getMeSummary();
    if (response.code === 0 && response.data) {
      setData(response.data);
      setState("success");
      return;
    }
    setState(response.code === 90003 ? "offline" : "error");
  };

  useEffect(() => {
    void load();
  }, []);

  return (
    <section>
      <h2>MeSummary（占位页）</h2>
      <StateView state={state} onRetry={() => void load()} hasData={Boolean(data)}>
        <p>用户：{data?.profile.user_id}</p>
        <p>身份：{data?.profile.is_guest ? "访客" : "登录用户"}</p>
        <p>收藏：{data?.counts.favorites_count}</p>
        <p>历史：{data?.counts.history_count}</p>
      </StateView>
    </section>
  );
}
