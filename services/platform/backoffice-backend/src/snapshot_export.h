#pragma once

#include "export_publisher.h"

namespace simple_living {
namespace content_server {
class ContentServiceImpl;
}
namespace backoffice_backend {

// Refreshes catalog / visibility / affiliate snapshot bundles after backoffice writes.
class SnapshotExportCoordinator {
public:
    void Bind(ExportPublisher* publisher, content_server::ContentServiceImpl* content);
    void RefreshNow();

private:
    ExportPublisher* publisher_{nullptr};
    content_server::ContentServiceImpl* content_{nullptr};
};

}  // namespace backoffice_backend
}  // namespace simple_living
