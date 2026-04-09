import Foundation

public enum ViewLoadState<Value> {
    case loading
    case success(Value)
    case empty
    case error(message: String)
    case offline
}
