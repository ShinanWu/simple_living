#pragma once

#include <brpc/controller.h>
#include <string>

#include "gateway_pages_edge.pb.h"

namespace simple_living {
namespace gateway {
namespace pages {
namespace backoffice_media {

bool SaveBackofficeMediaUpload(const BackofficeMediaUploadRequest& req,
                               BackofficeMediaUploadResponse* resp);

bool ServeBackofficeMediaAsset(const std::string& filename, brpc::Controller* outer);

bool IsPersistableMediaUrl(const std::string& url);

std::string NormalizeMediaUrlForClient(const std::string& url);

std::string NormalizeMediaUrlForStore(const std::string& url);

}  // namespace backoffice_media
}  // namespace pages
}  // namespace gateway
}  // namespace simple_living
