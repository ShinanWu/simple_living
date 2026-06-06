#include "snapshot_store.h"

#include <google/protobuf/util/json_util.h>

#include "recommendation_server_models.pb.h"

#include <fstream>
#include <sstream>

namespace simple_living {
namespace recommendation_server {

namespace {

std::string JoinPath(const std::string& a, const std::string& b) {
    if (a.empty()) {
        return b;
    }
    if (a.back() == '/') {
        return a + b;
    }
    return a + "/" + b;
}

void ClearCatalog(std::unordered_map<std::string, catalog::GuideCard>* cards,
                  std::unordered_set<std::string>* visible) {
    cards->clear();
    visible->clear();
}

bool ParseVisibleIdsArray(const std::string& body, std::unordered_set<std::string>* out) {
    out->clear();
    const auto key = body.find("\"visible_ids\"");
    if (key == std::string::npos) {
        return false;
    }
    const auto arr_start = body.find('[', key);
    if (arr_start == std::string::npos) {
        return false;
    }
    const auto arr_end = body.find(']', arr_start);
    if (arr_end == std::string::npos) {
        return false;
    }
    size_t p = arr_start;
    while ((p = body.find('"', p + 1)) != std::string::npos && p < arr_end) {
        size_t q = body.find('"', p + 1);
        if (q == std::string::npos || q > arr_end) {
            break;
        }
        const std::string id = body.substr(p + 1, q - p - 1);
        if (!id.empty()) {
            out->insert(id);
        }
        p = q;
    }
    return !out->empty();
}

void FillPublishedVisible(const std::unordered_map<std::string, catalog::GuideCard>& cards,
                        std::unordered_set<std::string>* visible) {
    visible->clear();
    for (const auto& kv : cards) {
        if (!kv.second.has_content_status() ||
            kv.second.content_status() == catalog::CONTENT_LIFECYCLE_STATUS_PUBLISHED) {
            visible->insert(kv.first);
        }
    }
}

}  // namespace

SnapshotStore::SnapshotStore(std::string snapshot_dir)
    : snapshot_dir_(std::move(snapshot_dir)) {
    LoadFromDisk();
}

bool SnapshotStore::ReloadIfChanged() {
    std::lock_guard<std::mutex> lock(mu_);
    return LoadFromDisk();
}

bool SnapshotStore::LoadFromDisk() {
    const std::string catalog_path = JoinPath(snapshot_dir_, "active/catalog.json");
    std::ifstream in(catalog_path);
    if (!in) {
        ClearCatalog(&cards_, &visible_ids_);
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    const std::string body = buffer.str();

    std::unordered_map<std::string, catalog::GuideCard> next_cards;
    std::unordered_set<std::string> next_visible;

    CatalogSnapshotBundle bundle;
    google::protobuf::util::JsonParseOptions opts;
    opts.ignore_unknown_fields = true;
    if (!google::protobuf::util::JsonStringToMessage(body, &bundle, opts).ok()) {
        ClearCatalog(&cards_, &visible_ids_);
        return false;
    }
    for (const auto& card : bundle.cards()) {
        if (card.card_id().empty()) {
            continue;
        }
        next_cards[card.card_id()] = card;
        next_visible.insert(card.card_id());
    }

    const std::string vis_path = JoinPath(snapshot_dir_, "active/visibility.json");
    std::ifstream vin(vis_path);
    if (vin) {
        std::ostringstream vbuf;
        vbuf << vin.rdbuf();
        std::unordered_set<std::string> from_file;
        if (ParseVisibleIdsArray(vbuf.str(), &from_file)) {
            next_visible = std::move(from_file);
        }
    }
    if (next_visible.empty()) {
        FillPublishedVisible(next_cards, &next_visible);
    }

    if (next_cards.empty()) {
        ClearCatalog(&cards_, &visible_ids_);
        return false;
    }
    cards_ = std::move(next_cards);
    visible_ids_ = std::move(next_visible);
    ++loaded_version_;
    return true;
}

bool SnapshotStore::IsVisible(const std::string& card_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    return visible_ids_.count(card_id) > 0;
}

bool SnapshotStore::GetCard(const std::string& card_id, catalog::GuideCard* out) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = cards_.find(card_id);
    if (it == cards_.end() || visible_ids_.count(card_id) == 0) {
        return false;
    }
    *out = it->second;
    return true;
}

void SnapshotStore::ListCards(std::vector<catalog::GuideCard>* out) const {
    std::lock_guard<std::mutex> lock(mu_);
    out->clear();
    for (const auto& id : visible_ids_) {
        auto it = cards_.find(id);
        if (it != cards_.end()) {
            out->push_back(it->second);
        }
    }
}

}  // namespace recommendation_server
}  // namespace simple_living
