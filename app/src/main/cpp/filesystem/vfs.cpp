#include "vfs.h"
#include "../utils/logger.h"

#include <sys/stat.h>

namespace PX5 {

namespace {

// v1.59 — mkdir -p: the v1.58 device session proved the mount points are
// recorded as strings but never existed on disk (step 5 died on ENOENT
// inside <logs>/vfs/). Create every path component; EEXIST is success.
void MkDirP(const std::string& path) {
    size_t pos = 0;
    while ((pos = path.find('/', pos + 1)) != std::string::npos) {
        const std::string dir = path.substr(0, pos);
        if (dir.empty()) continue;  // leading '/'
        if (mkdir(dir.c_str(), 0755) != 0 && errno != EEXIST) {
            PX5_LOGE(LogCategory::FILESYSTEM,
                     "VFS mkdir(%s) failed: errno=%d", dir.c_str(), errno);
            // Keep going: remaining components may still be creatable, and
            // the first real file operation reports the same errno with
            // full context if the anchor truly cannot be created.
        }
    }
    if (!path.empty() &&
        mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
        PX5_LOGE(LogCategory::FILESYSTEM,
                 "VFS mkdir(%s) failed: errno=%d", path.c_str(), errno);
    }
}

}  // namespace

VirtualFileSystem& VirtualFileSystem::GetInstance() {
    static VirtualFileSystem instance;
    return instance;
}

void VirtualFileSystem::Initialize(const std::string& baseDir) {
    m_baseDir = baseDir;

    // v1.59 — create the anchor and every standard mount point BEFORE the
    // first fixture/guest write touches them (see MkDirP note above).
    MkDirP(baseDir);

    // Standard PS5 Virtual Mount Points
    Mount("/app0", baseDir + "/app0", true);
    Mount("/data", baseDir + "/data", false);
    Mount("/dev", baseDir + "/dev", false);
    Mount("/system", baseDir + "/system", true);
    Mount("/sandbox", baseDir + "/sandbox", false);
    Mount("/tmp", baseDir + "/tmp", false);
    MkDirP(baseDir + "/app0");
    MkDirP(baseDir + "/data");
    MkDirP(baseDir + "/dev");
    MkDirP(baseDir + "/system");
    MkDirP(baseDir + "/sandbox");
    MkDirP(baseDir + "/tmp");

    PX5_LOGI(LogCategory::FILESYSTEM, "VFS initialized with base path: %s", baseDir.c_str());
}

bool VirtualFileSystem::Mount(const std::string& vpath, const std::string& hpath, bool readOnly) {
    m_mounts[vpath] = { vpath, hpath, readOnly };
    PX5_LOGI(LogCategory::FILESYSTEM, "Mounted VFS: %s -> %s [%s]", vpath.c_str(), hpath.c_str(), readOnly ? "RO" : "RW");
    return true;
}

bool VirtualFileSystem::Unmount(const std::string& vpath) {
    return m_mounts.erase(vpath) > 0;
}

std::string VirtualFileSystem::ResolveHostPath(const std::string& guestPath) const {
    for (const auto& [vpath, mount] : m_mounts) {
        if (guestPath.rfind(vpath, 0) == 0) { // starts_with
            std::string sub = guestPath.substr(vpath.size());
            return mount.hostPath + sub;
        }
    }
    return m_baseDir + guestPath;
}

std::vector<MountPoint> VirtualFileSystem::GetMountPoints() const {
    std::vector<MountPoint> result;
    for (const auto& kv : m_mounts) {
        result.push_back(kv.second);
    }
    return result;
}

} // namespace PX5


