#include "gateway_user_http_json.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <sstream>

namespace simple_living {
namespace gateway {
namespace user_http_json {

namespace {

std::string JsonEscape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':
                o += "\\\"";
                break;
            case '\\':
                o += "\\\\";
                break;
            case '\n':
                o += "\\n";
                break;
            case '\r':
                o += "\\r";
                break;
            case '\t':
                o += "\\t";
                break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    o += buf;
                } else {
                    o += static_cast<char>(c);
                }
        }
    }
    return o;
}

int64_t NowUnixMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

void WriteJsonEnvelope(brpc::Controller* outer,
                       bool success,
                       int code,
                       const std::string& message,
                       const std::string& data_json,
                       int http_status) {
    outer->manage_http_body_on_error(true);
    const std::string* req_id_hdr = outer->http_request().GetHeader("X-Request-Id");
    std::string req_id = (req_id_hdr && !req_id_hdr->empty()) ? *req_id_hdr : outer->request_id();
    if (req_id.empty()) {
        req_id = "gw_req";
    }

    std::ostringstream oss;
    oss << "{\"success\":" << (success ? "true" : "false") << ",\"code\":" << code << ",\"message\":\""
        << JsonEscape(message) << "\",\"data\":" << data_json << ",\"meta\":{\"request_id\":\""
        << JsonEscape(req_id) << "\",\"server_time_ms\":" << NowUnixMs() << "}}";

    outer->http_response().set_status_code(http_status);
    outer->http_response().set_content_type("application/json");
    outer->http_response().SetHeader("Access-Control-Allow-Origin", "*");
    outer->http_response().SetHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    outer->http_response().SetHeader("Access-Control-Allow-Headers",
                                     "Content-Type, Authorization, X-Guest-Session-Id, X-Request-Id, X-Backoffice-Role");
    outer->response_attachment().append(oss.str());
}

}  // namespace

std::string TimestampToIso8601Utc(const google::protobuf::Timestamp& ts) {
    if (ts.seconds() <= 0) {
        return "";
    }
    std::time_t t = static_cast<std::time_t>(ts.seconds());
    std::tm tm {};
    gmtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

std::string ContentRefTypeToJson(simple_living::user_server::ContentRefType type) {
    if (type == simple_living::user_server::CONTENT_REF_TYPE_GUIDE_CARD) {
        return "guide_card";
    }
    return "guide_card";
}

std::string BuildListFavoritesJson(const simple_living::user_server::ListFavoritesResponse& resp) {
    std::ostringstream oss;
    oss << "{\"items\":[";
    for (int i = 0; i < resp.items_size(); ++i) {
        if (i > 0) {
            oss << ',';
        }
        const auto& item = resp.items(i);
        oss << "{\"favorite_id\":\"" << JsonEscape(item.favorite_id()) << "\",\"guide_card_id\":\""
            << JsonEscape(item.content_id()) << "\",\"favorited_at\":\""
            << JsonEscape(item.has_favorited_at() ? TimestampToIso8601Utc(item.favorited_at()) : "")
            << "\"}";
    }
    oss << "],\"pagination\":{";
    if (resp.has_pagination()) {
        oss << "\"next_cursor\":";
        if (resp.pagination().has_next_cursor() && !resp.pagination().next_cursor().empty()) {
            oss << '"' << JsonEscape(resp.pagination().next_cursor()) << '"';
        } else {
            oss << "null";
        }
        oss << ",\"has_more\":" << (resp.pagination().has_more() ? "true" : "false");
        if (resp.pagination().has_limit()) {
            oss << ",\"limit\":" << resp.pagination().limit();
        }
    } else {
        oss << "\"next_cursor\":null,\"has_more\":false";
    }
    oss << "}}";
    return oss.str();
}

std::string BuildListHistoryJson(const simple_living::user_server::ListHistoryResponse& resp) {
    std::ostringstream oss;
    oss << "{\"items\":[";
    for (int i = 0; i < resp.items_size(); ++i) {
        if (i > 0) {
            oss << ',';
        }
        const auto& item = resp.items(i);
        std::string guide_id;
        std::string type_json = "guide_card";
        if (item.has_content_ref()) {
            guide_id = item.content_ref().content_id();
            type_json = ContentRefTypeToJson(item.content_ref().type());
        }
        oss << "{\"content_ref\":{\"type\":\"" << type_json << "\",\"guide_card_id\":\""
            << JsonEscape(guide_id) << "\"},\"last_seen_at\":\""
            << JsonEscape(item.has_last_seen_at() ? TimestampToIso8601Utc(item.last_seen_at()) : "")
            << "\"}";
    }
    oss << "],\"pagination\":{";
    if (resp.has_pagination()) {
        oss << "\"next_cursor\":";
        if (resp.pagination().has_next_cursor() && !resp.pagination().next_cursor().empty()) {
            oss << '"' << JsonEscape(resp.pagination().next_cursor()) << '"';
        } else {
            oss << "null";
        }
        oss << ",\"has_more\":" << (resp.pagination().has_more() ? "true" : "false");
        if (resp.pagination().has_limit()) {
            oss << ",\"limit\":" << resp.pagination().limit();
        }
    } else {
        oss << "\"next_cursor\":null,\"has_more\":false";
    }
    oss << "}}";
    return oss.str();
}

std::string BuildMeSummaryJson(const simple_living::user_server::GetMeSummaryResponse& resp) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"profile\":{";
    if (resp.has_profile()) {
        const auto& p = resp.profile();
        oss << "\"user_id\":";
        if (p.has_user_id() && !p.user_id().empty()) {
            oss << '"' << JsonEscape(p.user_id()) << '"';
        } else {
            oss << "null";
        }
        oss << ",\"is_guest\":" << (p.is_guest() ? "true" : "false");
        if (p.has_display_name()) {
            oss << ",\"display_name\":\"" << JsonEscape(p.display_name()) << '"';
        }
        if (p.has_avatar_url()) {
            oss << ",\"avatar_url\":\"" << JsonEscape(p.avatar_url()) << '"';
        }
    } else {
        oss << "\"user_id\":null,\"is_guest\":true";
    }
    oss << "},\"counts\":{";
    if (resp.has_counts()) {
        oss << "\"favorites_count\":" << resp.counts().favorites_count()
            << ",\"history_count\":" << resp.counts().history_count();
    } else {
        oss << "\"favorites_count\":0,\"history_count\":0";
    }
    oss << "}";
    if (resp.has_consent()) {
        const auto& c = resp.consent();
        oss << ",\"consent\":{";
        oss << "\"personalization_allowed\":" << (c.personalization_allowed() ? "true" : "false");
        if (c.has_consent_version()) {
            oss << ",\"consent_version\":\"" << JsonEscape(c.consent_version()) << '"';
        }
        if (c.has_updated_at()) {
            oss << ",\"updated_at\":\"" << JsonEscape(TimestampToIso8601Utc(c.updated_at())) << '"';
        }
        oss << '}';
    }
    oss << '}';
    return oss.str();
}

std::string BuildIssueTokenPairJson(const simple_living::user_server::IssueTokenPairResponse& resp) {
    std::ostringstream oss;
    oss << "{\"access_token\":\"" << JsonEscape(resp.access_token()) << "\",\"refresh_token\":\""
        << JsonEscape(resp.refresh_token()) << "\"";
    if (resp.has_expires_in_seconds()) {
        oss << ",\"expires_in\":" << resp.expires_in_seconds();
    }
    if (resp.has_session_id()) {
        oss << ",\"session_id\":\"" << JsonEscape(resp.session_id()) << '"';
    }
    if (resp.has_access_expires_at()) {
        oss << ",\"access_expires_at\":\"" << JsonEscape(TimestampToIso8601Utc(resp.access_expires_at()))
            << '"';
    }
    oss << '}';
    return oss.str();
}

std::string BuildHealthJson(const simple_living::user_server::HealthCheckResponse& resp) {
    std::ostringstream oss;
    const std::string status = resp.status().empty() ? "ok" : resp.status();
    oss << "{\"status\":\"" << JsonEscape(status) << "\",\"components\":[";
    for (int i = 0; i < resp.components_size(); ++i) {
        if (i > 0) {
            oss << ',';
        }
        const auto& c = resp.components(i);
        oss << "{\"name\":\"" << JsonEscape(c.name()) << "\",\"ok\":" << (c.ok() ? "true" : "false")
            << ",\"detail\":\"" << JsonEscape(c.detail()) << "\"}";
    }
    if (resp.components_size() == 0) {
        oss << "{\"name\":\"gateway\",\"ok\":true,\"detail\":\"\"}";
    }
    oss << "]}";
    return oss.str();
}

void FinishOkJson(brpc::Controller* outer, const std::string& data_json) {
    WriteJsonEnvelope(outer, true, 0, "OK", data_json, brpc::HTTP_STATUS_OK);
}

}  // namespace user_http_json
}  // namespace gateway
}  // namespace simple_living
