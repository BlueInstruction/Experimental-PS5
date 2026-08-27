#include "elf_loader.h"
#include "../utils/logger.h"

namespace PX5 {

// v1 of this file was FAKE: it logged "Decrypting PS5 SELF" and then handed
// the encrypted bytes straight to the ELF loader.
//
// v2 honesty contract: all SELF logic lives in ElfLoader::LoadSelf()
// (elf_loader.cpp) which detects the magic and rejects with an explicit
// Phase-C explanation. This TU is kept only so the CMake source list keeps
// compiling without churn; add real SELF crypto work here in Phase C.

} // namespace PX5
