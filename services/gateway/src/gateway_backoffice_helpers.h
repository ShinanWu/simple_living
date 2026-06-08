#pragma once

#include <brpc/controller.h>
#include <string>

#include "affiliate_server.pb.h"
#include "catalog.pb.h"
#include "content_service.pb.h"
#include "gateway_pages_edge.pb.h"
#include "governance_server.pb.h"

namespace simple_living {
namespace gateway {
namespace pages {
namespace backoffice_helpers {

bool EnsureBackofficeAuth(brpc::Controller* outer, std::string* role_out = nullptr);
bool ValidateBackofficeLoginToken(const std::string& access_token, std::string* session_token_out);

std::string ThemeIdFromTheme(const std::string& theme);
std::string ThemeFromThemeId(const std::string& theme_id);
std::string CanonicalThemeId(const std::string& raw);
std::string SanitizeIdPart(const std::string& raw);

std::string StatusFromContentStatus(simple_living::catalog::ContentLifecycleStatus status);
simple_living::catalog::ContentLifecycleStatus ContentStatusFromString(const std::string& status);

void FillBackofficeContentItem(const simple_living::catalog::GuideCard& card,
                               BackofficeContentItem* out);

simple_living::catalog::GuideCard BuildGuideCardFromCreate(
    const BackofficeCreateContentItemRequest& req);

void ApplyContentUpdate(const BackofficeUpdateContentItemRequest& req,
                        simple_living::catalog::GuideCard* card);

BackofficePartner PartnerFromSnapshot(
    const simple_living::affiliate_server::PartnerCapabilitySnapshot& snap);

simple_living::affiliate_server::PartnerCapabilitySnapshot SnapshotFromPartnerRequest(
    const BackofficeCreatePartnerRequest& req);

std::string PartnerLifecycleToStatus(
    simple_living::affiliate_server::PartnerLifecycleStatus status);

simple_living::affiliate_server::PartnerLifecycleStatus StatusToPartnerLifecycle(
    const std::string& status);

std::string VisibilityStateToString(simple_living::governance_server::VisibilityState state);
std::string VisibilitySourceToString(simple_living::governance_server::VisibilityVerdictSource source);
std::string ReviewQueueStatusToString(simple_living::governance_server::ReviewQueueItemStatus status);
std::string ReviewOutcomeToString(simple_living::governance_server::ReviewOutcome outcome);
simple_living::governance_server::ReviewOutcome ReviewOutcomeFromString(const std::string& status);

simple_living::governance_server::VisibilityState VisibilityStateFromString(const std::string& state);

}  // namespace backoffice_helpers
}  // namespace pages
}  // namespace gateway
}  // namespace simple_living
