//
// AniSwitch 首页：WebDAV 源列表
//
//   +（Switch 上的 + 键 = BUTTON_START）：新增源，四步输入 名称 / 地址 / 用户名 / 密码
//   -（Switch 上的 - 键 = BUTTON_BACK） ：删除当前选中的源，二次确认
//   A                                    ：进入该源
//   B                                    ：返回 wiliwili 主界面
//
// 配置存放：sdmc:/config/wiliwili/aniswitch_sources.json
//

#include <cstdio>
#include <string>
#include <vector>

#include <borealis/core/application.hpp>
#include <borealis/core/box.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/views/dialog.hpp>

#include "activity/aniswitch_activity.hpp"
#include "activity/aniswitch_add_dialog.hpp"
#include "utils/activity_helper.hpp"
#include "utils/aniswitch_util.hpp"

AniSwitchSourceCell::AniSwitchSourceCell() {
    this->setFocusable(true);
    this->setHeight(110.0f);
    this->setWidthPercentage(100.0f);
    this->setPadding(12, 18, 12, 18);

    this->nameLabel = new brls::Label();
    this->nameLabel->setFontSize(26);
    this->nameLabel->setWidthPercentage(100.0f);
    this->addView(this->nameLabel);

    this->detailLabel = new brls::Label();
    this->detailLabel->setFontSize(16);
    this->detailLabel->setWidthPercentage(100.0f);
    this->detailLabel->setMarginTop(6);
    this->detailLabel->setTextColor(nvgRGB(160, 160, 160));
    this->addView(this->detailLabel);
}

void AniSwitchSourceCell::setSource(const AniSwitchSource& source, size_t index) {
    this->index = index;

    std::string title = source.name;
    if (source.builtin) title += "（内置）";
    this->nameLabel->setText(title);

    std::string detail = source.url;
    if (!source.user.empty()) detail += "  ·  用户 " + source.user;
    this->detailLabel->setText(detail);
}

void AniSwitchSourceCell::onFocusGained() {
    RecyclingGridItem::onFocusGained();
    if (this->focusCallback) this->focusCallback(this->index);
}

RecyclingGridItem* AniSwitchSourceDataSource::cellForRow(RecyclingGrid* recycler, size_t index) {
    auto* cell = static_cast<AniSwitchSourceCell*>(recycler->dequeueReusableCell("AniSwitchSourceCell"));
    if (!cell) {
        cell = new AniSwitchSourceCell();
        cell->setFocusCallback([this](size_t i) {
            if (this->focusCallback) this->focusCallback(i);
        });
    }
    if (index < this->sources.size()) cell->setSource(this->sources[index], index);
    return cell;
}

void AniSwitchSourceDataSource::onItemSelected(RecyclingGrid* recycler, size_t index) {
    if (this->selectCallback) this->selectCallback(index);
}

AniSwitchActivity::AniSwitchActivity() { aniswitchLog("== AniSwitchActivity ctor (sources)"); }

AniSwitchActivity::~AniSwitchActivity() { aniswitchLog("== AniSwitchActivity dtor (sources)"); }

brls::View* AniSwitchActivity::createContentView() {
    return brls::View::createFromXMLResource("activity/aniswitch_sources.xml");
}

void AniSwitchActivity::willAppear(bool resetState) {
    brls::Activity::willAppear(resetState);
    aniswitchLog("== sources willAppear");
    if (this->grid) brls::Application::giveFocus(this->grid);
}

void AniSwitchActivity::onContentAvailable() {
    brls::Activity::onContentAvailable();
    aniswitchLog("== sources onContentAvailable");

    this->status = dynamic_cast<brls::Label*>(this->getView("aniswitch/status"));
    this->grid   = dynamic_cast<RecyclingGrid*>(this->getView("aniswitch/grid"));

    if (!this->grid) {
        aniswitchLog("sources: grid null");
        return;
    }

    this->grid->registerCell("AniSwitchSourceCell", []() { return new AniSwitchSourceCell(); });
    this->grid->showSkeleton();
    this->grid->setRefreshAction([this]() { this->reload(); });

    // + 键：新增 WebDAV 源
    this->registerAction(
        "添加", brls::ControllerButton::BUTTON_START,
        [this](brls::View*) -> bool {
            this->addSourceFlow();
            return true;
        },
        false, false, brls::SOUND_CLICK);

    // - 键：删除选中的源
    this->registerAction(
        "删除", brls::ControllerButton::BUTTON_BACK,
        [this](brls::View*) -> bool {
            this->deleteSelectedSource();
            return true;
        },
        false, false, brls::SOUND_CLICK);

    // X 键：编辑选中的源
    this->registerAction(
        "编辑", brls::ControllerButton::BUTTON_X,
        [this](brls::View*) -> bool {
            this->editSelectedSource();
            return true;
        },
        false, false, brls::SOUND_CLICK);

    this->registerAction(
        "返回", brls::ControllerButton::BUTTON_B,
        [](brls::View*) -> bool {
            brls::Application::popActivity(brls::TransitionAnimation::FADE);
            return true;
        },
        true);

    this->reload();
}

void AniSwitchActivity::reload() {
    this->sources  = aniswitch::loadSources();
    this->selected = 0;

    auto* ds = new AniSwitchSourceDataSource(this->sources);
    ds->setSelectCallback([this](size_t index) {
        this->selected = index;
        this->openSelectedSource();
    });
    ds->setFocusCallback([this](size_t index) { this->selected = index; });

    this->grid->setDataSource(ds);
    this->grid->notifyDataChanged();
    brls::Application::giveFocus(this->grid);

    char tip[200];
    snprintf(tip, sizeof(tip), "共 %zu 个源 · + 添加 · − 删除 · X 编辑 · A 进入",
             this->sources.size());
    this->showMessage(tip);
    aniswitchLog("sources: grid filled, count=" + std::to_string(this->sources.size()));
}

void AniSwitchActivity::showMessage(const std::string& text) {
    if (this->status) this->status->setText(text);
}

void AniSwitchActivity::openSelectedSource() {
    if (this->selected >= this->sources.size()) return;
    const AniSwitchSource source = this->sources[this->selected];
    aniswitchLog("sources: open " + source.name + " url=" + source.url);

    if (source.builtin) {
        Intent::openAniSwitchList();
        return;
    }

    // WebDAV 源：进入目录浏览（PROPFIND）
    Intent::openAniSwitchBrowse(source, "");
}

size_t AniSwitchActivity::focusedSourceIndex() const {
    // 光标停在哪个格子上，就以那个格子的 index 为准：
    // RecyclingGrid 复用单元格时光标的 onFocusGained 不一定再次触发，
    // 只靠 this->selected 会出现「光标在 b、删掉的是 a」。
    if (auto* item = dynamic_cast<RecyclingGridItem*>(brls::Application::getCurrentFocus())) {
        if (item->getIndex() < this->sources.size()) return item->getIndex();
    }
    return this->selected < this->sources.size() ? this->selected : 0;
}

void AniSwitchActivity::deleteSelectedSource() {
    if (this->sources.empty()) return;

    const size_t target = this->focusedSourceIndex();
    if (target >= this->sources.size()) return;
    const AniSwitchSource source = this->sources[target];
    aniswitchLog("sources: delete target=" + std::to_string(target) + " name=" + source.name);

    if (source.builtin) {
        brls::Dialog* dialog = new brls::Dialog("内置网关源不可删除");
        dialog->addButton("确定", []() {});
        dialog->open();
        return;
    }

    brls::Dialog* dialog = new brls::Dialog("确定删除这个 WebDAV 源吗？\n\n" + source.name + "\n" + source.url);
    dialog->addButton("取消", []() {});
    dialog->addButton("确定", [this, source]() {
        std::vector<AniSwitchSource> next;
        for (auto& s : this->sources) {
            if (s.url == source.url && s.name == source.name) continue;
            next.push_back(s);
        }
        aniswitch::saveSources(next);
        aniswitchLog("sources: deleted " + source.name);
        this->reload();
    });
    dialog->open();
}


void AniSwitchActivity::addSourceFlow() {
    aniswitchShowSourceDialog("添加 WebDAV", AniSwitchSource{},
                              [this](const AniSwitchSource& source) {
                                  std::vector<AniSwitchSource> next = this->sources;
                                  next.push_back(source);
                                  aniswitch::saveSources(next);
                                  aniswitchLog("sources: added " + source.name + " " + source.url);
                                  this->reload();
                              });
}

void AniSwitchActivity::editSelectedSource() {
    if (this->sources.empty()) return;

    const size_t target = this->focusedSourceIndex();
    if (target >= this->sources.size()) return;

    const AniSwitchSource current = this->sources[target];
    aniswitchLog("sources: edit target=" + std::to_string(target) + " name=" + current.name);

    aniswitchShowSourceDialog("编辑 WebDAV", current, [this, target](const AniSwitchSource& source) {
        std::vector<AniSwitchSource> next = this->sources;
        if (target >= next.size()) return;
        next[target] = source;
        aniswitch::saveSources(next);
        aniswitchLog("sources: edited target=" + std::to_string(target) + " -> " + source.url);
        this->reload();
    });
}
