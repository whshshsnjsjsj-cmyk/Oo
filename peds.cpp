#include "common.h"
#include <dlfcn.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cstring>
#include <cstdio>
#include <cctype>

static void* g_gta = nullptr;
RwApi RW;

void* Sym(const char* name) {
    if (!g_gta) {
        g_gta = dlopen("libGTASA.so", RTLD_NOW | RTLD_NOLOAD);
        if (!g_gta) g_gta = RTLD_DEFAULT;
    }
    return dlsym(g_gta, name);
}

bool RwInit() {
    bool ok = true;
#define L(f, n) do { RW.f = (decltype(RW.f))Sym(n); if (!RW.f) { logger->Error("missing symbol: %s", n); ok = false; } } while (0)
    L(StreamOpen,        "_Z12RwStreamOpen12RwStreamType18RwStreamAccessTypePKv");
    L(StreamClose,       "_Z13RwStreamCloseP8RwStreamPv");
    L(StreamFindChunk,   "_Z17RwStreamFindChunkP8RwStreamjPjS1_");
    L(ClumpStreamRead,   "_Z17RpClumpStreamReadP8RwStream");
    L(ClumpDestroy,      "_Z14RpClumpDestroyP7RpClump");
    L(ClumpRender,       "_Z13RpClumpRenderP7RpClump");
    L(ClumpForAllAtomics,"_Z20RpClumpForAllAtomicsP7RpClumpPFP8RpAtomicS2_PvES3_");
    L(FrameAddChild,     "_Z15RwFrameAddChildP7RwFrameS0_");
    L(FrameRemoveChild,  "_Z18RwFrameRemoveChildP7RwFrame");
    L(FrameTransform,    "_Z16RwFrameTransformP7RwFramePK11RwMatrixTag15RwOpCombineType");
    L(FrameTranslate,    "_Z16RwFrameTranslateP7RwFramePK5RwV3d15RwOpCombineType");
    L(FrameRotate,       "_Z13RwFrameRotateP7RwFramePK5RwV3df15RwOpCombineType");
    L(FrameScale,        "_Z12RwFrameScaleP7RwFramePK5RwV3d15RwOpCombineType");
    L(FrameUpdateObjects,"_Z20RwFrameUpdateObjectsP7RwFrame");
    L(GeometryForAllMaterials, "_Z25RpGeometryForAllMaterialsP10RpGeometryPFP10RpMaterialS2_PvES3_");
    L(MaterialSetTexture,"_Z20RpMaterialSetTextureP10RpMaterialP9RwTexture");
    L(PngRead,           "_Z14RtPNGImageReadPKc");
    L(ImageFindRasterFormat, "_Z23RwImageFindRasterFormatP7RwImageiPiS1_S1_S1_");
    L(RasterCreate,      "_Z14RwRasterCreateiiii");
    L(RasterSetFromImage,"_Z20RwRasterSetFromImageP8RwRasterP7RwImage");
    L(TextureCreate,     "_Z15RwTextureCreateP8RwRaster");
    L(ImageDestroy,      "_Z14RwImageDestroyP7RwImage");
    L(GetFrameFromId,    "_ZN15CClumpModelInfo14GetFrameFromIdEP7RpClumpi");
#undef L
    return ok;
}

// Readability probe without risking a crash: write() fails with EFAULT on bad memory.
bool SafeRead(const void* p, size_t n) {
    static int fds[2] = { -1, -1 };
    if (!p || n == 0 || n > 256) return false;
    if (fds[0] < 0 && pipe(fds) != 0) return false;
    ssize_t w = write(fds[1], p, n);
    if (w != (ssize_t)n) return false;
    char tmp[256];
    return read(fds[0], tmp, n) == (ssize_t)n;
}

void* PedFromHandle(int handle) {
    static void* (*fn)(int) = (void* (*)(int))Sym("_ZN6CPools6GetPedEi");
    return (fn && handle > 0) ? fn(handle) : nullptr;
}

// CPlaceable (64-bit): vtbl@0, placement{pos(12),heading}@8, CMatrix* @24 ; CMatrix.pos @48
bool PedPos(void* ped, float out[3]) {
    if (!ped) return false;
    char* e = (char*)ped;
    void* m = *(void**)(e + 24);
    const float* p = (m && SafeRead(m, 64)) ? (const float*)((char*)m + 48) : (const float*)(e + 8);
    if (!SafeRead(p, 12)) return false;
    out[0] = p[0]; out[1] = p[1]; out[2] = p[2];
    return true;
}

static int g_clumpOff = -1;
static bool LooksLikeClump(void* p) {
    if (!p || !SafeRead(p, 16)) return false;
    if (*(uint8_t*)p != 2) return false;                      // rpCLUMP object type
    RwFrame* f = ClumpFrame((RpClump*)p);
    if (!f || !SafeRead(f, 8)) return false;
    return RW.GetFrameFromId((RpClump*)p, 24) != nullptr;     // ped skeletons have bone id 24
}
void* PedClump(void* ped) {
    if (!ped) return nullptr;
    if (g_clumpOff < 0) {
        static void* (*findPlayer)(int) = (void* (*)(int))Sym("_Z13FindPlayerPedi");
        void* pl = findPlayer ? findPlayer(-1) : nullptr;
        if (pl) {
            for (int off = 0x10; off <= 0x80; off += 8)
                if (LooksLikeClump(*(void**)((char*)pl + off))) { g_clumpOff = off; break; }
            logger->Info("CPed clump offset: %s 0x%x", g_clumpOff < 0 ? "NOT FOUND, default" : "found", g_clumpOff < 0 ? 0x20 : g_clumpOff);
            if (g_clumpOff < 0) g_clumpOff = 0x20;
        } else return LooksLikeClump(*(void**)((char*)ped + 0x20)) ? *(void**)((char*)ped + 0x20) : nullptr;
    }
    void* c = *(void**)((char*)ped + g_clumpOff);
    return LooksLikeClump(c) ? c : nullptr;
}

std::string ResolvePath(const char* in) {
    std::string p(in ? in : "");
    for (auto& c : p) if (c == '\\') c = '/';
    if (access(p.c_str(), F_OK) == 0) return p;
    char cmd[128] = {0};
    FILE* f = fopen("/proc/self/cmdline", "r");
    if (f) { fread(cmd, 1, sizeof(cmd) - 1, f); fclose(f); }
    std::string pkg(cmd), base = "/storage/emulated/0/Android/data/" + pkg + "/files/";
    const char* dirs[] = { "", "cleo/", "CLEO/", "Cleo/" };
    for (auto d : dirs) { std::string c = base + d + p; if (access(c.c_str(), F_OK) == 0) return c; }
    return p;
}

bool HookPedVtable(int slot, void* hook, void** orig, const char* origSym) {
    char* vt = (char*)Sym("_ZTV4CPed");
    void* expect = Sym(origSym);
    if (!vt || !expect) { logger->Error("vtable/symbol not found (%s)", origSym); return false; }
    uintptr_t* slots = (uintptr_t*)(vt + 16);
    uintptr_t cur = slots[slot];
    if (cur != (uintptr_t)expect) {
        Dl_info di;
        if (!dladdr((void*)cur, &di) || !di.dli_fname || strstr(di.dli_fname, "libGTASA")) {
            logger->Error("CPed vtable slot %d does not match %s - wrong game build?", slot, origSym);
            return false;
        }                                                      // else: another mod already hooked it, chain
    }
    long pg = sysconf(_SC_PAGESIZE);
    uintptr_t a = (uintptr_t)&slots[slot] & ~(uintptr_t)(pg - 1);
    if (mprotect((void*)a, pg * 2, PROT_READ | PROT_WRITE) != 0) { logger->Error("mprotect failed"); return false; }
    *orig = (void*)cur;
    slots[slot] = (uintptr_t)hook;
    mprotect((void*)a, pg * 2, PROT_READ);
    return true;
}
