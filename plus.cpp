// CLEOPlus subset used by the scripts: 0E4D 0E0B 0E0A 0EE4 0EEA 0D59 0E40 0D4E 0E28 0EC0
#include "common.h"
#include <random>
#include <unordered_map>
#include <fstream>
#include <string>
#include <cctype>
#include <cstdlib>

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


// 0AF0: path, section, key -> int (IniFiles plugin has no arm64 build, so it is provided here)
static std::string Trim(std::string s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
static std::string Lower(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }
static bool IniReadInt(const char* path, const char* section, const char* key, int& out) {
    std::ifstream f(ResolvePath(path));
    if (!f) return false;
    std::string line, cur, sec = Lower(section), k = Lower(key);
    while (std::getline(f, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[') { size_t e = line.find(']'); cur = Lower(Trim(line.substr(1, e == std::string::npos ? std::string::npos : e - 1))); continue; }
        size_t eq = line.find('=');
        if (eq == std::string::npos || cur != sec) continue;
        if (Lower(Trim(line.substr(0, eq))) != k) continue;
        std::string v = line.substr(eq + 1);
        size_t sc = v.find(';'); if (sc != std::string::npos) v = v.substr(0, sc);
        out = atoi(Trim(v).c_str());
        return true;
    }
    return false;
}
OPC(READ_INT_FROM_INI) {
    char path[260], sec[128], key[128];
    ReadStr(h, path, sizeof path); ReadStr(h, sec, sizeof sec); ReadStr(h, key, sizeof key);
    int v = 0; bool ok = IniReadInt(path, sec, key, v);
    OutInt(h, v); Cond(h, ok);
}

static void (*orig_Delete)(void*) = nullptr;
static void hook_Delete(void* ped) { g_flag.erase((int)(uintptr_t)ped); orig_Delete(ped); }

void RegisterOpcodes() {
    HookPedVtable(VT_DELETE_RW_OBJECT, (void*)hook_Delete, (void**)&orig_Delete, "_ZN4CPed14DeleteRwObjectEv");
    Reg(0x0E4D, RANDOM_PERCENT);        Reg(0x0E0B, MARK_CHAR_AS_NEEDED);
    Reg(0x0E0A, IS_CHAR_SCRIPT_CONTROLLED);
    Reg(0x0EE4, DIST_CHAR_CHAR);        Reg(0x0EEA, DIST_CHAR_COORDS);
    Reg(0x0D59, GET_WEATHER);           Reg(0x0E40, GET_HOUR);
    Reg(0x0D4E, READ_STRUCT);           Reg(0x0E28, WRITE_STRUCT);
    Reg(0x0EC0, ADD_FX_PARTICLE);
    Reg(0x0AF0, READ_INT_FROM_INI);
}
