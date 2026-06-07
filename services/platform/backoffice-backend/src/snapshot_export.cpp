#include "snapshot_export.h"

#include <vector>

#include "affiliate_server.pb.h"
#include "backoffice_module.h"
#include "pg_affiliate_store.h"
#include "pg_governance_store.h"

namespace simple_living {
namespace backoffice_backend {

void SnapshotExportCoordinator::Bind(
    ExportPublisher* publisher,
    content_server::ContentServiceImpl* content,
    ::simple_living::governance_server::PgGovernanceStore* governance,
    ::simple_living::affiliate_server::PgAffiliateStore* affiliate) {
    publisher_ = publisher;
    content_ = content;
    governance_ = governance;
    affiliate_ = affiliate;
}

void SnapshotExportCoordinator::RefreshNow() {
    if (!publisher_ || !content_) {
        return;
    }
    const auto cards = CollectPublishedCards(content_);
    std::vector<std::string> visible_ids;
    visible_ids.reserve(cards.size());
    for (const auto& card : cards) {
        if (!governance_ || governance_->IsCsideVisible(card.card_id())) {
            visible_ids.push_back(card.card_id());
        }
    }
    publisher_->PublishCatalog(cards);
    publisher_->PublishVisibility(visible_ids);
    if (affiliate_) {
        std::vector<::simple_living::affiliate_server::PartnerCapabilitySnapshot> partners;
        if (affiliate_->ListPartners(&partners)) {
            publisher_->PublishAffiliateSpec(partners);
            return;
        }
    }
    publisher_->PublishAffiliateSpec({});
}

}  // namespace backoffice_backend
}  // namespace simple_living
