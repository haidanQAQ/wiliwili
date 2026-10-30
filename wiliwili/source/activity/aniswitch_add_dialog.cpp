//
// AniSwitch: 「添加 / 编辑 WebDAV」小窗
//
// 一个弹窗里三个输入框（地址 / 用户名 / 密码），底部 取消 / 确定。
// 每个输入框是一个可聚焦的行：手柄选中后按 A 打开软键盘，也支持触屏点按。
// 地址会自动补全 http:// 前缀（见 aniswitchNormalizeUrl）。
//

#include <string>

#include <borealis/core/application.hpp>
#include <borealis/core/box.hpp>
#include <borealis/core/ime.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/core/touch/tap_gesture.hpp>
#include <borealis/views/dialog.hpp>
#include <borealis/views/label.hpp>

#include "activity/aniswitch_add_dialog.hpp"
#include "utils/aniswitch_util.hpp"

namespace {

const NVGcolor COLOR_ROW_NORMAL  = {0.16f, 0.16f, 0.19f, 1.0f};
const NVGcolor COLOR_ROW_FOCUSED = {0.24f, 0.24f, 0.30f, 1.0f};

class AniSwitchFieldRow : public brls::Box {
public:
    AniSwitchFieldRow(std::string title, std::string hint, bool masked, std::string placeholder,
                      std::string initial)
        : title(std::move(title)), hint(std::move(hint)), masked(masked),
          placeholder(std::move(placeholder)), value(std::move(initial)) {
        this->setAxis(brls::Axis::ROW);
        this->setAlignItems(brls::AlignItems::CENTER);
        this->setFocusable(true);
        this->setWidthPercentage(100);
        this->setHeight(64);
        this->setPadding(10, 14, 10, 14);
        this->setMarginBottom(10);
        this->setCornerRadius(6);
        this->setBackgroundColor(COLOR_ROW_NORMAL);

        auto* name = new brls::Label();
        name->setText(this->title);
        name->setFontSize(20);
        name->setWidth(120);
        name->setTextColor(nvgRGB(200, 200, 200));
        this->addView(name);

        this->valueLabel = new brls::Label();
        this->valueLabel->setFontSize(20);
        this->valueLabel->setGrow(1);
        this->addView(this->valueLabel);

        // 手柄：选中后按 A
        this->registerClickAction([this](brls::View*) {
            this->edit();
            return true;
        });
        // 触屏：点一下同样能编辑
        this->addGestureRecognizer(new brls::TapGestureRecognizer(this));

        this->refreshLabel();
    }

    std::string getValue() const { return this->value; }

    void onFocusGained() override {
        brls::Box::onFocusGained();
        this->setBackgroundColor(COLOR_ROW_FOCUSED);
    }

    void onFocusLost() override {
        brls::Box::onFocusLost();
        this->setBackgroundColor(COLOR_ROW_NORMAL);
    }

private:
    std::string title;
    std::string hint;
    std::string placeholder;
    std::string value;
    bool masked = false;
    brls::Label* valueLabel = nullptr;

    void refreshLabel() {
        if (this->value.empty()) {
            this->valueLabel->setText(this->placeholder);
            this->valueLabel->setTextColor(nvgRGB(120, 120, 120));
            return;
        }
        this->valueLabel->setTextColor(nvgRGB(255, 255, 255));
        if (this->masked) {
            this->valueLabel->setText(std::string(this->value.size(), '*'));
        } else {
            this->valueLabel->setText(this->value);
        }
    }

    void edit() {
        brls::Application::getImeManager()->openForText(
            [this](std::string text) {
                this->value = text;
                this->refreshLabel();
            },
            this->title, this->hint, 220, this->value, 0);
    }
};

}  // namespace

void aniswitchShowSourceDialog(const std::string& title, const AniSwitchSource& initial,
                               std::function<void(const AniSwitchSource&)> onConfirm) {
    auto* content = new brls::Box();
    content->setAxis(brls::Axis::COLUMN);
    content->setWidth(640);
    content->setPadding(24, 24, 16, 24);

    auto* titleLabel = new brls::Label();
    titleLabel->setText(title);
    titleLabel->setFontSize(28);
    titleLabel->setMarginBottom(6);
    content->addView(titleLabel);

    auto* hint = new brls::Label();
    hint->setText("WebDAV 地址需要带 dav 后缀，例如 192.168.31.251:5244/dav（http:// 可省略）");
    hint->setFontSize(16);
    hint->setTextColor(nvgRGB(150, 150, 150));
    hint->setMarginBottom(16);
    content->addView(hint);

    auto* urlRow = new AniSwitchFieldRow("地址", "以 /dav 结尾，http:// 可以省略", false,
                                         "点击输入，例如 192.168.31.251:5244/dav", initial.url);
    auto* userRow =
        new AniSwitchFieldRow("用户名", "没有就留空", false, "点击输入（可留空）", initial.user);
    auto* passRow =
        new AniSwitchFieldRow("密码", "没有就留空", true, "点击输入（可留空）", initial.pass);

    content->addView(urlRow);
    content->addView(userRow);
    content->addView(passRow);

    brls::Dialog* dialog = new brls::Dialog(content);

    dialog->addButton("取消", []() {});

    dialog->addButton("确定", [urlRow, userRow, passRow, initial, onConfirm]() {
        // 注意：Dialog::buttonClick 已经调用 dismiss(cb)，本回调执行时对话框的
        // activity 已经被弹出；这里**不能**再调 dialog->close()，否则会把下面
        // 的 AniSwitchActivity 一起弹掉（表现为按「确定」后界面卡死）。
        const std::string url = aniswitchNormalizeUrl(urlRow->getValue());
        const std::string err = aniswitchCheckUrl(url);
        if (!err.empty()) {
            aniswitchLog("source dialog: " + err);
            brls::sync([err]() {
                brls::Dialog* tip = new brls::Dialog(err);
                tip->addButton("确定", []() {});
                tip->open();
            });
            return;
        }

        AniSwitchSource source;
        source.url     = url;
        source.user    = userRow->getValue();
        source.pass    = passRow->getValue();
        source.builtin = initial.builtin;

        // 显示名：编辑时沿用原名，新增时取 host:port
        if (!initial.name.empty() && initial.builtin == false && initial.url == url) {
            source.name = initial.name;
        } else {
            std::string name = url;
            size_t scheme    = name.find("://");
            if (scheme != std::string::npos) name = name.substr(scheme + 3);
            size_t slash = name.find('/');
            if (slash != std::string::npos) name = name.substr(0, slash);
            source.name = name;
        }

        aniswitchLog("source dialog: confirmed " + source.name + " " + source.url);

        // 延到下一帧再动列表：此刻对话框 activity 正在销毁
        brls::sync([onConfirm, source]() { onConfirm(source); });
    });

    dialog->open();
}
