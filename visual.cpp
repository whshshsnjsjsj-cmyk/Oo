// CLEO_VisualObject replacement: opcodes 6000 6001 6002 6003 6004 6009 6011 6019 601F
#include "common.h"
#include <unordered_map>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cctype>

struct VO {
    int id; void* ped; RpClump* clump; RwFrame* root;
    int bone; RwV3d off, rot, scl; bool visible; int hideDead, hideWeapon, hideCar;
    std::string dff; int modelId;
};
static std::unordered_map<int, VO> g_vos;
static std::unordered_map<void*, std::vector<int>> g_byPed;
static int g_nextId = 1;

static std::string Norm(const char* s) {
    std::string r(s ? s : "");
    for (auto& c : r) { if (c == '\\') c = '/'; c = (char)tolower((unsigned char)c); }
    return r;
}

static void ApplyTransform(VO& v) {
    RwMatrix I = {};
    I.right = {1, 0, 0}; I.up = {0, 1, 0}; I.at = {0, 0, 1}; I.flags = 0x20003; // orthonormal + identity
    RW.FrameTransform(v.root, &I, RW_REPLACE);
    RW.FrameScale(v.root, &v.scl, RW_POSTCONCAT);
    const RwV3d ax = {1, 0, 0}, ay = {0, 1, 0}, az = {0, 0, 1};
    if (v.rot.x != 0) RW.FrameRotate(v.root, &ax, v.rot.x, RW_POSTCONCAT);
    if (v.rot.y != 0) RW.FrameRotate(v.root, &ay, v.rot.y, RW_POSTCONCAT);
    if (v.rot.z != 0) RW.FrameRotate(v.root, &az, v.rot.z, RW_POSTCONCAT);
    RW.FrameTranslate(v.root, &v.off, RW_POSTCONCAT);
    RW.FrameUpdateObjects(v.root);
}

static RpClump* ClumpFromDff(const char* path) {
    static std::unordered_map<std::string, std::vector<uint8_t>> cache;
    std::string abs = ResolvePath(path);
    auto it = cache.find(abs);
    if (it == cache.end()) {
        std::ifstream f(abs, std::ios::binary);
        if (!f) { FileLog("DFF not found: %s", abs.c_str()); return nullptr; }
        std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        it = cache.emplace(abs, std::move(buf)).first;
    }
    RwMemory mem = { it->second.data(), (uint32_t)it->second.size() };
    RwStream* s = RW.StreamOpen(RW_STREAM_MEMORY, RW_ACCESS_READ, &mem);
    if (!s) return nullptr;
    RpClump* c = nullptr;
    if (RW.StreamFindChunk(s, RW_ID_CLUMP, nullptr, nullptr)) c = RW.ClumpStreamRead(s);
    RW.StreamClose(s, nullptr);
    if (!c) FileLog("bad DFF: %s", abs.c_str());
    return c;
}

static RpClump* ClumpFromModelId(int id) {   // model must already be loaded (scripts do load_model first)
    void** tab = (void**)Sym("_ZN10CModelInfo16ms_modelInfoPtrsE");
    auto create = (RpClump * (*)(void*))Sym("_ZN15CClumpModelInfo14CreateInstanceEv");
    if (!tab || !create || id < 0 || id > 20000 || !tab[id]) return nullptr;
    return create(tab[id]);
}

static int Create(void* ped, const char* dff, int modelId, int bone, RwV3d off, RwV3d rot) {
    if (!ped) return -1;
    void* pc = PedClump(ped);
    if (!pc) return -1;
    RwFrame* boneFrame = RW.GetFrameFromId((RpClump*)pc, bone);
    if (!boneFrame) return -1;
    RpClump* c = dff ? ClumpFromDff(dff) : ClumpFromModelId(modelId);
    if (!c) return -1;
    RwFrame* root = ClumpFrame(c);
    if (!root) { RW.ClumpDestroy(c); return -1; }
    VO v = {}; v.id = g_nextId++; v.ped = ped; v.clump = c; v.root = root; v.bone = bone;
    v.off = off; v.rot = rot; v.scl = {1, 1, 1}; v.visible = true; v.dff = dff ? Norm(dff) : ""; v.modelId = dff ? -1 : modelId;
    RW.FrameAddChild(boneFrame, root);
    ApplyTransform(v);
    g_byPed[ped].push_back(v.id);
    g_vos[v.id] = v;
    return v.id;
}

static void Remove(int id) {
    auto it = g_vos.find(id);
    if (it == g_vos.end()) return;
    VO& v = it->second;
    RW.FrameRemoveChild(v.root);       // detach from the ped's skeleton first
    RW.ClumpDestroy(v.clump);
    auto& lst = g_byPed[v.ped];
    lst.erase(std::remove(lst.begin(), lst.end(), id), lst.end());
    if (lst.empty()) g_byPed.erase(v.ped);
    g_vos.erase(it);
}
static void RemoveAllFromPed(void* ped) {
    auto it = g_byPed.find(ped);
    if (it == g_byPed.end()) return;
    std::vector<int> ids = it->second;
    for (int id : ids) Remove(id);
}

// ---- texture from PNG (cached) ----
static RwTexture* PngTexture(const char* path) {
    static std::unordered_map<std::string, RwTexture*> cache;
    std::string abs = ResolvePath(path);
    auto it = cache.find(abs);
    if (it != cache.end()) return it->second;
    RwTexture* t = nullptr;
    if (RwImage* img = RW.PngRead(abs.c_str())) {
        int w = 0, h = 0, d = 0, fmt = 0;
        RW.ImageFindRasterFormat(img, RW_RASTER_TEXTURE, &w, &h, &d, &fmt);
        if (RwRaster* r = RW.RasterCreate(w, h, d, fmt)) { RW.RasterSetFromImage(r, img); t = RW.TextureCreate(r); }
        RW.ImageDestroy(img);
    } else FileLog("PNG load failed: %s", abs.c_str());
    cache[abs] = t;
    return t;
}
struct TexCtx { int idx, cur; RwTexture* tex; };
static RpMaterial* MatCb(RpMaterial* m, void* d) { auto* c = (TexCtx*)d; if (c->cur++ == c->idx) RW.MaterialSetTexture(m, c->tex); return m; }
static RpAtomic* AtomCb(RpAtomic* a, void* d) {
    if (RpGeometry* g = AtomicGeometry(a)) { ((TexCtx*)d)->cur = 0; RW.GeometryForAllMaterials(g, MatCb, d); }
    return a;
}

// ---- render / lifetime hooks (CPed vtable) ----
static void (*orig_Render)(void*) = nullptr;
static void (*orig_Delete)(void*) = nullptr;
static void hook_Render(void* ped) {
    orig_Render(ped);
    auto it = g_byPed.find(ped);
    if (it == g_byPed.end()) return;
    for (int id : it->second) {
        VO& v = g_vos[id];
        if (!v.visible) continue;
        RW.FrameUpdateObjects(v.root);
        RW.ClumpRender(v.clump);
    }
}
static void hook_Delete(void* ped) { RemoveAllFromPed(ped); orig_Delete(ped); }

// ---- opcodes (operand order decoded from the compiled .csa files) ----
OPC(CREATE_FROM_DFF) {            // 6000: ped, path, bone, off xyz, rot xyz -> id
    int ped = ReadInt(h); char path[260]; ReadStr(h, path, sizeof path); int bone = ReadInt(h);
    RwV3d off = { ReadFloat(h), ReadFloat(h), ReadFloat(h) };
    RwV3d rot = { ReadFloat(h), ReadFloat(h), ReadFloat(h) };
    OutInt(h, Create(PedFromHandle(ped), path, -1, bone, off, rot));
}
OPC(CREATE_FROM_MODEL_ID) {       // 6009: ped, model, bone, off xyz, rot xyz -> id
    int ped = ReadInt(h); int model = ReadInt(h); int bone = ReadInt(h);
    RwV3d off = { ReadFloat(h), ReadFloat(h), ReadFloat(h) };
    RwV3d rot = { ReadFloat(h), ReadFloat(h), ReadFloat(h) };
    OutInt(h, Create(PedFromHandle(ped), nullptr, model, bone, off, rot));
}
OPC(SET_SCALE) {                  // 6001: obj, sx, sy, sz
    int id = ReadInt(h); RwV3d s = { ReadFloat(h), ReadFloat(h), ReadFloat(h) };
    auto it = g_vos.find(id); if (it != g_vos.end()) { it->second.scl = s; ApplyTransform(it->second); }
}
OPC(DELETE_VO)   { Remove(ReadInt(h)); }                       // 6002
OPC(AUTO_HIDE) {                  // 6003: obj, dead, weapon, car  (stored; NOT enforced yet, see README)
    int id = ReadInt(h); int d = ReadInt(h), w = ReadInt(h), c = ReadInt(h);
    auto it = g_vos.find(id); if (it != g_vos.end()) { it->second.hideDead = d; it->second.hideWeapon = w; it->second.hideCar = c; }
}
OPC(SET_VISIBLE) {                // 6004: obj, visible
    int id = ReadInt(h); int vis = ReadInt(h);
    auto it = g_vos.find(id); if (it != g_vos.end()) it->second.visible = vis != 0;
}
OPC(IS_VISIBLE) { auto it = g_vos.find(ReadInt(h)); Cond(h, it != g_vos.end() && it->second.visible); }  // 6011
OPC(IS_DFF) {                     // 6019: obj, path
    int id = ReadInt(h); char path[260]; ReadStr(h, path, sizeof path);
    auto it = g_vos.find(id); Cond(h, it != g_vos.end() && it->second.dff == Norm(path));
}
OPC(IS_MODEL_ID) {                // 6018: obj, model id
    int id = ReadInt(h); int m = ReadInt(h);
    auto it = g_vos.find(id); Cond(h, it != g_vos.end() && it->second.modelId == m);
}
OPC(SET_TEXTURE_PNG) {            // 601F: obj, path, material index
    int id = ReadInt(h); char path[260]; ReadStr(h, path, sizeof path); int mat = ReadInt(h);
    auto it = g_vos.find(id); if (it == g_vos.end()) return;
    RwTexture* t = PngTexture(path); if (!t) return;
    TexCtx ctx = { mat, 0, t };
    RW.ClumpForAllAtomics(it->second.clump, AtomCb, &ctx);
}

void RegisterOpcodes() {
    if (!RwInit()) { FileLog("RenderWare symbols missing - VisualObject disabled"); return; }
    if (!HookPedVtable(VT_RENDER, (void*)hook_Render, (void**)&orig_Render, "_ZN4CPed6RenderEv") ||
        !HookPedVtable(VT_DELETE_RW_OBJECT, (void*)hook_Delete, (void**)&orig_Delete, "_ZN4CPed14DeleteRwObjectEv")) {
        FileLog("could not hook CPed - VisualObject disabled"); return;
    }
    Reg(0x6000, CREATE_FROM_DFF);   Reg(0x6001, SET_SCALE);
    Reg(0x6002, DELETE_VO);         Reg(0x6003, AUTO_HIDE);
    Reg(0x6004, SET_VISIBLE);       Reg(0x6009, CREATE_FROM_MODEL_ID);
    Reg(0x6011, IS_VISIBLE);        Reg(0x6018, IS_MODEL_ID);        Reg(0x6019, IS_DFF);
    Reg(0x601F, SET_TEXTURE_PNG);
}
