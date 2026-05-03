import SwiftUI
import SimpleLivingCore

struct AppShellView: View {
    private let api: GatewayAPI

    init(api: GatewayAPI) {
        self.api = api
    }

    var body: some View {
        NavigationStack {
            HomeView(api: api)
        }
    }
}
