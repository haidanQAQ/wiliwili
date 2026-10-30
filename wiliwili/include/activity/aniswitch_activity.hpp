#pragma once

#include <functional>
#include <string>
#include <vector>

#include <borealis/core/activity.hpp>
#include <borealis/views/label.hpp>

#include "activity/aniswitch_sources.hpp"
#include "view/recycling_grid.hpp"

// AniSwitch 首页：WebDAV 源列表
//   + ：新增源（名称 → 地址 → 用户名 → 密码，四步输入）
//   - ：删除当前选中的源（二次确认）
//   A ：进入该源（内置网关 → 剧集列表；WebDAV → 目录浏览）
class AniSwitchSourceCell : public RecyclingGridItem {
public:
    AniSwitchSourceCell();
    ~AniSwitchSourceCell() override = default;

    void setSource(const AniSwitchSource& source, size_t index);

    void setFocusCallback(std::function<void(size_t)> callback) { this->focusCallback = callback; }

    void onFocusGained() override;

private:
    brls::Label* nameLabel   = nullptr;
    brls::Label* detailLabel = nullptr;
    size_t index             = 0;
    std::function<void(size_t)> focusCallback;
};

class AniSwitchSourceDataSource : public RecyclingGridDataSource {
public:
    explicit AniSwitchSourceDataSource(std::vector<AniSwitchSource> sources)
        : sources(std::move(sources)) {}

    size_t getItemCount() override { return this->sources.size(); }

    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) override;

    void onItemSelected(RecyclingGrid* recycler, size_t index) override;

    void clearData() override { this->sources.clear(); }

    void setSelectCallback(std::function<void(size_t)> callback) { this->selectCallback = std::move(callback); }

    void setFocusCallback(std::function<void(size_t)> callback) { this->focusCallback = std::move(callback); }

private:
    std::vector<AniSwitchSource> sources;
    std::function<void(size_t)> selectCallback;
    std::function<void(size_t)> focusCallback;
};

class AniSwitchActivity : public brls::Activity {
public:
    AniSwitchActivity();
    ~AniSwitchActivity() override;

    brls::View* createContentView() override;

    void onContentAvailable() override;

    void willAppear(bool resetState = false) override;

private:
    brls::Label* status = nullptr;
    RecyclingGrid* grid = nullptr;

    std::vector<AniSwitchSource> sources;
    size_t selected = 0;

    void reload();
    void showMessage(const std::string& text);
    void addSourceFlow();
    void editSelectedSource();
    void deleteSelectedSource();
    void openSelectedSource();

    // 以「当前焦点所在的单元格」为准，避免 RecyclcingGrid 复用单元格时
    // onFocusGained 不再触发导致 selected 过期
    size_t focusedSourceIndex() const;
};
