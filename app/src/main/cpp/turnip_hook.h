#ifndef PX5_TURNIP_HOOK_H
#define PX5_TURNIP_HOOK_H

#include <jni.h>
#include <string>

struct TurnipDriverConfig {
    std::string driverPath;
    std::string libraryName;
    std::string hookLibrary;
    bool enableBCnDecoding;
    bool enablePipelineCache;
    bool enableKgslHook;
};

class TurnipDriverHook {
public:
    static TurnipDriverHook& getInstance();
    
    bool initAdrenotools(const std::string& driverDir, const std::string& libName, const std::string& hookLib);
    void setBCnDecoding(bool enabled);
    void setPipelineCaching(bool enabled);
    std::string getDriverInfo() const;

private:
    TurnipDriverHook() = default;
    TurnipDriverConfig m_config{
        "/sdcard/PX5/Drivers/turnip_24_1",
        "Turnip Mesa 24.1.0-devel",
        "libadrenotools.so",
        true,
        true,
        true
    };
};

#endif // PX5_TURNIP_HOOK_H
