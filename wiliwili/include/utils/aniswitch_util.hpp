#pragma once

// AniSwitch 排查日志：直接写 SD 卡，便于在没有 crash report 的情况下定位问题
#include <cstdio>
#include <string>

inline void aniswitchLog(const std::string& text) {
    FILE* fp = fopen("sdmc:/aniswitch.log", "a");
    if (!fp) return;
    fputs(text.c_str(), fp);
    fputc('\n', fp);
    fclose(fp);
}
