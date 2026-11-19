// Shared declarations for the arm64 CLEO_VisualObject / CLEOPlus replacements (GTA SA 2.10).
#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <mod/amlmod.h>
#include <mod/logger.h>
#include "cleo.h"

extern cleo_ifs_t* cleo;

// ===================== ADAPT: the ONLY code that touches the CLEO SDK =====================
// If the build fails here, the names below must be matched to the cleo.h of your CLEO build.
#define OPC(fn) static void fn(void* h, uint32_t* ip, uint16_t opcode, const char* name)
static inline int   ReadInt(void* h)                 { return cleo->ReadParam(h)->i; }
static inline float ReadFloat(void* h)               { return cleo->ReadParam(h)->f; }
static inline void  ReadStr(void* h, char* b, int n) { cleo->ReadStringLong(h, b, n); }
static inline void  OutInt(void* h, int v)           { cleo->GetPointerToScriptVar(h)->i = v; }
static inline void  OutFloat(void* h, float v)       { cleo->GetPointerToScriptVar(h)->f = v; }
static inline void  Cond(void* h, bool b)            { cleo->SetScriptCondResult(h, b); }
// ===========================================================================================

// ---- RenderWare (opaque types; all calls resolved by symbol name from libGTASA.so 2.10) ----
struct RwFrame; struct RpClump; struct RpAtomic; struct RpGeometry; struct RpMaterial;
struct RwTexture; struct RwStream; struct RwRaster; struct RwImage;
struct RwV3d { float x, y, z; };
struct RwMatrix { RwV3d right; uint32_t flags; RwV3d up; uint32_t p1; RwV3d at; uint32_t p2; RwV3d pos; uint32_t p3; };
struct RwMemory { uint8_t* start; uint32_t length; };
enum { RW_REPLACE = 0, RW_PRECONCAT = 1, RW_POSTCONCAT = 2 };
enum { RW_STREAM_MEMORY = 3, RW_ACCESS_READ = 1, RW_ID_CLUMP = 0x10, RW_RASTER_TEXTURE = 4 };

struct RwApi {
    RwStream* (*StreamOpen)(int, int, const void*);
    int       (*StreamClose)(RwStream*, void*);
    RwStream* (*StreamFindChunk)(RwStream*, uint32_t, uint32_t*, uint32_t*);
    RpClump*  (*ClumpStreamRead)(RwStream*);
    int       (*ClumpDestroy)(RpClump*);
    RpClump*  (*ClumpRender)(RpClump*);
    RpClump*  (*ClumpForAllAtomics)(RpClump*, RpAtomic* (*)(RpAtomic*, void*), void*);
    RwFrame*  (*FrameAddChild)(RwFrame*, RwFrame*);
    RwFrame*  (*FrameRemoveChild)(RwFrame*);
    RwFrame*  (*FrameTransform)(RwFrame*, const RwMatrix*, int);
    RwFrame*  (*FrameTranslate)(RwFrame*, const RwV3d*, int);
    RwFrame*  (*FrameRotate)(RwFrame*, const RwV3d*, float, int);
    RwFrame*  (*FrameScale)(RwFrame*, const RwV3d*, int);
    RwFrame*  (*FrameUpdateObjects)(RwFrame*);
    RpGeometry* (*GeometryForAllMaterials)(RpGeometry*, RpMaterial* (*)(RpMaterial*, void*), void*);
    RpMaterial* (*MaterialSetTexture)(RpMaterial*, RwTexture*);
    RwImage*  (*PngRead)(const char*);
    RwImage*  (*ImageFindRasterFormat)(RwImage*, int, int*, int*, int*, int*);
    RwRaster* (*RasterCreate)(int, int, int, int);
    RwRaster* (*RasterSetFromImage)(RwRaster*, RwImage*);
    RwTexture*(*TextureCreate)(RwRaster*);
    int       (*ImageDestroy)(RwImage*);
    RwFrame*  (*GetFrameFromId)(RpClump*, int);          // CClumpModelInfo::GetFrameFromId (static)
};
extern RwApi RW;
bool RwInit();
// RW 3.7 layouts on 64-bit: RpClump->object.parent @8 ; RpAtomic->geometry @48
static inline RwFrame*    ClumpFrame(RpClump* c)    { return *(RwFrame**)((char*)c + 8); }
static inline RpGeometry* AtomicGeometry(RpAtomic* a) { return *(RpGeometry**)((char*)a + 48); }

// ---- game helpers (peds.cpp) ----
void*       Sym(const char* name);                 // dlsym in libGTASA.so
bool        SafeRead(const void* p, size_t n);
void*       PedFromHandle(int handle);             // CPools::GetPed
bool        PedPos(void* ped, float out[3]);
void*       PedClump(void* ped);                   // CPed::m_pRwObject (offset calibrated at runtime)
std::string ResolvePath(const char* p);            // relative -> absolute (game files dir)
bool        HookPedVtable(int slot, void* hook, void** orig, const char* origSym);
enum { VT_DELETE_RW_OBJECT = 9, VT_RENDER = 19 }; // verified on libGTASA.so 2.10 arm64
void        RegisterOpcodes();                     // visual.cpp or plus.cpp
