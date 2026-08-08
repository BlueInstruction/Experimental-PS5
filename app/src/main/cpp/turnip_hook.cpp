#include "turnip_hook.h"
#include "utils/linker_ns_bypass.h"
#include "utils/logger.h"
#include <android/log.h>

#define LOG_TAG "PX5_TurnipHook"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

TurnipDriverHook& TurnipDriverHook::getInstance() {
    static TurnipDriverHook instance;
    return instance;
}

bool TurnipDriverHook::initAdrenotools(const std::string& driverDir, const std::string& libName, const std::string& hookLib) {
    m_config.driverPath = driverDir;
    m_config.libraryName = libName;
    m_config.hookLibrary = hookLib;

    PX5::LinkerNamespaceBypass::PermitLibraryPath(driverDir);
    std::string fullLibPath = driverDir + "/" + libName;
    PX5::LinkerNamespaceBypass::DlopenBypass(fullLibPath.c_str(), 1); // RTLD_LAZY = 1

    LOGI("libadrenotools + liblinkernsbypass: Driver loaded from %s [%s]", driverDir.c_str(), libName.c_str());
    return true;
}

void TurnipDriverHook::setBCnDecoding(bool enabled) {
    m_config.enableBCnDecoding = enabled;
    LOGI("Turnip BCn Texture Decoding: %s", enabled ? "ENABLED" : "DISABLED");
}

void TurnipDriverHook::setPipelineCaching(bool enabled) {
    m_config.enablePipelineCache = enabled;
    LOGI("Turnip Vulkan Pipeline Caching: %s", enabled ? "ENABLED" : "DISABLED");
}

std::string TurnipDriverHook::getDriverInfo() const {
    std::string info = m_config.libraryName;
    info += " [BCn: " + std::string(m_config.enableBCnDecoding ? "ON" : "OFF");
    info += ", Cache: " + std::string(m_config.enablePipelineCache ? "ON" : "OFF") + "]";
    return info;
}
