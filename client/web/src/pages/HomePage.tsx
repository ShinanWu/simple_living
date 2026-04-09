import { useEffect, useState } from "react";
import { StateView } from "../components/StateView";
import type { GatewayApiClient } from "../gateway/client";
import type { HomeFeedItem } from "../gateway/types";
import { THEME_TABS, type Theme } from "../types/theme";
import type { UiState } from "../types/ui-state";

interface HomePageProps {
  api: GatewayApiClient;
  onOpenGuideDetail: (item: HomeFeedItem) => void;
}

export function HomePage({ api, onOpenGuideDetail }: HomePageProps) {
  const [theme, setTheme] = useState<Theme>("clothing");
  const [state, setState] = useState<UiState>("loading");
  const [items, setItems] = useState<HomeFeedItem[]>([]);

  const load = async (nextTheme: Theme) => {
    setState("loading");
    const response = await api.getHomeFeed({ theme: nextTheme, cursor: "", limit: 20 });
    if (response.code === 0 && response.data) {
      setItems(response.data.items);
      setState(response.data.items.length === 0 ? "empty" : "success");
      return;
    }
    setState(response.code === 90003 ? "offline" : "error");
  };

  useEffect(() => {
    void load(theme);
  }, [theme]);

  return (
    <section>
      <h2>首页</h2>
      <div style={{ display: "flex", gap: 8, marginBottom: 12 }}>
        {THEME_TABS.map((tab) => (
          <button
            key={tab.key}
            onClick={() => setTheme(tab.key)}
            disabled={tab.key === theme}
          >
            {tab.label}
          </button>
        ))}
      </div>

      <StateView state={state} onRetry={() => void load(theme)} hasData={items.length > 0}>
        <ul style={{ display: "grid", gap: 12, padding: 0, listStyle: "none" }}>
          {items.map((item) => (
            <li key={`${item.recommendation_id}-${item.rank}`} style={{ border: "1px solid #eee", padding: 12 }}>
              <p>
                <strong>{item.guide_card_id}</strong>
              </p>
              <p>推荐解释：{item.reason_tags.join(" / ") || "-"}</p>
              <button onClick={() => onOpenGuideDetail(item)}>查看详情</button>
            </li>
          ))}
        </ul>
      </StateView>
    </section>
  );
}
