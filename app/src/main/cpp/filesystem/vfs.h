#ifndef PX5_VFS_H
#define PX5_VFS_H

#include <string>
#include <unordered_map>
#include <vector>

namespace PX5 {

struct MountPoint {
    std::string virtualPath;
    std::string hostPath;
    bool readOnly;
};

class VirtualFileSystem {
public:
    static VirtualFileSystem& GetInstance();

    void Initialize(const std::string& baseDir);
    bool Mount(const std::string& vpath, const std::string& hpath, bool readOnly = false);
    bool Unmount(const std::string& vpath);

    std::string ResolveHostPath(const std::string& guestPath) const;
    std::vector<MountPoint> GetMountPoints() const;

private:
    VirtualFileSystem() = default;
    ~VirtualFileSystem() = default;

    std::string m_baseDir;
    std::unordered_map<std::string, MountPoint> m_mounts;
};

} // namespace PX5

#endif // PX5_VFS_H
