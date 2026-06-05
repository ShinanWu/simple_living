#include "export_publisher.h"

#include <google/protobuf/util/json_util.h>

#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace simple_living {
namespace backoffice_backend {

namespace {

bool EnsureDir(const std::string& path) {
    struct stat st {};
    if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }
    return mkdir(path.c_str(), 0755) == 0;
}

std::string JoinPath(const std::string& a, const std::string& b) {
    if (a.empty()) {
        return b;
    }
    if (a.back() == '/') {
        return a + b;
    }
    return a + "/" + b;
}

}  // namespace

ExportPublisher::ExportPublisher(std::string export_dir) : export_dir_(std::move(export_dir)) {}

bool ExportPublisher::WriteManifest(const std::string& bundle,
                                    const std::string& filename,
                                    const std::string& body) {
    const std::string staging = JoinPath(export_dir_, "staging");
    const std::string active = JoinPath(export_dir_, "active");
    if (!EnsureDir(export_dir_) || !EnsureDir(staging) || !EnsureDir(active)) {
        return false;
    }
    const std::string staging_path = JoinPath(staging, filename);
    std::ofstream out(staging_path, std::ios::trunc);
    if (!out) {
        return false;
    }
    out << body;
    out.close();
    const std::string active_path = JoinPath(active, filename);
    std::ifstream in(staging_path);
    std::ofstream active_out(active_path, std::ios::trunc);
    active_out << in.rdbuf();

    std::ostringstream manifest;
    manifest << "{\"bundle\":\"" << bundle << "\",\"version\":" << version_
             << ",\"file\":\"" << filename << "\"}\n";
    std::ofstream manifest_out(JoinPath(active, bundle + ".manifest"), std::ios::trunc);
    manifest_out << manifest.str();
    return true;
}

bool ExportPublisher::PublishCatalog(const std::vector<catalog::GuideCard>& cards) {
    ++version_;
    google::protobuf::util::JsonPrintOptions opts;
    opts.preserve_proto_field_names = true;
    std::ostringstream oss;
    oss << "{\"cards\":[";
    bool first = true;
    for (const auto& card : cards) {
        std::string json;
        if (!google::protobuf::util::MessageToJsonString(card, &json, opts).ok()) {
            continue;
        }
        if (!first) {
            oss << ',';
        }
        first = false;
        oss << json;
    }
    oss << "]}";
    return WriteManifest("catalog_snapshot", "catalog.json", oss.str());
}

bool ExportPublisher::PublishVisibility(const std::vector<std::string>& visible_card_ids) {
    std::ostringstream oss;
    oss << "{\"visible_ids\":[";
    for (size_t i = 0; i < visible_card_ids.size(); ++i) {
        if (i > 0) {
            oss << ',';
        }
        oss << "\"" << visible_card_ids[i] << "\"";
    }
    oss << "]}";
    return WriteManifest("visibility_index", "visibility.json", oss.str());
}

bool ExportPublisher::PublishAffiliateSpec() {
    const std::string body =
        R"({"partners":[{"partner_id":"tmall","channel_code":"tmall","template":"https://go.simpleliving.com/r"}]})";
    return WriteManifest("affiliate_link_spec", "affiliate_spec.json", body);
}

}  // namespace backoffice_backend
}  // namespace simple_living
