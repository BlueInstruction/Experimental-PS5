#ifndef PX5_LINKER_NS_BYPASS_H
#define PX5_LINKER_NS_BYPASS_H

#include <string>

namespace PX5 {

class LinkerNamespaceBypass {
public:
    // Bypass Android linker namespace limits to open arbitrary native libraries (.so)
    static void* DlopenBypass(const char* filename, int flags);
    static bool PermitLibraryPath(const std::string& path);
};

} // namespace PX5

#endif // PX5_LINKER_NS_BYPASS_H
