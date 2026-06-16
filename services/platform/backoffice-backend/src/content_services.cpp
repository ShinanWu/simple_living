// platform/backoffice-backend brpc server — authoritative state in MySQL/PostgreSQL + Redis per services/README.md.
#include <gflags/gflags.h>
#include <brpc/controller.h>
#include <brpc/server.h>
#include <butil/logging.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "content_service.pb.h"
#include "dev_content_seeds.h"
#include "pg_content_store.h"
#include "snapshot_export.h"


namespace simple_living {
namespace content_server {

using catalog::ContentLifecycleStatus;
using catalog::EditorialContent;
using catalog::GuideCard;
using catalog::GuideCardType;
using catalog::LifeTheme;
using catalog::MediaType;
using catalog::RankingList;
using catalog::ThemeStatus;
using catalog::Topic;
using catalog::CONTENT_LIFECYCLE_STATUS_DRAFT;
using catalog::CONTENT_LIFECYCLE_STATUS_IN_REVIEW;
using catalog::CONTENT_LIFECYCLE_STATUS_PUBLISHED;
using catalog::CONTENT_LIFECYCLE_STATUS_UNSPECIFIED;
using catalog::GUIDE_CARD_TYPE_PHYSICAL_GOOD;
using catalog::LIFE_THEME_CLOTHING;
using catalog::LIFE_THEME_FOOD;
using catalog::LIFE_THEME_HOUSING;
using catalog::LIFE_THEME_MOBILITY;
using catalog::MEDIA_TYPE_IMAGE;
using catalog::THEME_STATUS_ACTIVE;

namespace {
static std::atomic<int64_t> g_counter{1};
std::string GenId(const std::string& prefix) {
    auto ts = std::chrono::system_clock::now().time_since_epoch().count();
    return prefix + "_" + std::to_string(ts) + "_" + std::to_string(g_counter.fetch_add(1));
}
}  // namespace

GuideCard BuildGuideCardFromSeed(const dev_seeds::PublishedGuideSeed& seed) {
    GuideCard card;
    card.set_card_id(seed.guide_card_id);
    card.set_type(GUIDE_CARD_TYPE_PHYSICAL_GOOD);
    card.set_title(seed.title);
    card.set_subtitle(seed.subtitle);
    card.add_selling_points(seed.summary);
    card.set_price_hint(seed.price_hint);
    card.add_theme_ids(seed.theme_id);
    card.set_commercial_disclosure_required(true);
    card.set_content_status(CONTENT_LIFECYCLE_STATUS_PUBLISHED);
    card.set_published_revision(1);
    card.set_revision(1);
    auto* cover = card.mutable_cover_media();
    cover->set_url(seed.cover_url);
    cover->set_type(MEDIA_TYPE_IMAGE);
    auto* aff = card.add_affiliate_refs();
    aff->set_channel("tmall");
    aff->set_external_item_id(seed.external_item_id);
    (*aff->mutable_payload())["landing_url"] = seed.landing_url;
    return card;
}

std::vector<GuideCard> BuildSeedGuideCards() {
    std::vector<GuideCard> cards;
    cards.reserve(std::size(dev_seeds::kPublishedGuides));
    for (const auto& seed : dev_seeds::kPublishedGuides) {
        cards.push_back(BuildGuideCardFromSeed(seed));
    }
    return cards;
}

class ContentServiceImpl : public ContentService {
    std::mutex mu_;
    PgContentStore guide_store_;
    std::unordered_map<std::string, EditorialContent> editorials_;
    std::unordered_map<std::string, Topic> topics_;
    std::unordered_map<std::string, RankingList> rankings_;
    backoffice_backend::SnapshotExportCoordinator* export_coord_{nullptr};

public:
    bool Init(const std::string& pg_conninfo) {
        if (!guide_store_.ConnectAndInit(pg_conninfo)) {
            return false;
        }
        return guide_store_.EnsureSeedGuideCards(BuildSeedGuideCards());
    }

    void Close() {
        guide_store_.Close();
    }

    void AttachExportCoordinator(backoffice_backend::SnapshotExportCoordinator* coord) {
        export_coord_ = coord;
    }

    std::vector<GuideCard> ListPublishedGuideCards() {
        std::vector<GuideCard> cards;
        if (!guide_store_.ListPublishedGuideCards(&cards) || cards.empty()) {
            return BuildSeedGuideCards();
        }
        return cards;
    }

    void RefreshSnapshotExport() {
        if (export_coord_) {
            export_coord_->RefreshNow();
        }
    }

    void ListThemes(::google::protobuf::RpcController*,
                    const ListThemesRequest*,
                    ListThemesResponse* resp,
                    ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        const char* names[] = {"衣", "食", "住", "行"};
        LifeTheme themes[] = {LIFE_THEME_CLOTHING, LIFE_THEME_FOOD, LIFE_THEME_HOUSING, LIFE_THEME_MOBILITY};
        for (int i = 0; i < 4; ++i) {
            auto* t = resp->add_themes();
            t->set_theme_id("theme_" + std::to_string(i + 1));
            t->set_display_name(names[i]);
            t->set_life_theme(themes[i]);
            t->set_status(THEME_STATUS_ACTIVE);
        }
    }

    void GetThemeDetail(::google::protobuf::RpcController*,
                        const GetThemeDetailRequest* req,
                        GetThemeDetailResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        resp->mutable_theme()->set_theme_id(req->theme_id());
        resp->mutable_theme()->set_display_name("Theme " + req->theme_id());
        resp->mutable_theme()->set_status(THEME_STATUS_ACTIVE);
    }

    void BatchGetGuideCards(::google::protobuf::RpcController*,
                            const BatchGetGuideCardsRequest* req,
                            BatchGetGuideCardsResponse* resp,
                            ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::vector<std::string> ids;
        for (const auto& id : req->card_ids()) {
            ids.push_back(id);
        }
        std::vector<GuideCard> cards;
        std::vector<std::string> missing_ids;
        if (!guide_store_.BatchGetGuideCards(ids, &cards, &missing_ids)) {
            return;
        }
        for (const auto& card : cards) {
            *resp->add_cards() = card;
        }
        for (const auto& id : missing_ids) {
            resp->add_missing_ids(id);
        }
    }

    void ListGuideCards(::google::protobuf::RpcController*,
                        const ListGuideCardsRequest* req,
                        ListGuideCardsResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        int limit = (req->limit() > 0 && req->limit() <= 50) ? req->limit() : 20;
        std::vector<GuideCard> cards;
        bool has_more = false;
        if (!guide_store_.ListGuideCards(req->theme_id(), limit, &cards, &has_more)) {
            return;
        }
        for (const auto& card : cards) {
            auto* s = resp->add_cards();
            s->set_card_id(card.card_id());
            s->set_title(card.title());
            s->set_subtitle(card.subtitle());
            if (card.has_cover_media()) {
                *s->mutable_cover_media() = card.cover_media();
            }
            for (const auto& theme_id : card.theme_ids()) {
                s->add_theme_ids(theme_id);
            }
            s->set_price_hint(card.price_hint());
            s->set_content_status(card.content_status());
            s->set_commercial_disclosure_required(card.commercial_disclosure_required());
        }
        resp->mutable_pagination()->set_has_more(has_more);
        resp->mutable_pagination()->set_limit(limit);
    }

    void GetEditorialContent(::google::protobuf::RpcController*,
                             const GetEditorialContentRequest* req,
                             GetEditorialContentResponse* resp,
                             ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        auto it = editorials_.find(req->content_id());
        if (it != editorials_.end()) *resp->mutable_content() = it->second;
    }

    void ListEditorialContents(::google::protobuf::RpcController*,
                               const ListEditorialContentsRequest*,
                               ListEditorialContentsResponse* resp,
                               ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& [id, ec] : editorials_) {
            auto* item = resp->add_items();
            item->set_content_id(ec.content_id());
            item->set_title(ec.title());
        }
    }

    void GetTopic(::google::protobuf::RpcController*,
                  const GetTopicRequest* req,
                  GetTopicResponse* resp,
                  ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        auto it = topics_.find(req->topic_id());
        if (it != topics_.end()) *resp->mutable_topic() = it->second;
    }

    void ListTopics(::google::protobuf::RpcController*,
                    const ListTopicsRequest*,
                    ListTopicsResponse* resp,
                    ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& [id, t] : topics_) {
            auto* s = resp->add_topics();
            s->set_topic_id(t.topic_id());
            s->set_title(t.title());
        }
    }

    void GetRankingList(::google::protobuf::RpcController*,
                        const GetRankingListRequest* req,
                        GetRankingListResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        auto it = rankings_.find(req->ranking_id());
        if (it != rankings_.end()) *resp->mutable_ranking() = it->second;
    }

    void ListRankingLists(::google::protobuf::RpcController*,
                          const ListRankingListsRequest*,
                          ListRankingListsResponse* resp,
                          ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& [id, r] : rankings_) {
            auto* s = resp->add_rankings();
            s->set_ranking_id(r.ranking_id());
            s->set_title(r.title());
        }
    }

    void UpsertGuideCard(::google::protobuf::RpcController* cntl_base,
                         const UpsertGuideCardRequest* req,
                         UpsertGuideCardResponse* resp,
                         ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* cntl = static_cast<brpc::Controller*>(cntl_base);
        if (!req->has_guide_card()) {
            cntl->SetFailed(brpc::EREQUEST, "guide_card is required");
            return;
        }
        GuideCard card = req->guide_card();
        if (card.card_id().empty()) card.set_card_id(GenId("card"));
        int64_t rev = card.revision() + 1;
        card.set_revision(rev);
        if (card.content_status() == CONTENT_LIFECYCLE_STATUS_UNSPECIFIED) {
            card.set_content_status(CONTENT_LIFECYCLE_STATUS_DRAFT);
        }
        if (!guide_store_.UpsertGuideCard(&card)) {
            cntl->SetFailed(brpc::EREQUEST, "landing_url already exists or failed to persist guide card");
            return;
        }
        resp->set_card_id(card.card_id());
        resp->set_revision(rev);
        if (card.content_status() == CONTENT_LIFECYCLE_STATUS_PUBLISHED) {
            RefreshSnapshotExport();
        }
    }

    void UpsertEditorialContent(::google::protobuf::RpcController*,
                                const UpsertEditorialContentRequest* req,
                                UpsertEditorialContentResponse* resp,
                                ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        if (!req->has_content()) return;
        std::lock_guard<std::mutex> lock(mu_);
        EditorialContent ec = req->content();
        if (ec.content_id().empty()) ec.set_content_id(GenId("ec"));
        int64_t rev = ec.revision() + 1;
        ec.set_revision(rev);
        editorials_[ec.content_id()] = ec;
        resp->set_content_id(ec.content_id());
        resp->set_revision(rev);
    }

    void UpsertTopic(::google::protobuf::RpcController*,
                     const UpsertTopicRequest* req,
                     UpsertTopicResponse* resp,
                     ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        if (!req->has_topic()) return;
        std::lock_guard<std::mutex> lock(mu_);
        Topic t = req->topic();
        if (t.topic_id().empty()) t.set_topic_id(GenId("topic"));
        int64_t rev = t.revision() + 1;
        t.set_revision(rev);
        topics_[t.topic_id()] = t;
        resp->set_topic_id(t.topic_id());
        resp->set_revision(rev);
    }

    void UpsertRankingList(::google::protobuf::RpcController*,
                           const UpsertRankingListRequest* req,
                           UpsertRankingListResponse* resp,
                           ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        if (!req->has_ranking()) return;
        std::lock_guard<std::mutex> lock(mu_);
        RankingList r = req->ranking();
        if (r.ranking_id().empty()) r.set_ranking_id(GenId("rank"));
        int64_t rev = r.revision() + 1;
        r.set_revision(rev);
        rankings_[r.ranking_id()] = r;
        resp->set_ranking_id(r.ranking_id());
        resp->set_revision(rev);
    }

    void SubmitForReview(::google::protobuf::RpcController*,
                         const SubmitForReviewRequest* req,
                         SubmitForReviewResponse* resp,
                         ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        GuideCard updated;
        guide_store_.UpdateGuideCardStatus(req->resource_id(),
                                           CONTENT_LIFECYCLE_STATUS_IN_REVIEW,
                                           0,
                                           &updated);
        resp->set_content_status(CONTENT_LIFECYCLE_STATUS_IN_REVIEW);
        resp->set_governance_queue_item_id(GenId("qi"));
    }

    void PublishRevision(::google::protobuf::RpcController* controller,
                         const PublishRevisionRequest* req,
                         PublishRevisionResponse* resp,
                         ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* cntl = static_cast<brpc::Controller*>(controller);
        if (req->resource_id().empty()) {
            if (cntl) {
                cntl->SetFailed("resource_id required");
            }
            return;
        }
        GuideCard updated;
        const int64_t requested_revision = req->revision();
        if (!guide_store_.UpdateGuideCardStatus(req->resource_id(),
                                                CONTENT_LIFECYCLE_STATUS_PUBLISHED,
                                                requested_revision,
                                                &updated)) {
            if (cntl) {
                cntl->SetFailed("publish failed");
            }
            return;
        }
        resp->set_content_status(CONTENT_LIFECYCLE_STATUS_PUBLISHED);
        resp->set_published_revision(requested_revision > 0 ? requested_revision : updated.published_revision());
        RefreshSnapshotExport();
    }

    void ListGuideCardRevisions(::google::protobuf::RpcController*,
                                const ListGuideCardRevisionsRequest* req,
                                ListGuideCardRevisionsResponse* resp,
                                ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        if (!req->has_card_id() || req->card_id().empty()) {
            return;
        }
        std::vector<GuideCardRevisionMeta> revisions;
        if (!guide_store_.ListGuideCardRevisions(req->card_id(), &revisions)) {
            return;
        }
        for (const auto& rev : revisions) {
            auto* out = resp->add_revisions();
            out->set_revision(rev.revision);
            out->set_change_summary(rev.change_summary);
            out->set_created_by(rev.created_by);
        }
    }

    void RollbackGuideCardRevision(::google::protobuf::RpcController*,
                                   const RollbackGuideCardRevisionRequest* req,
                                   RollbackGuideCardRevisionResponse* resp,
                                   ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        if (!req->has_card_id() || req->card_id().empty() || req->target_revision() <= 0) {
            return;
        }
        GuideCard updated;
        if (!guide_store_.RollbackGuideCardRevision(req->card_id(), req->target_revision(), &updated)) {
            return;
        }
        resp->set_card_id(updated.card_id());
        resp->set_new_revision(updated.revision());
        if (updated.content_status() == CONTENT_LIFECYCLE_STATUS_PUBLISHED) {
            RefreshSnapshotExport();
        }
    }
};

}  // namespace content_server

namespace backoffice_backend {

bool RegisterContentModule(brpc::Server* server,
                           const std::string& pg_conninfo,
                           content_server::ContentServiceImpl** out_svc,
                           SnapshotExportCoordinator* export_coord) {
    auto* svc = new content_server::ContentServiceImpl();
    if (!svc->Init(pg_conninfo)) {
        delete svc;
        return false;
    }
    if (export_coord) {
        svc->AttachExportCoordinator(export_coord);
    }
    if (server->AddService(svc, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        svc->Close();
        delete svc;
        return false;
    }
    *out_svc = svc;
    return true;
}

void ShutdownContentModule(content_server::ContentServiceImpl* svc) {
    if (svc) {
        svc->Close();
        delete svc;
    }
}

std::vector<catalog::GuideCard> CollectPublishedCards(
    content_server::ContentServiceImpl* svc) {
    if (!svc) {
        return content_server::BuildSeedGuideCards();
    }
    return svc->ListPublishedGuideCards();
}

}  // namespace backoffice_backend
}  // namespace simple_living


