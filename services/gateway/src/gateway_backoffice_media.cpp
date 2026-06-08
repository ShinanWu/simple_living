#include "gateway_backoffice_media.h"

#include <gflags/gflags.h>

#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

namespace simple_living {
namespace gateway {
namespace pages {
namespace backoffice_media {

DEFINE_string(gateway_backoffice_media_dir,
              "/var/lib/simple-living/media/backoffice",
              "Directory for backoffice uploaded media assets");

DEFINE_string(gateway_backoffice_media_public_base_url,
              "",
              "Public origin for backoffice media URLs (e.g. https://cdn.example.com). "
              "Required for media upload; returned url is absolute http(s).");

namespace {

std::string TrimTrailingSlash(std::string s) {
    while (!s.empty() && s.back() == '/') {
        s.pop_back();
    }
    return s;
}

std::string MediaPublicBaseUrl() {
    return TrimTrailingSlash(FLAGS_gateway_backoffice_media_public_base_url);
}

std::string BuildPublicMediaUrlFromPath(const std::string& media_path) {
    const std::string base = MediaPublicBaseUrl();
    if (base.empty()) {
        return media_path;
    }
    if (media_path.empty()) {
        return media_path;
    }
    if (media_path.rfind("http://", 0) == 0 || media_path.rfind("https://", 0) == 0) {
        return media_path;
    }
    if (media_path.front() != '/') {
        return base + "/" + media_path;
    }
    return base + media_path;
}

bool IsMediaPath(const std::string& url) {
    return url.rfind("/media/backoffice/", 0) == 0;
}

}  // namespace

std::string GenAssetId() {
    const auto now = std::chrono::system_clock::now().time_since_epoch().count();
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(100000, 999999);
    return "media_" + std::to_string(now) + "_" + std::to_string(dist(rng));
}

std::string ExtensionForContentType(const std::string& content_type) {
    if (content_type.find("png") != std::string::npos) {
        return "png";
    }
    if (content_type.find("webp") != std::string::npos) {
        return "webp";
    }
    if (content_type.find("gif") != std::string::npos) {
        return "gif";
    }
    return "jpg";
}

std::string ContentTypeForExtension(const std::string& ext) {
    if (ext == "png") {
        return "image/png";
    }
    if (ext == "webp") {
        return "image/webp";
    }
    if (ext == "gif") {
        return "image/gif";
    }
    return "image/jpeg";
}

bool Base64Decode(const std::string& input, std::string* out) {
    static const int8_t kDec[256] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62,
        -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, -1, 0,
        1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
        23, 24, 25, -1, -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
        39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
    };

    out->clear();
    int val = 0;
    int valb = -8;
    for (unsigned char c : input) {
        if (std::isspace(c)) {
            continue;
        }
        if (c == '=') {
            break;
        }
        const int8_t d = kDec[c];
        if (d < 0) {
            return false;
        }
        val = (val << 6) + d;
        valb += 6;
        if (valb >= 0) {
            out->push_back(static_cast<char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return !out->empty();
}

bool EnsureMediaDir() {
    std::error_code ec;
    std::filesystem::create_directories(FLAGS_gateway_backoffice_media_dir, ec);
    return !ec;
}

bool DecodeUploadPayload(const BackofficeMediaUploadRequest& req,
                         std::string* bytes,
                         std::string* content_type) {
    if (req.has_data_url() && !req.data_url().empty()) {
        const std::string& data_url = req.data_url();
        const auto comma = data_url.find(',');
        if (comma == std::string::npos) {
            return false;
        }
        const std::string header = data_url.substr(0, comma);
        if (header.find("image/png") != std::string::npos) {
            *content_type = "image/png";
        } else if (header.find("image/webp") != std::string::npos) {
            *content_type = "image/webp";
        } else if (header.find("image/gif") != std::string::npos) {
            *content_type = "image/gif";
        } else {
            *content_type = "image/jpeg";
        }
        return Base64Decode(data_url.substr(comma + 1), bytes);
    }
    if (req.has_content_base64() && !req.content_base64().empty()) {
        *content_type = req.has_content_type() && !req.content_type().empty() ? req.content_type()
                                                                              : "image/jpeg";
        return Base64Decode(req.content_base64(), bytes);
    }
    return false;
}

bool IsSafeFilename(const std::string& filename) {
    if (filename.empty() || filename.size() > 200) {
        return false;
    }
    for (char c : filename) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '-')) {
            return false;
        }
    }
    return filename.find("..") == std::string::npos;
}

bool IsPersistableMediaUrl(const std::string& url) {
    if (url.empty() || url.rfind("data:", 0) == 0) {
        return false;
    }
    if (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) {
        return true;
    }
    return IsMediaPath(url) && !MediaPublicBaseUrl().empty();
}

std::string NormalizeMediaUrlForClient(const std::string& url) {
    if (url.empty()) {
        return url;
    }
    if (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) {
        return url;
    }
    if (IsMediaPath(url)) {
        return BuildPublicMediaUrlFromPath(url);
    }
    return url;
}

std::string NormalizeMediaUrlForStore(const std::string& url) {
    if (url.empty()) {
        return url;
    }
    if (url.rfind("data:", 0) == 0) {
        return url;
    }
    if (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) {
        return url;
    }
    if (IsMediaPath(url)) {
        return BuildPublicMediaUrlFromPath(url);
    }
    return url;
}

bool SaveBackofficeMediaUpload(const BackofficeMediaUploadRequest& req,
                               BackofficeMediaUploadResponse* resp) {
    std::string bytes;
    std::string content_type;
    if (!DecodeUploadPayload(req, &bytes, &content_type)) {
        return false;
    }
    if (bytes.size() > 5 * 1024 * 1024) {
        return false;
    }
    if (!EnsureMediaDir()) {
        return false;
    }
    if (MediaPublicBaseUrl().empty()) {
        return false;
    }

    const std::string asset_id = GenAssetId();
    const std::string ext = ExtensionForContentType(content_type);
    const std::string filename = asset_id + "." + ext;
    const std::filesystem::path path =
        std::filesystem::path(FLAGS_gateway_backoffice_media_dir) / filename;

    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out.is_open() || !out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
            return false;
        }
    }

    resp->set_asset_id(asset_id);
    resp->set_url(BuildPublicMediaUrlFromPath("/media/backoffice/" + filename));
    resp->set_content_type(content_type);
    return true;
}

bool ServeBackofficeMediaAsset(const std::string& filename, brpc::Controller* outer) {
    if (!IsSafeFilename(filename)) {
        return false;
    }
    const std::filesystem::path path =
        std::filesystem::path(FLAGS_gateway_backoffice_media_dir) / filename;
    if (!std::filesystem::is_regular_file(path)) {
        return false;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    std::ostringstream oss;
    oss << in.rdbuf();
    const std::string body = oss.str();

    const std::string ext = path.extension().string().substr(1);
    outer->http_response().set_status_code(brpc::HTTP_STATUS_OK);
    outer->http_response().set_content_type(ContentTypeForExtension(ext));
    outer->http_response().SetHeader("Cache-Control", "public, max-age=31536000, immutable");
    outer->http_response().SetHeader("Access-Control-Allow-Origin", "*");
    outer->response_attachment().append(body);
    return true;
}

}  // namespace backoffice_media
}  // namespace pages
}  // namespace gateway
}  // namespace simple_living
