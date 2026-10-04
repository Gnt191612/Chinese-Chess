#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <future>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <memory>
#include "AIEngine.h"
#include "ExperienceBook.h"
#include <fstream>
#include <iostream>
#include <array>
#include <map>
#include <cstring>

// 一次性旧版迁移工具：仅请求读权限，输出经过校验的棋局，不输出进程内存。
struct SavedBoard { int player, ended; std::array<int, 90> pieces; };
// 固定本次旧版布局，后续项目类增加字段不会改变本工具的读偏移。
// 仅用于2026-10-03更新前的旧程序，不能当作通用进程恢复器使用。
struct LegacyBoard {
    uintptr_t pieces[90]; bool ended; PieceColor player; uint64_t hash;
};
struct LegacyTurn { LegacyBoard board; size_t experienceCount; };
struct LegacyPrefix {
    LegacyBoard board;
    AIEngine ai;
    ExperienceBook experience;
    std::future<AIEngine::Move> future;
    std::vector<ExperienceSample> samples;
    std::vector<LegacyTurn> turns;
};
static uint64_t Mix(uint64_t value) {
    value += 0x9e3779b97f4a7c15ull;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ull;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebull;
    return value ^ (value >> 31);
}
static bool Read(HANDLE process, uintptr_t address, void* data, size_t size) {
    SIZE_T actual = 0;
    return ReadProcessMemory(process, reinterpret_cast<void*>(address), data, size, &actual)
        && actual == size;
}
static bool Board(HANDLE process, uintptr_t address, SavedBoard& result) {
    std::array<unsigned char, sizeof(LegacyBoard)> raw;
    if (!Read(process, address, raw.data(), raw.size())) return false;
    memcpy(&result.player, raw.data() + offsetof(LegacyBoard, player), 4);
    result.ended = raw[offsetof(LegacyBoard, ended)];
    if (result.player < 0 || result.player > 1 || result.ended > 1) return false;
    uint64_t hash = result.player == BLACK_P ? Mix(2000) : 0, recorded;
    memcpy(&recorded, raw.data() + offsetof(LegacyBoard, hash), 8);
    int generals[2] = {}, counts[14] = {}, total = 0;
    for (int x = 0; x < 9; ++x)
        for (int y = 0; y < 10; ++y) {
            uintptr_t pointer;
            memcpy(&pointer, raw.data() + (x * 10 + y) * sizeof(uintptr_t), sizeof(pointer));
            int type = -1;
            if (pointer) {
                int piece[4];
                if (pointer < 0x10000 || pointer % 8 || !Read(process, pointer, piece, 16)) return false;
                type = piece[0];
                if (type < 0 || type > 13 || piece[1] != (type >= 7 ? BLACK_P : RED_P) ||
                    piece[2] != x || piece[3] != y) return false;
                if (++counts[type] > (type % 7 == 6 ? 5 : type % 7 == 0 ? 1 : 2)) return false;
                if (type % 7 == 0) ++generals[type >= 7];
                ++total;
                hash ^= Mix(static_cast<uint64_t>(type + 1) * 90 + y * 9 + x);
            }
            result.pieces[y * 9 + x] = type;
        }
    return total >= 2 && total <= 32 && generals[0] == 1 && generals[1] == 1 && hash == recorded;
}
static void Write(std::ofstream& file, const SavedBoard& board) {
    file << board.player << ' ' << board.ended << '\n';
    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 9; ++x) file << board.pieces[y * 9 + x] << ' ';
        file << '\n';
    }
}
int main(int argc, char** argv) {
    if (argc != 3) return 1;
    const DWORD pid = static_cast<DWORD>(std::stoul(argv[1]));
    HANDLE process = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!process) return 2;
    wchar_t image[32768]; DWORD length = 32768;
    if (!QueryFullProcessImageNameW(process, 0, image, &length) ||
        std::wstring(image).find(L"中国象棋.exe") == std::wstring::npos) return 3;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 thread = {}; thread.dwSize = sizeof(thread);
    DWORD mainThread = 0; uint64_t earliest = UINT64_MAX;
    if (Thread32First(snapshot, &thread)) do {
        if (thread.th32OwnerProcessID != pid) continue;
        HANDLE handle = OpenThread(THREAD_QUERY_INFORMATION, FALSE, thread.th32ThreadID);
        FILETIME created, exited, kernel, user;
        if (handle && GetThreadTimes(handle, &created, &exited, &kernel, &user)) {
            const uint64_t time = (uint64_t(created.dwHighDateTime) << 32) | created.dwLowDateTime;
            if (time < earliest) { earliest = time; mainThread = thread.th32ThreadID; }
        }
        if (handle) CloseHandle(handle);
    } while (Thread32Next(snapshot, &thread));
    CloseHandle(snapshot);
    struct ThreadInfo { LONG status; void* teb; uintptr_t client[2], affinity; LONG priority, base; };
    using QueryThread = LONG(WINAPI*)(HANDLE, ULONG, void*, ULONG, ULONG*);
    auto query = reinterpret_cast<QueryThread>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),
        "NtQueryInformationThread"));
    HANDLE handle = OpenThread(THREAD_QUERY_INFORMATION, FALSE, mainThread);
    ThreadInfo info = {}; uintptr_t tib[3];
    if (!query || !handle || query(handle, 0, &info, sizeof(info), nullptr) < 0 ||
        !Read(process, reinterpret_cast<uintptr_t>(info.teb), tib, sizeof(tib))) return 4;
    CloseHandle(handle);
    const uintptr_t base = tib[1], limit = tib[2];
    if (base <= limit || base - limit > 2 * 1024 * 1024) return 5;
    std::vector<unsigned char> stack(base - limit);
    if (!Read(process, limit, stack.data(), stack.size())) return 6;
    std::vector<uintptr_t> matches;
    for (size_t offset = 0; offset + sizeof(LegacyBoard) < stack.size(); offset += 8) {
        if (stack[offset + offsetof(LegacyBoard, ended)] > 1) continue;
        int player; memcpy(&player, stack.data() + offset + offsetof(LegacyBoard, player), 4);
        if (player < 0 || player > 1) continue;
        int pointerCount = 0;
        for (int index = 0; index < 90; ++index) {
            uintptr_t pointer; memcpy(&pointer, stack.data() + offset + index * 8, 8);
            if (pointer && (pointer < 0x10000 || pointer % 8)) { pointerCount = -1; break; }
            if (pointer) ++pointerCount;
        }
        if (pointerCount < 2 || pointerCount > 32) continue;
        SavedBoard board;
        if (Board(process, limit + offset, board)) matches.push_back(limit + offset);
    }
    if (matches.size() != 1) { std::cout << "有效棋盘数量=" << matches.size() << '\n'; return 7; }
    const uintptr_t game = matches[0];
    SavedBoard current, verify;
    if (!Board(process, game, current)) return 8;
    uintptr_t history[3];
    if (!Read(process, game + offsetof(LegacyPrefix, turns), history, sizeof(history))) return 9;
    if (history[1] < history[0] || history[2] < history[1] ||
        (history[1] - history[0]) % sizeof(LegacyTurn) ||
        (history[1] - history[0]) / sizeof(LegacyTurn) > 10000) return 10;
    std::vector<SavedBoard> prior;
    for (uintptr_t address = history[0]; address < history[1]; address += sizeof(LegacyTurn)) {
        SavedBoard board;
        if (!Board(process, address, board)) return 11;
        prior.push_back(board);
    }
    if (!Board(process, game, verify) || verify.pieces != current.pieces ||
        verify.player != current.player) return 12;
    // 迁移文件独立创建，绝不覆盖已有备份。
    if (std::ifstream(argv[2]).good()) return 13;
    std::ofstream file(argv[2]);
    file << "XQRECOVER 1\n" << prior.size() << '\n';
    Write(file, current);
    for (const auto& board : prior) Write(file, board);
    file.close(); CloseHandle(process);
    if (!file) return 14;
    std::cout << "棋局备份完成，轮到=" << current.player << "，可悔轮数=" << prior.size() << '\n';
    return 0;
}
