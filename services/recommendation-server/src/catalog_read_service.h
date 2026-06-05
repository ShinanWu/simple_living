#pragma once

#include <brpc/server.h>

namespace simple_living {
namespace recommendation_server {

class SnapshotStore;
class CatalogReadServiceImpl;

CatalogReadServiceImpl* NewCatalogReadService(SnapshotStore* store);
bool RegisterCatalogReadService(brpc::Server* server, CatalogReadServiceImpl* impl);

}  // namespace recommendation_server
}  // namespace simple_living
