import { useMemo, useState } from "react";
import { HttpGatewayApiClient } from "./gateway/client";
import { isBackofficeLoggedIn } from "./services/auth/storage";
import { LoginPage } from "./pages/LoginPage";
import { OperationsConsolePage } from "./pages/OperationsConsolePage";

export default function App() {
  const api = useMemo(
    () => new HttpGatewayApiClient(import.meta.env.VITE_GATEWAY_BASE_URL ?? ""),
    [],
  );
  const [loggedIn, setLoggedIn] = useState(isBackofficeLoggedIn());

  if (!loggedIn) {
    return <LoginPage api={api} onLoggedIn={() => setLoggedIn(true)} />;
  }

  return <OperationsConsolePage api={api} onLogout={() => setLoggedIn(false)} />;
}
