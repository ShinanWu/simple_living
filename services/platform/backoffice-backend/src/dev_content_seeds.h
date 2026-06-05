#pragma once

// Lab bootstrap seeds owned by backoffice-backend. Written to PostgreSQL on first ConnectAndInit,
// then exported via snapshot bundles for recommendation-server / tracking-server.
namespace simple_living::dev_seeds {

struct PublishedGuideSeed {
    const char* guide_card_id;
    const char* title;
    const char* subtitle;
    const char* summary;
    const char* price_hint;
    const char* external_item_id;
    const char* landing_url;
};

inline constexpr const char kClothingThemeId[] = "theme_1";

inline constexpr PublishedGuideSeed kPublishedClothingGuides[] = {
    {
        "guide_clothing_tmall_919142939015",
        "天猫精选衣物（衣·基础款）",
        "日常通勤 · 少糖为你筛过",
        "来自天猫详情页的衣类导购：点击「去购买」将跳转至天猫完成选购。含商业合作标识。",
        "以天猫实时价格为准",
        "919142939015",
        "https://detail.tmall.com/item.htm?abbucket=5&id=919142939015&mi_id=0000zkyGIQYgzr3CM50-"
        "Z5tqgIX3rnycmGx0htkyd_WMA60&ns=1&priceTId=213e074f17798925606413055e1141&skuId=6249834403810&"
        "spm=a21n57.1.hoverItem.4&utparam=%7B%22aplus_abtest%22%3A%22c1139bdd41735959cc3b17eb99935f78%22%7D&"
        "xxc=taobaoSearch",
    },
    {
        "guide_clothing_tmall_1044209976073",
        "天猫精选衣物（衣·轻搭款）",
        "轻松搭配 · 少糖为你筛过",
        "来自天猫详情页的衣类导购：适合加入日常穿搭候选，点击「去购买」跳转至天猫。含商业合作标识。",
        "以天猫实时价格为准",
        "1044209976073",
        "https://detail.tmall.com/item.htm?abbucket=5&id=1044209976073&mi_id=0000TmdqzhjwyN9TB68G2e5FxZ672eLGH_"
        "GuXttmmTx_cQE&ns=1&priceTId=213e074f17798925606413055e1141&skuId=6229687043342&spm=a21n57.1.hoverItem.2&"
        "utparam=%7B%22aplus_abtest%22%3A%22d7913f46ee6fc2fbc47ee1922980ab65%22%7D&xxc=taobaoSearch",
    },
    {
        "guide_clothing_tmall_901024796701",
        "天猫精选衣物（衣·舒适款）",
        "舒适实穿 · 少糖为你筛过",
        "来自天猫详情页的衣类导购：作为衣类频道的精选候选，点击「去购买」跳转至天猫。含商业合作标识。",
        "以天猫实时价格为准",
        "901024796701",
        "https://detail.tmall.com/item.htm?abbucket=5&id=901024796701&mi_id=00002SccXsXPzFdgoZWLlmrD7lhcFYrtPWFRl_"
        "ktNqtLSmg&ns=1&priceTId=213e074f17798925606413055e1141&skuId=5922764406788&spm=a21n57.1.hoverItem.9&"
        "utparam=%7B%22aplus_abtest%22%3A%22b76d28dad3587654c6126bf2ed1a4c26%22%7D&xxc=taobaoSearch",
    },
};

}  // namespace simple_living::dev_seeds
