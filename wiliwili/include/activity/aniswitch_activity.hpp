#pragma once

#include <string>
#include <vector>

#include <borealis/core/activity.hpp>
#include <borealis/core/bind.hpp>
#include <borealis/views/label.hpp>

#include "view/recycling_grid.hpp"

// AniSwitch: 从 AniSwitch Gateway 拉取 OpenList 播放列表 + 合并弹幕
// Gateway 默认地址可通过环境变量 ANISWITCH_GW 覆盖（Switch 上没有环境变量，
// 此时使用编译期默认值）
#ifndef ANISWITCH_GW_DEFAULT
#define ANISWITCH_GW_DEFAULT "http://192.168.31.251:9333"
#endif

struct AniSwitchEpisode {
    int ep = 0;
    std::string name;
    long long size = 0;
    std::string url;
};

class AniSwitchCell : public RecyclingGridItem {
public:
    AniSwitchCell();
    void setText(const std::string& text);
    void prepareForReuse() override;
    void cacheForReuse() override;

private:
    brls::Label* label = nullptr;
};

class AniSwitchDataSource : public RecyclingGridDataSource {
public:
    explicit AniSwitchDataSource(std::vector<AniSwitchEpisode> list) : episodes(std::move(list)) {}

    size_t getItemCount() override { return episodes.size(); }
    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) override;
    void onItemSelected(RecyclingGrid* recycler, size_t index) override;
    void clearData() override { episodes.clear(); }

    std::vector<AniSwitchEpisode> episodes;
};

class AniSwitchActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/aniswitch_activity.xml");

    AniSwitchActivity() = default;

    void onContentAvailable() override;

private:
    BRLS_BIND(RecyclingGrid, grid, "aniswitch/grid");
    BRLS_BIND(brls::Label, status, "aniswitch/status");

    void loadEpisodes();
};
