// CLEOPlus subset used by the scripts: 0E4D 0E0B 0E0A 0EE4 0EEA 0D59 0E40 0D4E 0E28 0EC0
#include "common.h"
#include <random>
#include <unordered_map>

static std::mt19937 g_rng{ std::random_device{}() };

static bool Dist(void* a, float x, float y, float z, float r) {
    float p[3]; if (!PedPos(a, p)) return false;
    float dx = p[0] - x, dy = p[1] - y, dz = p[2] - z;
    return dx * dx + dy * dy + dz * dz <= r * r;
}

OPC(RANDOM_PERCENT) { int p = ReadInt(h); Cond(h, (int)(g_rng() % 100) < p); }            // 0E4D
OPC(MARK_CHAR_AS_NEEDED) { ReadInt(h); }                                                  // 0E0B (no-op for now)
OPC(IS_CHAR_SCRIPT_CONTROLLED) { ReadInt(h); Cond(h, false); }                            // 0E0A (not implemented)
OPC(DIST_CHAR_CHAR) {                                                                     // 0EE4
    int a = ReadInt(h), b = ReadInt(h); float r = ReadFloat(h);
    float p[3]; void* pb = PedFromHandle(b);
    Cond(h, PedPos(pb, p) && Dist(PedFromHandle(a), p[0], p[1], p[2], r));
}
OPC(DIST_CHAR_COORDS) {                                                                   // 0EEA
    int a = ReadInt(h); float x = ReadFloat(h), y = ReadFloat(h), z = ReadFloat(h), r = ReadFloat(h);
    Cond(h, Dist(PedFromHandle(a), x, y, z, r));
}
OPC(GET_WEATHER) { short* w = (short*)Sym("_ZN8CWeather14NewWeatherTypeE"); OutInt(h, w ? *w : 0); }   // 0D59
OPC(GET_HOUR)    { uint8_t* c = (uint8_t*)Sym("_ZN6CClock18ms_nGameClockHoursE"); OutInt(h, c ? *c : 12); } // 0E40

// The scripts use byte 0x11 of the ped struct as a private "already processed" flag. With 64-bit
// pointers the script cannot dereference ped structs, so that flag is emulated per script value.
static std::unordered_map<int, int> g_flag;
OPC(READ_STRUCT) {                                                                        // 0D4E: ptr, off, size -> value
    int ptr = ReadInt(h), off = ReadInt(h), size = ReadInt(h); (void)size;
    auto it = g_flag.find(ptr);
    OutInt(h, (off == 0x11 && it != g_flag.end()) ? it->second : 0);
}
OPC(WRITE_STRUCT) {                                                                       // 0E28: ptr, off, size, value
    int ptr = ReadInt(h), off = ReadInt(h), size = ReadInt(h), val = ReadInt(h); (void)size;
    if (off == 0x11) g_flag[ptr] = val;
    if (g_flag.size() > 4096) g_flag.clear();
}
OPC(ADD_FX_PARTICLE) {                                                                    // 0EC0: 14 operands, no-op
    for (int i = 0; i < 14; i++) cleo->ReadParam(h);
}

static void (*orig_Delete)(void*) = nullptr;
static void hook_Delete(void* ped) { g_flag.erase((int)(uintptr_t)ped); orig_Delete(ped); }

void RegisterOpcodes() {
    HookPedVtable(VT_DELETE_RW_OBJECT, (void*)hook_Delete, (void**)&orig_Delete, "_ZN4CPed14DeleteRwObjectEv");
    cleo->RegisterOpcode(0x0E4D, RANDOM_PERCENT);        cleo->RegisterOpcode(0x0E0B, MARK_CHAR_AS_NEEDED);
    cleo->RegisterOpcode(0x0E0A, IS_CHAR_SCRIPT_CONTROLLED);
    cleo->RegisterOpcode(0x0EE4, DIST_CHAR_CHAR);        cleo->RegisterOpcode(0x0EEA, DIST_CHAR_COORDS);
    cleo->RegisterOpcode(0x0D59, GET_WEATHER);           cleo->RegisterOpcode(0x0E40, GET_HOUR);
    cleo->RegisterOpcode(0x0D4E, READ_STRUCT);           cleo->RegisterOpcode(0x0E28, WRITE_STRUCT);
    cleo->RegisterOpcode(0x0EC0, ADD_FX_PARTICLE);
}
