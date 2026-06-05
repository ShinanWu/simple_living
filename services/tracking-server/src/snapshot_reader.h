#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

namespace simple_living {
namespace tracking_server {

// Reads catalog + affiliate_link_spec snapshots exported by backoffice-backend.
class LinkSnapshotReader {
public:
    explicit LinkSnapshotReader(std::string snapshot_dir);

    bool ReloadIfChanged();
    std::string LandingUrlForCard(const std::string& guide_card_id) const;

private:
    bool LoadFromDisk();

    std::string snapshot_dir_;
    mutable std::mutex mu_;
    std::unordered_map<std::string, std::string> landing_by_card_;
    uint64_t loaded_version_{0};
};

}  // namespace tracking_server
}  // namespace simple_living
