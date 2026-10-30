#pragma once

#include <string>
#include <vector>

#include <borealis/core/activity.hpp>
#include <borealis/views/label.hpp>

#include "activity/aniswitch_sources.hpp"
#include "view/recycling_grid.hpp"

// 内置网关的剧集（火影 4K，723 集）
struct AniSwitchEpisode {
    int ep         = 0;
    std::string name;
    long long size = 0;
    std::string url;
};

class AniSwitchEpisodeCell : public RecyclingGridItem {
public:
    AniSwitchEpisodeCell();
    ~AniSwitchEpisodeCell() override = default;

    void setEpisode(const AniSwitchEpisode& episode);

private:
    brls::Label* title = nullptr;
};

class AniSwitchEpisodeDataSource : public RecyclingGridDataSource {
public:
    explicit AniSwitchEpisodeDataSource(std::vector<AniSwitchEpisode> episodes)
        : episodes(std::move(episodes)) {}

    size_t getItemCount() override { return this->episodes.size(); }

    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) override;

    void onItemSelected(RecyclingGrid* recycler, size_t index) override;

    void clearData() override { this->episodes.clear(); }

    void setSelectCallback(std::function<void(size_t)> callback) { this->callback = std::move(callback); }

private:
    std::vector<AniSwitchEpisode> episodes;
    std::function<void(size_t)> callback;
};

// 剧集列表页（数据来自 AniSwitch Gateway /api/episodes）
class AniSwitchListActivity : public brls::Activity {
public:
    AniSwitchListActivity();
    ~AniSwitchListActivity() override;

    brls::View* createContentView() override;

    void onContentAvailable() override;

    void willAppear(bool resetState = false) override;

private:
    brls::Label* status = nullptr;
    RecyclingGrid* grid = nullptr;
    std::vector<AniSwitchEpisode> episodes;

    std::shared_ptr<std::atomic<bool>> alive  = std::make_shared<std::atomic<bool>>(true);
    std::shared_ptr<std::atomic<int>> ticket = std::make_shared<std::atomic<int>>(0);

    void loadEpisodes();
    void showMessage(const std::string& text);
    void openEpisode(size_t index);
};
