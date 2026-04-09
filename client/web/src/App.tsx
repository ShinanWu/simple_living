import { useMemo, useState } from "react";
import { AppShell, type RootPage } from "./components/AppShell";
import { FakeGatewayRepository } from "./gateway/fakeRepository";
import type { HomeFeedItem } from "./gateway/types";
import { GuideDetailPage } from "./pages/GuideDetailPage";
import { HomePage } from "./pages/HomePage";
import { MeSummaryPage } from "./pages/MeSummaryPage";
import { RedirectPreparePage } from "./pages/RedirectPreparePage";

type OverlayPage = "guide_detail" | "redirect_prepare";

export default function App() {
  const api = useMemo(() => new FakeGatewayRepository(), []);
  const [rootPage, setRootPage] = useState<RootPage>("home");
  const [overlayPage, setOverlayPage] = useState<OverlayPage | null>(null);
  const [activeItem, setActiveItem] = useState<HomeFeedItem | null>(null);

  const openGuideDetail = (item: HomeFeedItem) => {
    setActiveItem(item);
    setOverlayPage("guide_detail");
  };

  const openRedirectPrepare = (item: HomeFeedItem) => {
    setActiveItem(item);
    setOverlayPage("redirect_prepare");
  };

  const homeContent =
    overlayPage === "guide_detail" && activeItem ? (
      <GuideDetailPage
        api={api}
        item={activeItem}
        onBack={() => setOverlayPage(null)}
        onGoBuy={openRedirectPrepare}
      />
    ) : overlayPage === "redirect_prepare" && activeItem ? (
      <RedirectPreparePage
        api={api}
        item={activeItem}
        onBack={() => setOverlayPage("guide_detail")}
      />
    ) : (
      <HomePage api={api} onOpenGuideDetail={openGuideDetail} />
    );

  return (
    <AppShell
      active={rootPage}
      onChange={(page) => {
        setRootPage(page);
        setOverlayPage(null);
      }}
    >
      {rootPage === "home" ? homeContent : <MeSummaryPage api={api} />}
    </AppShell>
  );
}
