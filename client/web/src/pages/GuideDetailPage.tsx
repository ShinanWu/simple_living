import { useEffect, useState } from "react";
import { StateView } from "../components/StateView";
import type { GatewayApiClient } from "../gateway/client";
import type { HomeFeedItem } from "../gateway/types";
import type { UiState } from "../types/ui-state";

interface GuideDetailPageProps {
  api: GatewayApiClient;
  item: HomeFeedItem;
  onBack: () => void;
  onGoBuy: (item: HomeFeedItem) => void;
}

export function GuideDetailPage({ api, item, onBack, onGoBuy }: GuideDetailPageProps) {
  const [state, setState] = useState<UiState>("loading");
  const [title, setTitle] = useState("");

  const load = async () => {
    setState("loading");
    const response = await api.getGuideDetail({ guide_card_id: item.guide_card_id, include_related: true });
    if (response.code === 0 && response.data) {
      setTitle(response.data.guide.title);
      setState("success");
      return;
    }
    if (response.code === 30001) {
      setState("empty");
      return;
    }
    setState(response.code === 90003 ? "offline" : "error");
  };

  useEffect(() => {
    void load();
  }, [item.guide_card_id]);

  return (
    <section>
      <h2>GuideDetail（占位页）</h2>
      <button onClick={onBack}>返回首页</button>
      <StateView state={state} onRetry={() => void load()} hasData={Boolean(title)}>
        <p>标题：{title}</p>
        <p>guide_card_id：{item.guide_card_id}</p>
        <button onClick={() => onGoBuy(item)}>去购买</button>
      </StateView>
    </section>
  );
}
