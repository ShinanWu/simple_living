#include <brpc/server.h>
#include <butil/logging.h>

#include "content_service.pb.h"
#include "snapshot_store.h"

namespace simple_living {
namespace recommendation_server {

namespace {

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
        auto* theme = resp->add_themes();
        theme->set_theme_id("theme_clothing");
        theme->set_slug("clothing");
        theme->set_life_theme(catalog::LIFE_THEME_CLOTHING);
        theme->set_display_name("衣");
    }

    void ListGuideCards(::google::protobuf::RpcController*,
                        const content_server::ListGuideCardsRequest* req,
                        content_server::ListGuideCardsResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->ReloadIfChanged();
        std::vector<catalog::GuideCard> cards;
        store_->ListCards(&cards);
        const int limit = req->has_limit() && req->limit() > 0 ? req->limit() : 50;
        int count = 0;
        for (const auto& card : cards) {
            if (count >= limit) {
                break;
            }
            CardToSummary(card, resp->add_cards());
            ++count;
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
