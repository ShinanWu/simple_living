import Foundation

public enum GatewayAPIError: Error, LocalizedError, Equatable, Sendable {
    case business(code: Int, message: String)
    case transport(underlying: String)
    case decoding(underlying: String)
    
    public var errorDescription: String? {
        switch self {
        case .business(let code, let message):
            return "Gateway business error (\(code)): \(message)"
        case .transport(let underlying):
            return "Gateway transport error: \(underlying)"
        case .decoding(let underlying):
            return "Gateway decoding error: \(underlying)"
        }
    }
    
    public static func business(code: Int, _ message: String) -> GatewayAPIError {
        .business(code: code, message: message)
    }
}
