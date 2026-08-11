#ifndef PSX5_FEXCORE_INTEGRATION_H
#define PSX5_FEXCORE_INTEGRATION_H

#include <string>

namespace PX5::FexCoreIntegration {

bool Initialize();
void Shutdown();
bool IsInitialized();
std::string GetArchitectureSummary();
bool RunGuestCodeTest();

}

#endif