#include "snapshot_reader.h"

#include <google/protobuf/util/json_util.h>

#include <fstream>
#include <sstream>

#include "catalog.pb.h"

namespace simple_living {
namespace tracking_server {

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

std::string ExtractLandingFromCardJson(const std::string& obj) {
    catalog::GuideCard card;
    google::protobuf::util::JsonParseOptions opts;
    opts.ignore_unknown_fields = true;
    if (!google::protobuf::util::JsonStringToMessage(obj, &card, opts).ok() ||
        card.card_id().empty()) {
        return "";
    }
    for (const auto& ref : card.affiliate_refs()) {
        const auto it = ref.payload().find("landing_url");
        if (it != ref.payload().end() && !it->second.empty()) {
            return it->second;
        }
    }
    return "";
}

}  // namespace

LinkSnapshotReader::LinkSnapshotReader(std::string snapshot_dir)
    : snapshot_dir_(std::move(snapshot_dir)) {
    LoadFromDisk();
}

bool LinkSnapshotReader::ReloadIfChanged() {
    std::lock_guard<std::mutex> lock(mu_);
    return LoadFromDisk();
}

bool LinkSnapshotReader::LoadFromDisk() {
    const std::string catalog_path = JoinPath(snapshot_dir_, "active/catalog.json");
    std::ifstream in(catalog_path);
    if (!in) {
        landing_by_card_.clear();
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    const std::string body = buffer.str();

    std::unordered_map<std::string, std::string> next;
    size_t i = 0;
    while ((i = body.find('{', i)) != std::string::npos) {
        size_t end = body.find('}', i);
        if (end == std::string::npos) {
            break;
        }
        const std::string obj = body.substr(i, end - i + 1);
        const std::string landing = ExtractLandingFromCardJson(obj);
        if (!landing.empty()) {
            catalog::GuideCard card;
            google::protobuf::util::JsonParseOptions opts;
            opts.ignore_unknown_fields = true;
            if (google::protobuf::util::JsonStringToMessage(obj, &card, opts).ok() &&
                !card.card_id().empty()) {
                next[card.card_id()] = landing;
            }
        }
        i = end + 1;
    }

    if (next.empty()) {
        landing_by_card_.clear();
        return false;
    }
    landing_by_card_ = std::move(next);
    ++loaded_version_;
    return true;
}

std::string LinkSnapshotReader::LandingUrlForCard(const std::string& guide_card_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    const auto it = landing_by_card_.find(guide_card_id);
    return it == landing_by_card_.end() ? "" : it->second;
}

}  // namespace tracking_server
}  // namespace simple_living
