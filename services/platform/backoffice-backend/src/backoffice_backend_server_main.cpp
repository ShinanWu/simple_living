// platform/backoffice-backend — merged content + governance + affiliate write surface.
#include <gflags/gflags.h>
#include <brpc/server.h>
#include <butil/logging.h>

#include <vector>

#include "backoffice_module.h"
#include "export_publisher.h"
#include "snapshot_export.h"

DEFINE_int32(port, 9110, "TCP port for backoffice-backend brpc server");
DEFINE_string(pg_conninfo,
              "host=127.0.0.1 port=5432 dbname=simple_living user=simple password=simple",
              "PostgreSQL connection string");
DEFINE_string(export_dir, "/var/lib/simple-living/exports", "Snapshot export root directory");

int main(int argc, char* argv[]) {
    GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);

    brpc::Server server;
    simple_living::backoffice_backend::ExportPublisher exporter(FLAGS_export_dir);
    simple_living::backoffice_backend::SnapshotExportCoordinator export_coord;
    simple_living::content_server::ContentServiceImpl* content_svc = nullptr;
    simple_living::backoffice_backend::GovernanceModule* governance_mod = nullptr;
    simple_living::backoffice_backend::AffiliateModule* affiliate_mod = nullptr;

    if (!simple_living::backoffice_backend::RegisterContentModule(
            &server, FLAGS_pg_conninfo, &content_svc, &export_coord) ||
        !simple_living::backoffice_backend::RegisterGovernanceModule(
            &server, FLAGS_pg_conninfo, &governance_mod, &export_coord) ||
        !simple_living::backoffice_backend::RegisterAffiliateModule(
            &server, FLAGS_pg_conninfo, &affiliate_mod)) {
        LOG(ERROR) << "backoffice-backend module registration failed";
        return 1;
    }

    export_coord.Bind(&exporter, content_svc);
    export_coord.RefreshNow();

    brpc::ServerOptions options;
    if (server.Start(FLAGS_port, &options) != 0) {
        LOG(ERROR) << "Fail to start backoffice-backend server";
        return 1;
    }
    LOG(INFO) << "backoffice-backend listening on port " << FLAGS_port
              << " export_dir=" << FLAGS_export_dir;
    server.RunUntilAskedToQuit();

    simple_living::backoffice_backend::ShutdownAffiliateModule(affiliate_mod);
    simple_living::backoffice_backend::ShutdownGovernanceModule(governance_mod);
    simple_living::backoffice_backend::ShutdownContentModule(content_svc);
    return 0;
}
