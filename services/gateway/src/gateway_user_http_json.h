#pragma once

#include <string>

#include <brpc/controller.h>

#include "user_server_models.pb.h"

namespace simple_living {
namespace gateway {
namespace user_http_json {

std::string TimestampToIso8601Utc(const google::protobuf::Timestamp& ts);
std::string ContentRefTypeToJson(simple_living::user_server::ContentRefType type);

std::string BuildListFavoritesJson(const simple_living::user_server::ListFavoritesResponse& resp);
std::string BuildListHistoryJson(const simple_living::user_server::ListHistoryResponse& resp);
std::string BuildMeSummaryJson(const simple_living::user_server::GetMeSummaryResponse& resp);
std::string BuildIssueTokenPairJson(const simple_living::user_server::IssueTokenPairResponse& resp);
std::string BuildHealthJson(const simple_living::user_server::HealthCheckResponse& resp);

void FinishOkJson(brpc::Controller* outer, const std::string& data_json);

}  // namespace user_http_json
}  // namespace gateway
}  // namespace simple_living
