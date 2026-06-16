#include "gateway_home_feed_helpers.h"

#include "gateway_backoffice_media.h"

namespace simple_living {
namespace gateway {
namespace pages {
namespace home_feed {

namespace {

std::string JoinParts(const std::vector<std::string>& parts, const std::string& sep) {
    std::string out;
    for (const auto& part : parts) {
        const std::string trimmed = TrimCopy(part);
        if (trimmed.empty()) {
            continue;
        }
        if (!out.empty()) {
            out += sep;
        }
        out += trimmed;
    }
    return out;
}

std::string JoinSellingPoints(const simple_living::catalog::GuideCard& card, size_t max_count) {
    std::vector<std::string> points;
    points.reserve(static_cast<size_t>(card.selling_points_size()));
    for (const auto& point : card.selling_points()) {
        const std::string trimmed = TrimCopy(point);
        if (trimmed.empty()) {
            continue;
        }
        points.push_back(trimmed);
        if (points.size() >= max_count) {
            break;
        }
    }
    return JoinParts(points, " / ");
}

}  // namespace

std::string TrimCopy(const std::string& raw) {
    const auto begin = raw.find_first_not_of(" \t\n\r");
    if (begin == std::string::npos) {
        return "";
    }
    const auto end = raw.find_last_not_of(" \t\n\r");
    return raw.substr(begin, end - begin + 1);
}

void HydrateGuideCardSummary(GuideCardSummary* summary,
                             const simple_living::catalog::GuideCard& card,
                             const std::string& theme) {
    summary->set_guide_card_id(card.card_id());
    summary->set_title(card.title());
    summary->set_subtitle(card.subtitle());
    if (!card.selling_points().empty()) {
        summary->set_summary(card.selling_points(0));
    } else if (!card.subtitle().empty()) {
        summary->set_summary(card.subtitle());
    }
    if (card.has_cover_media() && !card.cover_media().url().empty()) {
        summary->set_cover_url(
            backoffice_media::NormalizeMediaUrlForClient(card.cover_media().url()));
    }
    summary->set_theme(theme);
}

std::string BuildReasonText(const simple_living::catalog::GuideCard& card,
                            const std::vector<std::string>& recommendation_reason_tags,
                            const std::string& explanation_summary) {
    const std::string summary = TrimCopy(explanation_summary);
    if (!summary.empty()) {
        return summary;
    }

    const std::string tags = JoinParts(recommendation_reason_tags, " / ");
    if (!tags.empty()) {
        return tags;
    }

    const std::string selling_points = JoinSellingPoints(card, 2);
    if (!selling_points.empty()) {
        return selling_points;
    }

    return TrimCopy(card.subtitle());
}

void FillHomeFeedItemFromCard(HomeFeedItem* item,
                              const simple_living::catalog::GuideCard& card,
                              const std::string& theme,
                              const std::string& recommendation_id,
                              const std::string& scene,
                              int rank,
                              const std::vector<std::string>& recommendation_reason_tags,
                              const std::string& explanation_summary) {
    item->set_recommendation_id(recommendation_id);
    item->set_scene(scene);
    item->set_rank(rank);
    item->set_guide_card_id(card.card_id());
    HydrateGuideCardSummary(item->mutable_guide_card(), card, theme);
    item->set_reason_text(BuildReasonText(card, recommendation_reason_tags, explanation_summary));
    item->clear_reason_tags();
    for (const auto& tag : recommendation_reason_tags) {
        const std::string trimmed = TrimCopy(tag);
        if (!trimmed.empty()) {
            item->add_reason_tags(trimmed);
        }
    }
}

}  // namespace home_feed
}  // namespace pages
}  // namespace gateway
}  // namespace simple_living
