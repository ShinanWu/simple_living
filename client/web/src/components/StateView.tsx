import { ReactNode } from "react";
import type { UiState } from "../types/ui-state";

interface StateViewProps {
  state: UiState;
  onRetry?: () => void;
  children: ReactNode;
  hasData?: boolean;
}

export function StateView({ state, onRetry, children, hasData }: StateViewProps) {
  if (state === "loading") return <p>加载中...</p>;
  if (state === "offline")
    return (
      <div>
        <p>网络不可用（offline）</p>
        {onRetry ? <button onClick={onRetry}>重试</button> : null}
      </div>
    );
  if (state === "error")
    return (
      <div>
        <p>请求失败（error）</p>
        {onRetry ? <button onClick={onRetry}>重试</button> : null}
      </div>
    );
  if (state === "empty" || !hasData)
    return (
      <div>
        <p>暂无内容（empty）</p>
        {onRetry ? <button onClick={onRetry}>刷新</button> : null}
      </div>
    );
  return <>{children}</>;
}
