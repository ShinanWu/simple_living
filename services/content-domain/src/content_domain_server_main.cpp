// content-domain brpc server — authoritative state in MySQL/PostgreSQL + Redis per docs/architecture.
#include <gflags/gflags.h>
#include <brpc/server.h>
#include <butil/logging.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

#include "content_domain.pb.h"

DEFINE_int32(port, 9102, "TCP port for this brpc server");

namespace simple_living {
namespace content_domain {

namespace {
static std::atomic<int64_t> g_counter{1};
std::string GenId(const std::string& prefix) {
    auto ts = std::chrono::system_clock::now().time_since_epoch().count();
    return prefix + "_" + std::to_string(ts) + "_" + std::to_string(g_counter.fetch_add(1));
}
void InitSeedData(std::unordered_map<std::string, GuideCard>& cards) {
    for (int i = 1; i <= 5; ++i) {
        GuideCard c;
        std::string id = "card_seed_" + std::to_string(i);
        c.set_card_id(id);
        c.set_type(GUIDE_CARD_TYPE_PHYSICAL_GOOD);
        c.set_title("Sample Guide " + std::to_string(i));
        c.set_subtitle("A curated simple living item");
        c.set_price_hint("¥" + std::to_string(i * 100));
        c.set_content_status(CONTENT_LIFECYCLE_STATUS_PUBLISHED);
        c.set_revision(1);
        cards[id] = c;
    }
}
}  // namespace

class ContentServiceImpl : public ContentService {
    std::mutex mu_;
    std::unordered_map<std::string, GuideCard> cards_;
    std::unordered_map<std::string, EditorialContent> editorials_;
    std::unordered_map<std::string, Topic> topics_;
    std::unordered_map<std::string, RankingList> rankings_;

public:
    ContentServiceImpl() { InitSeedData(cards_); }

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
        std::lock_guard<std::mutex> lock(mu_);
        for (const auto& id : req->card_ids()) {
            auto it = cards_.find(id);
            if (it != cards_.end()) *resp->add_cards() = it->second;
            else resp->add_missing_ids(id);
        }
    }

    void ListGuideCards(::google::protobuf::RpcController*,
                        const ListGuideCardsRequest* req,
                        ListGuideCardsResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        int limit = (req->limit() > 0 && req->limit() <= 50) ? req->limit() : 20;
        int count = 0;
        for (auto& [id, card] : cards_) {
            if (count >= limit) { resp->mutable_pagination()->set_has_more(true); break; }
            auto* s = resp->add_cards();
            s->set_card_id(card.card_id());
            s->set_title(card.title());
            s->set_subtitle(card.subtitle());
            s->set_content_status(card.content_status());
            ++count;
        }
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

    void UpsertGuideCard(::google::protobuf::RpcController*,
                         const UpsertGuideCardRequest* req,
                         UpsertGuideCardResponse* resp,
                         ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        if (!req->has_guide_card()) return;
        std::lock_guard<std::mutex> lock(mu_);
        GuideCard card = req->guide_card();
        if (card.card_id().empty()) card.set_card_id(GenId("card"));
        int64_t rev = card.revision() + 1;
        card.set_revision(rev);
        cards_[card.card_id()] = card;
        resp->set_card_id(card.card_id());
        resp->set_revision(rev);
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
                         const SubmitForReviewRequest*,
                         SubmitForReviewResponse* resp,
                         ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        resp->set_content_status(CONTENT_LIFECYCLE_STATUS_IN_REVIEW);
        resp->set_governance_queue_item_id(GenId("qi"));
    }

    void PublishRevision(::google::protobuf::RpcController*,
                         const PublishRevisionRequest* req,
                         PublishRevisionResponse* resp,
                         ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        auto it = cards_.find(req->resource_id());
        if (it != cards_.end()) {
            it->second.set_content_status(CONTENT_LIFECYCLE_STATUS_PUBLISHED);
            it->second.set_published_revision(req->revision());
        }
        resp->set_content_status(CONTENT_LIFECYCLE_STATUS_PUBLISHED);
        resp->set_published_revision(req->revision());
    }
};

}  // namespace content_domain
}  // namespace simple_living

int main(int argc, char* argv[]) {
    GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);
    brpc::Server server;
    simple_living::content_domain::ContentServiceImpl g_svc;
    if (server.AddService(&g_svc, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add service";
        return 1;
    }
    brpc::ServerOptions options;
    if (server.Start(FLAGS_port, &options) != 0) {
        LOG(ERROR) << "Fail to start server";
        return 1;
    }
    LOG(INFO) << "content-domain server listening on port " << FLAGS_port;
    server.RunUntilAskedToQuit();
    return 0;
}
