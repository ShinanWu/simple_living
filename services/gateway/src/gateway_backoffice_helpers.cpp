#include "gateway_backoffice_helpers.h"

#include <gflags/gflags.h>

#include <chrono>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace simple_living {
namespace gateway {
namespace {

void WriteAuthError(brpc::Controller* outer, int code, const std::string& msg) {
    outer->http_response().set_status_code(brpc::HTTP_STATUS_UNAUTHORIZED);
    outer->http_response().set_content_type("application/json");
    outer->http_response().SetHeader("Access-Control-Allow-Origin", "*");
    outer->response_attachment().append("{\"success\":false,\"code\":" + std::to_string(code) +
                                        ",\"message\":\"" + msg + "\",\"data\":null}");
}

}  // namespace

namespace pages {
namespace backoffice_helpers {

DEFINE_string(backoffice_api_token,
              "simple-living-ops",
              "Bearer token required for /api/v2/backoffice/* (empty disables auth)");

bool EnsureBackofficeAuth(brpc::Controller* outer, std::string* role_out) {
    if (FLAGS_backoffice_api_token.empty()) {
        if (role_out) {
            *role_out = "backoffice_admin";
        }
        return true;
    }
    const std::string* auth = outer->http_request().GetHeader("Authorization");
    static constexpr char kBearer[] = "Bearer ";
    if (auth == nullptr || auth->size() <= sizeof(kBearer) - 1 ||
        auth->compare(0, sizeof(kBearer) - 1, kBearer) != 0) {
        WriteAuthError(outer, 20004, "Unauthorized");
        return false;
    }
    const std::string token = auth->substr(sizeof(kBearer) - 1);
    if (token != FLAGS_backoffice_api_token) {
        WriteAuthError(outer, 20004, "Unauthorized");
        return false;
    }
    if (role_out) {
        const std::string* role = outer->http_request().GetHeader("X-Backoffice-Role");
        *role_out = (role != nullptr && !role->empty()) ? *role : "backoffice_admin";
    }
    return true;
}

bool ValidateBackofficeLoginToken(const std::string& access_token, std::string* session_token_out) {
    if (access_token.empty()) {
        return false;
    }
    const std::string expected = FLAGS_backoffice_api_token.empty() ? "simple-living-ops"
                                                                  : FLAGS_backoffice_api_token;
    if (access_token != expected) {
        return false;
    }
    if (session_token_out) {
        *session_token_out = expected;
    }
    return true;
}

std::string ThemeIdFromTheme(const std::string& theme) {
    if (theme == "clothing" || theme.empty()) return "theme_1";
    if (theme == "food") return "theme_2";
    if (theme == "housing") return "theme_3";
    if (theme == "transport") return "theme_4";
    return theme;
}

std::string ThemeFromThemeId(const std::string& theme_id) {
    if (theme_id == "theme_1") return "clothing";
    if (theme_id == "theme_2") return "food";
    if (theme_id == "theme_3") return "housing";
    if (theme_id == "theme_4") return "transport";
    return theme_id;
}

std::string SanitizeIdPart(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (unsigned char c : raw) {
        if (std::isalnum(c)) {
            out.push_back(static_cast<char>(std::tolower(c)));
        } else if (c == '_' || c == '-') {
            out.push_back('_');
        }
    }
    return out.empty() ? "item" : out;
}

std::string StatusFromContentStatus(catalog::ContentLifecycleStatus status) {
    switch (status) {
        case catalog::CONTENT_LIFECYCLE_STATUS_IN_REVIEW:
            return "in_review";
        case catalog::CONTENT_LIFECYCLE_STATUS_PUBLISHED:
            return "published";
        case catalog::CONTENT_LIFECYCLE_STATUS_SCHEDULED:
            return "scheduled";
        case catalog::CONTENT_LIFECYCLE_STATUS_OFFLINE:
            return "offline";
        case catalog::CONTENT_LIFECYCLE_STATUS_ARCHIVED:
            return "archived";
        case catalog::CONTENT_LIFECYCLE_STATUS_DRAFT:
        default:
            return "draft";
    }
}

catalog::ContentLifecycleStatus ContentStatusFromString(const std::string& status) {
    if (status == "in_review") return catalog::CONTENT_LIFECYCLE_STATUS_IN_REVIEW;
    if (status == "published") return catalog::CONTENT_LIFECYCLE_STATUS_PUBLISHED;
    if (status == "scheduled") return catalog::CONTENT_LIFECYCLE_STATUS_SCHEDULED;
    if (status == "offline") return catalog::CONTENT_LIFECYCLE_STATUS_OFFLINE;
    if (status == "archived") return catalog::CONTENT_LIFECYCLE_STATUS_ARCHIVED;
    return catalog::CONTENT_LIFECYCLE_STATUS_DRAFT;
}

namespace {

std::string NowIso8601() {
    const auto now = std::chrono::system_clock::now();
    const auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf {};
    gmtime_r(&t, &tm_buf);
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

}  // namespace

void FillBackofficeContentItem(const catalog::GuideCard& card, BackofficeContentItem* out) {
    out->set_content_id(card.card_id());
    out->set_resource_kind("guide_card");
    out->set_title(card.title());
    out->set_subtitle(card.subtitle());
    out->clear_theme_ids();
    for (const auto& theme_id : card.theme_ids()) {
        out->add_theme_ids(theme_id);
        if (out->theme().empty()) {
            out->set_theme(ThemeFromThemeId(theme_id));
        }
    }
    const std::string status = StatusFromContentStatus(card.content_status());
    out->set_content_status(status);
    out->set_status(status);
    out->set_revision(card.revision());
    out->set_published_revision(card.published_revision());
    if (!card.selling_points().empty()) {
        out->set_summary(card.selling_points(0));
        out->clear_selling_points();
        for (const auto& sp : card.selling_points()) {
            out->add_selling_points(sp);
        }
    }
    if (card.has_cover_media()) {
        auto* media = out->mutable_cover_media();
        media->set_url(card.cover_media().url());
        media->set_type("image");
    }
    if (!card.affiliate_refs().empty()) {
        const auto& aff = card.affiliate_refs(0);
        out->set_external_item_id(aff.external_item_id());
        auto it = aff.payload().find("landing_url");
        if (it != aff.payload().end()) {
            out->set_landing_url(it->second);
        }
        out->clear_affiliate_refs();
        for (const auto& ref : card.affiliate_refs()) {
            auto* out_ref = out->add_affiliate_refs();
            out_ref->set_channel(ref.channel());
            out_ref->set_external_item_id(ref.external_item_id());
        }
    }
    out->set_commercial_disclosure_required(card.commercial_disclosure_required());
    out->set_created_at(NowIso8601());
    out->set_updated_at(NowIso8601());
}

catalog::GuideCard BuildGuideCardFromCreate(const BackofficeCreateContentItemRequest& req) {
    std::string theme = req.theme();
    if (theme.empty() && req.theme_ids_size() > 0) {
        theme = ThemeFromThemeId(req.theme_ids(0));
    }
    if (theme.empty()) {
        theme = "clothing";
    }
    const std::string external_id = req.external_item_id().empty()
        ? SanitizeIdPart(req.landing_url())
        : SanitizeIdPart(req.external_item_id());
    const std::string channel = req.affiliate_refs_size() > 0 && !req.affiliate_refs(0).channel().empty()
        ? req.affiliate_refs(0).channel()
        : "tmall";
    const std::string card_id = "guide_" + theme + "_" + channel + "_" + external_id;

    catalog::GuideCard card;
    card.set_card_id(card_id);
    card.set_type(catalog::GUIDE_CARD_TYPE_PHYSICAL_GOOD);
    card.set_title(req.title());
    card.set_subtitle(req.subtitle());
    if (!req.summary().empty()) {
        card.add_selling_points(req.summary());
    } else if (req.selling_points_size() > 0) {
        for (const auto& sp : req.selling_points()) {
            card.add_selling_points(sp);
        }
    } else {
        card.add_selling_points(req.title());
    }
    card.set_price_hint("以渠道实时价格为准");
    if (req.theme_ids_size() > 0) {
        for (const auto& theme_id : req.theme_ids()) {
            card.add_theme_ids(theme_id);
        }
    } else {
        card.add_theme_ids(ThemeIdFromTheme(theme));
    }
    card.set_commercial_disclosure_required(req.commercial_disclosure_required());
    const std::string initial = !req.initial_status().empty()
        ? req.initial_status()
        : (!req.status().empty() ? req.status() : "draft");
    card.set_content_status(ContentStatusFromString(initial));
    card.set_revision(1);
    card.set_published_revision(initial == "published" ? 1 : 0);
    if (req.has_cover_media() && !req.cover_media().url().empty()) {
        card.mutable_cover_media()->set_url(req.cover_media().url());
        card.mutable_cover_media()->set_type(catalog::MEDIA_TYPE_IMAGE);
    }
    auto* aff = card.add_affiliate_refs();
    aff->set_channel(channel);
    aff->set_external_item_id(external_id);
    if (!req.landing_url().empty()) {
        (*aff->mutable_payload())["landing_url"] = req.landing_url();
    }
    return card;
}

void ApplyContentUpdate(const BackofficeUpdateContentItemRequest& req, catalog::GuideCard* card) {
    if (req.has_title()) card->set_title(req.title());
    if (req.has_subtitle()) card->set_subtitle(req.subtitle());
    if (req.has_summary()) {
        if (card->selling_points_size() > 0) {
            card->set_selling_points(0, req.summary());
        } else {
            card->add_selling_points(req.summary());
        }
    }
    if (req.selling_points_size() > 0) {
        card->clear_selling_points();
        for (const auto& sp : req.selling_points()) {
            card->add_selling_points(sp);
        }
    }
    if (req.has_landing_url()) {
        if (card->affiliate_refs_size() > 0) {
            (*card->mutable_affiliate_refs(0)->mutable_payload())["landing_url"] = req.landing_url();
        }
    }
    if (req.has_external_item_id()) {
        if (card->affiliate_refs_size() > 0) {
            card->mutable_affiliate_refs(0)->set_external_item_id(req.external_item_id());
        }
    }
    if (req.has_cover_media() && !req.cover_media().url().empty()) {
        card->mutable_cover_media()->set_url(req.cover_media().url());
        card->mutable_cover_media()->set_type(catalog::MEDIA_TYPE_IMAGE);
    } else if (req.has_cover_url() && !req.cover_url().empty()) {
        card->mutable_cover_media()->set_url(req.cover_url());
        card->mutable_cover_media()->set_type(catalog::MEDIA_TYPE_IMAGE);
    }
    if (req.theme_ids_size() > 0) {
        card->clear_theme_ids();
        for (const auto& theme_id : req.theme_ids()) {
            card->add_theme_ids(theme_id);
        }
    } else if (req.has_theme()) {
        card->clear_theme_ids();
        card->add_theme_ids(ThemeIdFromTheme(req.theme()));
    }
    if (req.has_commercial_disclosure_required()) {
        card->set_commercial_disclosure_required(req.commercial_disclosure_required());
    }
}

BackofficePartner PartnerFromSnapshot(const affiliate_server::PartnerCapabilitySnapshot& snap) {
    BackofficePartner out;
    out.set_partner_id(snap.partner_id());
    out.set_display_name(snap.display_name().empty() ? snap.partner_id() : snap.display_name());
    out.set_status(PartnerLifecycleToStatus(snap.lifecycle_status()));
    out.set_primary_channel_code(
        snap.primary_channel_code().empty() ? snap.partner_id() : snap.primary_channel_code());
    return out;
}

affiliate_server::PartnerCapabilitySnapshot SnapshotFromPartnerRequest(
    const BackofficeCreatePartnerRequest& req) {
    affiliate_server::PartnerCapabilitySnapshot snap;
    snap.set_partner_id(req.partner_id());
    snap.set_display_name(req.display_name());
    snap.set_primary_channel_code(req.primary_channel_code().empty() ? req.partner_id()
                                                                     : req.primary_channel_code());
    snap.set_lifecycle_status(StatusToPartnerLifecycle(req.status()));
    snap.add_flags(affiliate_server::CAPABILITY_FLAG_DEEP_LINK);
    snap.mutable_link_constraints()->set_max_url_length(2048);
    return snap;
}

std::string PartnerLifecycleToStatus(affiliate_server::PartnerLifecycleStatus status) {
    switch (status) {
        case affiliate_server::PARTNER_LIFECYCLE_STATUS_ACTIVE:
            return "active";
        case affiliate_server::PARTNER_LIFECYCLE_STATUS_DISABLED:
            return "disabled";
        case affiliate_server::PARTNER_LIFECYCLE_STATUS_SUNSET:
            return "sunset";
        case affiliate_server::PARTNER_LIFECYCLE_STATUS_DRAFT:
        default:
            return "draft";
    }
}

affiliate_server::PartnerLifecycleStatus StatusToPartnerLifecycle(const std::string& status) {
    if (status == "active") return affiliate_server::PARTNER_LIFECYCLE_STATUS_ACTIVE;
    if (status == "disabled") return affiliate_server::PARTNER_LIFECYCLE_STATUS_DISABLED;
    if (status == "sunset") return affiliate_server::PARTNER_LIFECYCLE_STATUS_SUNSET;
    return affiliate_server::PARTNER_LIFECYCLE_STATUS_DRAFT;
}

std::string VisibilityStateToString(governance_server::VisibilityState state) {
    switch (state) {
        case governance_server::VISIBILITY_STATE_UNPUBLISHED:
            return "unpublished";
        case governance_server::VISIBILITY_STATE_RESTRICTED:
            return "restricted";
        case governance_server::VISIBILITY_STATE_PUBLISHED:
            return "published";
        default:
            return "unpublished";
    }
}

std::string VisibilitySourceToString(governance_server::VisibilityVerdictSource source) {
    switch (source) {
        case governance_server::VISIBILITY_VERDICT_SOURCE_REVIEW:
            return "review";
        case governance_server::VISIBILITY_VERDICT_SOURCE_POLICY:
            return "policy";
        case governance_server::VISIBILITY_VERDICT_SOURCE_SYSTEM:
            return "system";
        case governance_server::VISIBILITY_VERDICT_SOURCE_MANUAL_OPS:
        default:
            return "manual_ops";
    }
}

std::string ReviewQueueStatusToString(governance_server::ReviewQueueItemStatus status) {
    switch (status) {
        case governance_server::REVIEW_QUEUE_ITEM_STATUS_IN_REVIEW:
            return "in_review";
        case governance_server::REVIEW_QUEUE_ITEM_STATUS_COMPLETED:
            return "completed";
        case governance_server::REVIEW_QUEUE_ITEM_STATUS_PENDING:
        default:
            return "pending";
    }
}

governance_server::VisibilityState VisibilityStateFromString(const std::string& state) {
    if (state == "published") return governance_server::VISIBILITY_STATE_PUBLISHED;
    if (state == "restricted") return governance_server::VISIBILITY_STATE_RESTRICTED;
    return governance_server::VISIBILITY_STATE_UNPUBLISHED;
}

}  // namespace backoffice_helpers
}  // namespace pages
}  // namespace gateway
}  // namespace simple_living
