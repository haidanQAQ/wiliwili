/*
    Copyright 2020-2021 natinusala

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <borealis/core/application.hpp>
#include <borealis/core/touch/tap_gesture.hpp>

#include "activity/main_activity.hpp"
#include "utils/activity_helper.hpp"
#include "utils/dialog_helper.hpp"
#include "view/custom_button.hpp"
#include "view/svg_image.hpp"

MainActivity::~MainActivity() { brls::Logger::debug("del MainActivity"); }

void MainActivity::onContentAvailable() {
    // 主页只保留 AniSwitch 一个入口；wiliwili 的收件箱/设置占位按钮已移除。
    // 退出应用改用 HOME 键的默认行为（borealis 的全局退出动作）。
    this->registerAction(
        "退出", brls::ControllerButton::BUTTON_B,
        [](brls::View* view) -> bool {
            brls::Application::quit();
            return true;
        },
        true);

    this->aniswitchBtn->registerClickAction([](brls::View* view) -> bool {
        Intent::openAniSwitch();
        return true;
    });

    this->aniswitchBtn->getFocusEvent()->subscribe([this](bool value) {
        SVGImage* image = dynamic_cast<SVGImage*>(this->aniswitchBtn->getChildren()[0]);
        if (!image) return;
        if (value) {
            image->setImageFromSVGRes("svg/ico-aniswitch-activate.svg");
        } else {
            image->setImageFromSVGRes("svg/ico-aniswitch.svg");
        }
    });

    this->aniswitchBtn->addGestureRecognizer(new brls::TapGestureRecognizer(this->aniswitchBtn));

    brls::Application::giveFocus(this->aniswitchBtn);
}
