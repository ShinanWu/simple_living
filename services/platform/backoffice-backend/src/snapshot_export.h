#pragma once

#include "export_publisher.h"

namespace simple_living {
namespace content_server {
class ContentServiceImpl;
}
namespace governance_server {
class PgGovernanceStore;
}
namespace affiliate_server {
class PgAffiliateStore;
}
namespace backoffice_backend {

// Refreshes catalog / visibility / affiliate snapshot bundles after backoffice writes.
class SnapshotExportCoordinator {
public:
    void Bind(ExportPublisher* publisher,
              content_server::ContentServiceImpl* content,
              ::simple_living::governance_server::PgGovernanceStore* governance = nullptr,
              ::simple_living::affiliate_server::PgAffiliateStore* affiliate = nullptr);
    void RefreshNow();

private:
    ExportPublisher* publisher_{nullptr};
    content_server::ContentServiceImpl* content_{nullptr};
    ::simple_living::governance_server::PgGovernanceStore* governance_{nullptr};
    ::simple_living::affiliate_server::PgAffiliateStore* affiliate_{nullptr};
};

}  // namespace backoffice_backend
}  // namespace simple_living
