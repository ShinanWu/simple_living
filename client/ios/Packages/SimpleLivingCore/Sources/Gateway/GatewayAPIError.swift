import Foundation

public enum GatewayAPIError: Error, Equatable, Sendable {
    case business(code: Int, message: String)
    case transport(underlying: String)
    case decoding(underlying: String)
}
