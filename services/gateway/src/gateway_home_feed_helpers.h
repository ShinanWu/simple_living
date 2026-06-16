#pragma once

#include <string>
#include <vector>

#include "catalog.pb.h"
#include "gateway_pages_edge.pb.h"

namespace simple_living {
namespace gateway {
namespace pages {
namespace home_feed {

std::string TrimCopy(const std::string& raw);

void HydrateGuideCardSummary(GuideCardSummary* summary,
                             const simple_living::catalog::GuideCard& card,
                             const std::string& theme);

std::string BuildReasonText(const simple_living::catalog::GuideCard& card,
                            const std::vector<std::string>& recommendation_reason_tags,
                            const std::string& explanation_summary);

void FillHomeFeedItemFromCard(HomeFeedItem* item,
                              const simple_living::catalog::GuideCard& card,
                              const std::string& theme,
                              const std::string& recommendation_id,
                              const std::string& scene,
                              int rank,
                              const std::vector<std::string>& recommendation_reason_tags,
                              const std::string& explanation_summary);

}  // namespace home_feed
}  // namespace pages
}  // namespace gateway
}  // namespace simple_living
