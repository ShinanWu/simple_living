import Foundation

public enum Theme: String, CaseIterable, Identifiable, Sendable {
    case clothing
    case food
    case housing
    case transport

    public var id: String { rawValue }

    public var title: String {
        switch self {
        case .clothing: return "衣"
        case .food: return "食"
        case .housing: return "住"
        case .transport: return "行"
        }
    }
}
