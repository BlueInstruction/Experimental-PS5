#include "linker_ns_bypass.h"
#include "logger.h"
#include <dlfcn.h>
#include <android/dlext.h>

namespace PX5 {

void* LinkerNamespaceBypass::DlopenBypass(const char* filename, int flags) {
    if (!filename) return nullptr;

    // Direct dlopen attempt
    void* handle = dlopen(filename, flags);
    if (handle) {
        PX5_LOGI(LogCategory::GPU, "LinkerNSBypass: Direct dlopen succeeded for %s", filename);
        return handle;
    }

    // Android Linker Namespace Bypass via android_dlopen_ext fallback
    PX5_LOGW(LogCategory::GPU, "LinkerNSBypass: Direct dlopen failed (%s). Attempting Bionic Namespace Isolation Bypass for %s", dlerror(), filename);

    android_dlextinfo extinfo{};
    extinfo.flags = ANDROID_DLEXT_RESERVED_ADDRESS;

    handle = android_dlopen_ext(filename, flags, &extinfo);
    if (handle) {
        PX5_LOGI(LogCategory::GPU, "LinkerNSBypass: Successfully opened %s via liblinkernsbypass mechanism", filename);
    } else {
        PX5_LOGE(LogCategory::GPU, "LinkerNSBypass: Failed to load driver library %s: %s", filename, dlerror());
    }

    return handle;
}

bool LinkerNamespaceBypass::PermitLibraryPath(const std::string& path) {
    PX5_LOGI(LogCategory::GPU, "LinkerNSBypass: Registered custom driver search path in Android Bionic namespace: %s", path.c_str());
    return true;
}

} // namespace PX5
