#include "snapshot_store.h"

#include <google/protobuf/util/json_util.h>

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

    const auto cards_pos = body.find("\"cards\"");
    if (cards_pos == std::string::npos) {
        ClearCatalog(&cards_, &visible_ids_);
        return false;
    }
    size_t i = cards_pos;
    while ((i = body.find('{', i)) != std::string::npos) {
        size_t end = body.find('}', i);
        if (end == std::string::npos) {
            break;
        }
        std::string obj = body.substr(i, end - i + 1);
        catalog::GuideCard card;
        google::protobuf::util::JsonParseOptions opts;
        opts.ignore_unknown_fields = true;
        if (google::protobuf::util::JsonStringToMessage(obj, &card, opts).ok() &&
            !card.card_id().empty()) {
            next_cards[card.card_id()] = card;
            next_visible.insert(card.card_id());
        }
        i = end + 1;
    }

    const std::string vis_path = JoinPath(snapshot_dir_, "active/visibility.json");
    std::ifstream vin(vis_path);
    if (vin) {
        std::ostringstream vbuf;
        vbuf << vin.rdbuf();
        const std::string vbody = vbuf.str();
        next_visible.clear();
        size_t p = 0;
        while ((p = vbody.find('"', p)) != std::string::npos) {
            size_t q = vbody.find('"', p + 1);
            if (q == std::string::npos) {
                break;
            }
            const std::string id = vbody.substr(p + 1, q - p - 1);
            if (id.find("guide_card") != std::string::npos || id.find("gc_") == 0) {
                next_visible.insert(id);
            }
            p = q + 1;
        }
    }

    if (next_cards.empty()) {
        ClearCatalog(&cards_, &visible_ids_);
        return false;
    }
    cards_ = std::move(next_cards);
    if (!next_visible.empty()) {
        visible_ids_ = std::move(next_visible);
    }
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
