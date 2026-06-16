#include <brpc/server.h>
#include <butil/logging.h>

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "content_service.pb.h"
#include "snapshot_store.h"

namespace simple_living {
namespace recommendation_server {

namespace {

bool CardHasThemeId(const catalog::GuideCard& card, const std::string& theme_id) {
    for (const auto& tid : card.theme_ids()) {
        if (tid == theme_id) {
            return true;
        }
    }
    return false;
}

void CardToSummary(const catalog::GuideCard& card, catalog::GuideCardSummary* out) {
    out->set_card_id(card.card_id());
    out->set_type(card.type());
    out->set_title(card.title());
    out->set_subtitle(card.subtitle());
    if (card.has_cover_media()) {
        *out->mutable_cover_media() = card.cover_media();
    }
    for (const auto& tid : card.theme_ids()) {
        out->add_theme_ids(tid);
    }
    out->set_price_hint(card.price_hint());
    out->set_content_status(card.content_status());
    out->set_commercial_disclosure_required(card.commercial_disclosure_required());
}

}  // namespace

class CatalogReadServiceImpl : public content_server::ContentService {
    SnapshotStore* store_;

public:
    explicit CatalogReadServiceImpl(SnapshotStore* store) : store_(store) {}

    void ListThemes(::google::protobuf::RpcController*,
                    const content_server::ListThemesRequest*,
                    content_server::ListThemesResponse* resp,
                    ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->ReloadIfChanged();
        struct ThemeDef {
            const char* theme_id;
            const char* slug;
            catalog::LifeTheme life_theme;
            const char* display_name;
        };
        static const ThemeDef kThemes[] = {
            {"theme_1", "clothing", catalog::LIFE_THEME_CLOTHING, "衣"},
            {"theme_2", "food", catalog::LIFE_THEME_FOOD, "食"},
            {"theme_3", "housing", catalog::LIFE_THEME_HOUSING, "住"},
            {"theme_4", "transport", catalog::LIFE_THEME_MOBILITY, "行"},
        };
        for (const auto& t : kThemes) {
            auto* theme = resp->add_themes();
            theme->set_theme_id(t.theme_id);
            theme->set_slug(t.slug);
            theme->set_life_theme(t.life_theme);
            theme->set_display_name(t.display_name);
        }
    }

    void ListGuideCards(::google::protobuf::RpcController*,
                        const content_server::ListGuideCardsRequest* req,
                        content_server::ListGuideCardsResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->ReloadIfChanged();
        std::vector<catalog::GuideCard> cards;
        store_->ListCards(&cards);
        const bool filter_by_theme = req->has_theme_id() && !req->theme_id().empty();
        const std::string& theme_filter = req->theme_id();
        int limit = req->has_limit() && req->limit() > 0 ? req->limit() : 50;
        if (limit > 100) {
            limit = 100;
        }
        int64_t offset = 0;
        if (req->has_cursor() && !req->cursor().empty()) {
            offset = std::strtoll(req->cursor().c_str(), nullptr, 10);
            if (offset < 0) {
                offset = 0;
            }
        }
        int64_t matched = 0;
        int emitted = 0;
        bool has_more = false;
        for (const auto& card : cards) {
            if (filter_by_theme && !CardHasThemeId(card, theme_filter)) {
                continue;
            }
            if (matched < offset) {
                ++matched;
                continue;
            }
            if (emitted >= limit) {
                has_more = true;
                break;
            }
            CardToSummary(card, resp->add_cards());
            ++emitted;
            ++matched;
        }
        auto* pg = resp->mutable_pagination();
        pg->set_limit(limit);
        pg->set_has_more(has_more);
        if (has_more) {
            pg->set_next_cursor(std::to_string(offset + emitted));
        }
    }

    void BatchGetGuideCards(::google::protobuf::RpcController*,
                            const content_server::BatchGetGuideCardsRequest* req,
                            content_server::BatchGetGuideCardsResponse* resp,
                            ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->ReloadIfChanged();
        for (const auto& id : req->card_ids()) {
            catalog::GuideCard card;
            if (store_->GetCard(id, &card)) {
                *resp->add_cards() = card;
            }
        }
    }

    void UpsertGuideCard(::google::protobuf::RpcController* cntl,
                         const content_server::UpsertGuideCardRequest*,
                         content_server::UpsertGuideCardResponse*,
                         ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        cntl->SetFailed("read-only: use platform/backoffice-backend");
    }

    void SubmitForReview(::google::protobuf::RpcController* cntl,
                         const content_server::SubmitForReviewRequest*,
                         content_server::SubmitForReviewResponse*,
                         ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        cntl->SetFailed("read-only: use platform/backoffice-backend");
    }

    void PublishRevision(::google::protobuf::RpcController* cntl,
                         const content_server::PublishRevisionRequest*,
                         content_server::PublishRevisionResponse*,
                         ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        cntl->SetFailed("read-only: use platform/backoffice-backend");
    }
};

CatalogReadServiceImpl* NewCatalogReadService(SnapshotStore* store) {
    return new CatalogReadServiceImpl(store);
}

bool RegisterCatalogReadService(brpc::Server* server, CatalogReadServiceImpl* impl) {
    return server->AddService(impl, brpc::SERVER_DOESNT_OWN_SERVICE) == 0;
}

}  // namespace recommendation_server
}  // namespace simple_living
