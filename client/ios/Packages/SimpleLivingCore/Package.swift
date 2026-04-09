// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "SimpleLivingCore",
    platforms: [.iOS("18.0"), .macOS(.v13)],
    products: [
        .library(name: "SimpleLivingCore", targets: ["SimpleLivingCore"]),
    ],
    targets: [
        .target(
            name: "SimpleLivingCore",
            path: "Sources"
        ),
        .testTarget(
            name: "SimpleLivingCoreTests",
            dependencies: ["SimpleLivingCore"],
            path: "Tests/SimpleLivingCoreTests"
        ),
    ]
)
