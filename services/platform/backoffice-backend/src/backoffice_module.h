#pragma once

#include <brpc/server.h>
#include <string>
#include <vector>

#include "content_service.pb.h"

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

class SnapshotExportCoordinator;

bool RegisterContentModule(brpc::Server* server,
                           const std::string& pg_conninfo,
                           content_server::ContentServiceImpl** out_svc,
                           SnapshotExportCoordinator* export_coord = nullptr);
void ShutdownContentModule(content_server::ContentServiceImpl* svc);
std::vector<catalog::GuideCard> CollectPublishedCards(
    content_server::ContentServiceImpl* svc);

struct GovernanceModule;
bool RegisterGovernanceModule(brpc::Server* server,
                              const std::string& pg_conninfo,
                              GovernanceModule** out_mod,
                              SnapshotExportCoordinator* export_coord = nullptr);
void ShutdownGovernanceModule(GovernanceModule* mod);

struct AffiliateModule;
bool RegisterAffiliateModule(brpc::Server* server,
                             const std::string& pg_conninfo,
                             AffiliateModule** out_mod,
                             SnapshotExportCoordinator* export_coord = nullptr);
void ShutdownAffiliateModule(AffiliateModule* mod);

::simple_living::governance_server::PgGovernanceStore* GovernanceModuleStore(GovernanceModule* mod);
::simple_living::affiliate_server::PgAffiliateStore* AffiliateModuleStore(AffiliateModule* mod);

}  // namespace backoffice_backend
}  // namespace simple_living
