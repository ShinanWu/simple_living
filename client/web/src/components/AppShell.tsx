import { ReactNode } from "react";

export type RootPage = "home" | "me";

interface AppShellProps {
  active: RootPage;
  onChange: (page: RootPage) => void;
  children: ReactNode;
}

export function AppShell({ active, onChange, children }: AppShellProps) {
  return (
    <div style={{ maxWidth: 640, margin: "0 auto", padding: 16 }}>
      <main>{children}</main>
      <nav
        style={{
          position: "sticky",
          bottom: 0,
          display: "grid",
          gridTemplateColumns: "1fr 1fr",
          gap: 8,
          marginTop: 20,
          padding: 8,
          background: "#fafafa",
          border: "1px solid #eee",
          borderRadius: 12,
        }}
      >
        <button disabled={active === "home"} onClick={() => onChange("home")}>
          首页
        </button>
        <button disabled={active === "me"} onClick={() => onChange("me")}>
          我的
        </button>
      </nav>
    </div>
  );
}
