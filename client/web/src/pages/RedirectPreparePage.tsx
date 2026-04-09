import { useEffect, useState } from "react";
import { StateView } from "../components/StateView";
import type { GatewayApiClient } from "../gateway/client";
import type { HomeFeedItem } from "../gateway/types";
import type { UiState } from "../types/ui-state";

interface RedirectPreparePageProps {
  api: GatewayApiClient;
  item: HomeFeedItem;
  onBack: () => void;
}

export function RedirectPreparePage({ api, item, onBack }: RedirectPreparePageProps) {
  const [state, setState] = useState<UiState>("loading");
  const [landingUrl, setLandingUrl] = useState("");

  const prepare = async () => {
    setState("loading");
    const response = await api.postRedirectPrepare({
      guide_card_id: item.guide_card_id,
      recommendation_id: item.recommendation_id,
      scene: item.scene,
      item_rank: item.rank,
    });
    if (response.code === 0 && response.data?.landing_url) {
      setLandingUrl(response.data.landing_url);
      setState("success");
      return;
    }
    setState(response.code === 90003 ? "offline" : "error");
  };

  useEffect(() => {
    void prepare();
  }, [item.guide_card_id, item.recommendation_id, item.rank, item.scene]);

  return (
    <section>
      <h2>RedirectPrepare（占位页）</h2>
      <button onClick={onBack}>返回详情</button>
      <StateView state={state} onRetry={() => void prepare()} hasData={Boolean(landingUrl)}>
        <p>已生成跳转地址：</p>
        <code>{landingUrl}</code>
      </StateView>
    </section>
  );
}
