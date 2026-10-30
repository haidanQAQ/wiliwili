#pragma once

#include <functional>
#include <string>

#include "activity/aniswitch_sources.hpp"

// 弹出「添加 / 编辑 WebDAV」小窗：地址 / 用户名 / 密码 三个输入框 + 取消 / 确定
// initial 为空表示新增（三个框留空）；非空表示编辑（预填原值）
// onConfirm 拿到的 source.name 已按地址自动生成（编辑时保留原显示名）
void aniswitchShowSourceDialog(const std::string& title,
                               const AniSwitchSource& initial,
                               std::function<void(const AniSwitchSource&)> onConfirm);
