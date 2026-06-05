import { useMemo } from "react";
import { HttpGatewayApiClient } from "./gateway/client";
import { OperationsConsolePage } from "./pages/OperationsConsolePage";

export default function App() {
  const api = useMemo(
    () => new HttpGatewayApiClient(import.meta.env.VITE_GATEWAY_BASE_URL ?? "http://8.152.103.12"),
    [],
  );
  return <OperationsConsolePage api={api} />;
}
