import { useMemo } from "react";
import { FakeGatewayRepository } from "./gateway/fakeRepository";
import { OperationsConsolePage } from "./pages/OperationsConsolePage";

export default function App() {
  const api = useMemo(() => new FakeGatewayRepository(), []);
  return <OperationsConsolePage api={api} />;
}
