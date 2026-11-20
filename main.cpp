#include "common.h"

cleo_ifs_t* cleo = nullptr;

#ifdef BUILD_VISUALOBJECT
MYMODCFG(net.teco.visualobject, CLEO VisualObject arm64, 1.0.2, local build)
#else
MYMODCFG(net.juniordjjr.rusjj.cleoplus, CLEOPlus arm64 subset, 1.0.0, local build)
#endif

extern "C" void OnAllModsLoaded() {
    FileLog("OnAllModsLoaded");
    cleo = (cleo_ifs_t*)GetInterface("CLEO");
    FileLog("GetInterface(CLEO) = %p, GetInterface(CLEOAddon) = %p", (void*)cleo, (void*)GetInterface("CLEOAddon"));
    if (!cleo) { FileLog("CLEO interface not found - plugin inactive"); return; }
    if (const char* d = CleoStorageDir()) FileLog("cleo storage dir: %s", d);
    else FileLog("cleo storage dir: unavailable");
    RegisterOpcodes();
    FileLog("opcodes registered");
}
