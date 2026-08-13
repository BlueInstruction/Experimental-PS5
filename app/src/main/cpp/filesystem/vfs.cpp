#include "vfs.h"
#include "../utils/logger.h"

namespace PX5 {

VirtualFileSystem& VirtualFileSystem::GetInstance() {
    static VirtualFileSystem instance;
    return instance;
}

void VirtualFileSystem::Initialize(const std::string& baseDir) {
    m_baseDir = baseDir;

    // Standard PS5 Virtual Mount Points
    Mount("/app0", baseDir + "/app0", true);
    Mount("/data", baseDir + "/data", false);
    Mount("/dev", baseDir + "/dev", false);
    Mount("/system", baseDir + "/system", true);
    Mount("/sandbox", baseDir + "/sandbox", false);
    Mount("/tmp", baseDir + "/tmp", false);

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
    for (const auto& [/*unused*/, mount] : m_mounts) {
        result.push_back(mount);
    }
    return result;
}

} // namespace PX5

