#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "catalog.pb.h"

namespace simple_living {
namespace recommendation_server {

class SnapshotStore {
public:
    explicit SnapshotStore(std::string snapshot_dir);

    bool ReloadIfChanged();
    bool IsVisible(const std::string& card_id) const;
    bool GetCard(const std::string& card_id, catalog::GuideCard* out) const;
    void ListCards(std::vector<catalog::GuideCard>* out) const;

private:
    bool LoadFromDisk();

    std::string snapshot_dir_;
    mutable std::mutex mu_;
    std::unordered_map<std::string, catalog::GuideCard> cards_;
    std::unordered_set<std::string> visible_ids_;
    uint64_t loaded_version_{0};
};

}  // namespace recommendation_server
}  // namespace simple_living
