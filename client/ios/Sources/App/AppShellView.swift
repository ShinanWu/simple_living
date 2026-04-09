import SwiftUI
import SimpleLivingCore

struct AppShellView: View {
    private let api: GatewayAPI

    init(api: GatewayAPI) {
        self.api = api
    }

    var body: some View {
        TabView {
            NavigationStack {
                HomeView(api: api)
            }
            .tabItem {
                Label("首页", systemImage: "house")
            }

            MeSummaryView(api: api)
                .tabItem {
                    Label("我的", systemImage: "person")
                }
        }
    }
}
