#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "catalog.pb.h"
#include "affiliate_server.pb.h"

namespace simple_living {
namespace backoffice_backend {

// Writes versioned snapshot bundles for co-located recommendation-server / tracking-server.
class ExportPublisher {
public:
    explicit ExportPublisher(std::string export_dir);

    bool PublishCatalog(const std::vector<catalog::GuideCard>& cards);
    bool PublishVisibility(const std::vector<std::string>& visible_card_ids);
    bool PublishAffiliateSpec();
    bool PublishAffiliateSpec(const std::vector<::simple_living::affiliate_server::PartnerCapabilitySnapshot>& partners);

    uint64_t version() const { return version_; }

private:
    bool WriteManifest(const std::string& bundle, const std::string& filename, const std::string& body);

    std::string export_dir_;
    uint64_t version_{0};
};

}  // namespace backoffice_backend
}  // namespace simple_living
