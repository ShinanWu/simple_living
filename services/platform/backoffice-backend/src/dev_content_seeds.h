#pragma once

// Lab bootstrap seeds owned by backoffice-backend. Written to PostgreSQL on first ConnectAndInit,
// then exported via snapshot bundles for recommendation-server / tracking-server.
namespace simple_living::dev_seeds {

struct PublishedGuideSeed {
    const char* guide_card_id;
    const char* theme_id;
    const char* title;
    const char* subtitle;
    const char* summary;
    const char* price_hint;
    const char* external_item_id;
    const char* landing_url;
    const char* cover_url;
};

inline constexpr const char kThemeClothing[] = "theme_1";
inline constexpr const char kThemeFood[] = "theme_2";
inline constexpr const char kThemeHousing[] = "theme_3";
inline constexpr const char kThemeTransport[] = "theme_4";

// Lab placeholder covers; replace via media/upload + content update in production.
inline constexpr const char kLabSeedCoverTheme1[] =
    "https://shaotang.top/media/backoffice/seed_cover_theme_1.jpg";
inline constexpr const char kLabSeedCoverTheme2[] =
    "https://shaotang.top/media/backoffice/seed_cover_theme_2.jpg";
inline constexpr const char kLabSeedCoverTheme3[] =
    "https://shaotang.top/media/backoffice/seed_cover_theme_3.jpg";
inline constexpr const char kLabSeedCoverTheme4[] =
    "https://shaotang.top/media/backoffice/seed_cover_theme_4.jpg";

inline constexpr PublishedGuideSeed kPublishedGuides[] = {
    {
        "guide_clothing_tmall_919142939015",
        kThemeClothing,
        "天猫精选衣物（衣·基础款）",
        "日常通勤 · 少糖为你筛过",
        "来自天猫详情页的衣类导购：点击「去购买」将跳转至天猫完成选购。含商业合作标识。",
        "以天猫实时价格为准",
        "919142939015",
        "https://detail.tmall.com/item.htm?abbucket=5&id=919142939015&mi_id=0000zkyGIQYgzr3CM50-"
        "Z5tqgIX3rnycmGx0htkyd_WMA60&ns=1&priceTId=213e074f17798925606413055e1141&skuId=6249834403810&"
        "spm=a21n57.1.hoverItem.4&utparam=%7B%22aplus_abtest%22%3A%22c1139bdd41735959cc3b17eb99935f78%22%7D&"
        "xxc=taobaoSearch",
        kLabSeedCoverTheme1,
    },
    {
        "guide_clothing_tmall_1044209976073",
        kThemeClothing,
        "天猫精选衣物（衣·轻搭款）",
        "轻松搭配 · 少糖为你筛过",
        "来自天猫详情页的衣类导购：适合加入日常穿搭候选，点击「去购买」跳转至天猫。含商业合作标识。",
        "以天猫实时价格为准",
        "1044209976073",
        "https://detail.tmall.com/item.htm?abbucket=5&id=1044209976073&mi_id=0000TmdqzhjwyN9TB68G2e5FxZ672eLGH_"
        "GuXttmmTx_cQE&ns=1&priceTId=213e074f17798925606413055e1141&skuId=6229687043342&spm=a21n57.1.hoverItem.2&"
        "utparam=%7B%22aplus_abtest%22%3A%22d7913f46ee6fc2fbc47ee1922980ab65%22%7D&xxc=taobaoSearch",
        kLabSeedCoverTheme1,
    },
    {
        "guide_clothing_tmall_901024796701",
        kThemeClothing,
        "天猫精选衣物（衣·舒适款）",
        "舒适实穿 · 少糖为你筛过",
        "来自天猫详情页的衣类导购：作为衣类频道的精选候选，点击「去购买」跳转至天猫。含商业合作标识。",
        "以天猫实时价格为准",
        "901024796701",
        "https://detail.tmall.com/item.htm?abbucket=5&id=901024796701&mi_id=00002SccXsXPzFdgoZWLlmrD7lhcFYrtPWFRl_"
        "ktNqtLSmg&ns=1&priceTId=213e074f17798925606413055e1141&skuId=5922764406788&spm=a21n57.1.hoverItem.9&"
        "utparam=%7B%22aplus_abtest%22%3A%22b76d28dad3587654c6126bf2ed1a4c26%22%7D&xxc=taobaoSearch",
        kLabSeedCoverTheme1,
    },
    {
        "guide_food_tmall_45410431674",
        kThemeFood,
        "天猫精选食品（食·日常款）",
        "轻食日常 · 少糖为你筛过",
        "来自天猫详情页的食类导购：适合作为食频道的精选候选，点击「去购买」跳转至天猫。含商业合作标识。",
        "以天猫实时价格为准",
        "45410431674",
        "https://detail.tmall.com/item.htm?id=45410431674",
        kLabSeedCoverTheme2,
    },
    {
        "guide_housing_tmall_12345678901",
        kThemeHousing,
        "天猫精选家居（住·实用款）",
        "居家实用 · 少糖为你筛过",
        "来自天猫详情页的家居导购：适合作为住频道的精选候选，点击「去购买」跳转至天猫。含商业合作标识。",
        "以天猫实时价格为准",
        "12345678901",
        "https://detail.tmall.com/item.htm?id=12345678901",
        kLabSeedCoverTheme3,
    },
    {
        "guide_transport_tmall_98765432109",
        kThemeTransport,
        "天猫精选出行（行·便携款）",
        "出行便携 · 少糖为你筛过",
        "来自天猫详情页的出行用品导购：适合作为行频道的精选候选，点击「去购买」跳转至天猫。含商业合作标识。",
        "以天猫实时价格为准",
        "98765432109",
        "https://detail.tmall.com/item.htm?id=98765432109",
        kLabSeedCoverTheme4,
    },
};

}  // namespace simple_living::dev_seeds
