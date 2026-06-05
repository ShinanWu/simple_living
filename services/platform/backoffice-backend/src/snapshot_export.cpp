#include "snapshot_export.h"

#include <vector>

#include "backoffice_module.h"

namespace simple_living {
namespace backoffice_backend {

void SnapshotExportCoordinator::Bind(ExportPublisher* publisher,
                                     content_server::ContentServiceImpl* content) {
    publisher_ = publisher;
    content_ = content;
}

void SnapshotExportCoordinator::RefreshNow() {
    if (!publisher_ || !content_) {
        return;
    }
    const auto cards = CollectPublishedCards(content_);
    std::vector<std::string> visible_ids;
    visible_ids.reserve(cards.size());
    for (const auto& card : cards) {
        visible_ids.push_back(card.card_id());
    }
    publisher_->PublishCatalog(cards);
    publisher_->PublishVisibility(visible_ids);
    publisher_->PublishAffiliateSpec();
}

}  // namespace backoffice_backend
}  // namespace simple_living
