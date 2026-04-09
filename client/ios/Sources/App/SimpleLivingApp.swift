import SwiftUI
import SimpleLivingCore

@main
struct SimpleLivingApp: App {
    private let api: GatewayAPI = GatewayRuntime.makeFromEnvironment()

    var body: some Scene {
        WindowGroup {
            AppShellView(api: api)
        }
    }
}
