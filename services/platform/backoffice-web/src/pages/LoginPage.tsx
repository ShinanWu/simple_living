import { useState } from "react";
import type { GatewayApiClient } from "../gateway/client";
import { setBackofficeSession } from "../services/auth/storage";
import "./LoginPage.css";

interface LoginPageProps {
  api: GatewayApiClient;
  onLoggedIn: () => void;
}

export function LoginPage({ api, onLoggedIn }: LoginPageProps) {
  const [accessToken, setAccessToken] = useState("");
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(false);

  const handleSubmit = async (event: React.FormEvent) => {
    event.preventDefault();
    setLoading(true);
    setError("");
    try {
      const resp = await api.loginBackoffice(accessToken.trim());
      if (!resp.success || !resp.data?.token) {
        setError(resp.message || "登录失败");
        return;
      }
      setBackofficeSession(resp.data.token, resp.data.role ?? "backoffice_admin");
      onLoggedIn();
    } catch {
      setError("无法连接 gateway，请检查网络或部署状态");
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="login-page">
      <form className="login-card" onSubmit={handleSubmit}>
        <h1>运营管理台</h1>
        <p className="login-hint">输入运营访问令牌以继续</p>
        <label htmlFor="access-token">访问令牌</label>
        <input
          id="access-token"
          type="password"
          value={accessToken}
          onChange={(e) => setAccessToken(e.target.value)}
          placeholder="simple-living-ops"
          autoComplete="current-password"
          required
        />
        {error ? <p className="login-error">{error}</p> : null}
        <button type="submit" disabled={loading || !accessToken.trim()}>
          {loading ? "登录中…" : "进入运营台"}
        </button>
      </form>
    </div>
  );
}
