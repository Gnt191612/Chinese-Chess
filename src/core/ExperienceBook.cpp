#include "ExperienceBook.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <windows.h>

namespace {
const uint32_t FILE_MAGIC = 0x45585143; // CQXE
const uint32_t FILE_VERSION = 1;

#pragma pack(push, 1)
struct FileHeader {
    uint32_t magic;
    uint32_t version;
};

struct DiskEntry {
    uint64_t hash;
    int8_t fromX, fromY, toX, toY;
    uint32_t wins;
    uint32_t losses;
};
#pragma pack(pop)
}

ExperienceBook::ExperienceBook(const std::string& filePath)
    : m_filePath(filePath.empty() ? DefaultPath() : filePath) {
    m_entries.reserve(MAX_MEMORY_ENTRIES);
    Load();
}

std::string ExperienceBook::DefaultPath() const {
    char executable[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, executable, MAX_PATH);
    std::string path(executable);
    size_t separator = path.find_last_of("\\/");
    return (separator == std::string::npos ? std::string() : path.substr(0, separator + 1))
        + "experience.dat";
}

bool ExperienceBook::SameMove(const ChessMove& left, const ChessMove& right) const {
    return left.fromX == right.fromX && left.fromY == right.fromY
        && left.toX == right.toX && left.toY == right.toY;
}

ExperienceBook::Entry* ExperienceBook::Find(uint64_t hash, const ChessMove& move) {
    auto range = m_entries.equal_range(hash);
    for (auto iterator = range.first; iterator != range.second; ++iterator)
        if (SameMove(iterator->second.move, move)) return &iterator->second;
    return nullptr;
}

const ExperienceBook::Entry* ExperienceBook::Find(uint64_t hash, const ChessMove& move) const {
    auto range = m_entries.equal_range(hash);
    for (auto iterator = range.first; iterator != range.second; ++iterator)
        if (SameMove(iterator->second.move, move)) return &iterator->second;
    return nullptr;
}

int ExperienceBook::GetMoveBonus(uint64_t positionHash, const ChessMove& move) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const Entry* entry = Find(positionHash, move);
    if (!entry || entry->wins + entry->losses < 2) return 0;
    int balance = static_cast<int>(entry->wins) - static_cast<int>(entry->losses);
    return (std::max)(-50000, (std::min)(50000, balance * 2000));
}

void ExperienceBook::RecordGame(const std::vector<ExperienceSample>& samples, bool aiWon) {
    if (samples.empty()) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const ExperienceSample& sample : samples) {
        Entry* entry = Find(sample.positionHash, sample.move);
        if (!entry) {
            if (m_entries.size() >= MAX_MEMORY_ENTRIES) break;
            Entry created;
            created.move = sample.move;
            entry = &m_entries.emplace(sample.positionHash, created)->second;
        }
        if (aiWon) ++entry->wins;
        else ++entry->losses;
        Append(sample.positionHash, *entry);
    }

    std::ifstream file(m_filePath, std::ios::binary | std::ios::ate);
    bool needsCompaction = file && static_cast<uint64_t>(file.tellg()) >= MAX_FILE_BYTES;
    file.close();
    if (needsCompaction) Compact();
}

void ExperienceBook::Load() {
    std::ifstream file(m_filePath, std::ios::binary);
    FileHeader header = {};
    if (!file.read(reinterpret_cast<char*>(&header), sizeof(header))
        || header.magic != FILE_MAGIC || header.version != FILE_VERSION) return;

    DiskEntry disk = {};
    while (file.read(reinterpret_cast<char*>(&disk), sizeof(disk))) {
        ChessMove move = { disk.fromX, disk.fromY, disk.toX, disk.toY };
        Entry* entry = Find(disk.hash, move);
        if (!entry) {
            if (m_entries.size() >= MAX_MEMORY_ENTRIES) continue;
            Entry created;
            created.move = move;
            entry = &m_entries.emplace(disk.hash, created)->second;
        }
        entry->wins = disk.wins;
        entry->losses = disk.losses;
    }
}

void ExperienceBook::Append(uint64_t hash, const Entry& entry) {
    bool needsHeader = false;
    {
        std::ifstream existing(m_filePath, std::ios::binary | std::ios::ate);
        needsHeader = !existing || existing.tellg() < static_cast<std::streamoff>(sizeof(FileHeader));
    }
    std::ofstream file(m_filePath, std::ios::binary | std::ios::app);
    if (!file) return;
    if (needsHeader) {
        FileHeader header = { FILE_MAGIC, FILE_VERSION };
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    }
    DiskEntry disk = {
        hash,
        static_cast<int8_t>(entry.move.fromX), static_cast<int8_t>(entry.move.fromY),
        static_cast<int8_t>(entry.move.toX), static_cast<int8_t>(entry.move.toY),
        entry.wins, entry.losses
    };
    file.write(reinterpret_cast<const char*>(&disk), sizeof(disk));
}

void ExperienceBook::Compact() {
    std::string temporary = m_filePath + ".tmp";
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    if (!file) return;
    FileHeader header = { FILE_MAGIC, FILE_VERSION };
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    for (const auto& item : m_entries) {
        const Entry& entry = item.second;
        DiskEntry disk = {
            item.first,
            static_cast<int8_t>(entry.move.fromX), static_cast<int8_t>(entry.move.fromY),
            static_cast<int8_t>(entry.move.toX), static_cast<int8_t>(entry.move.toY),
            entry.wins, entry.losses
        };
        file.write(reinterpret_cast<const char*>(&disk), sizeof(disk));
    }
    file.close();
    std::remove(m_filePath.c_str());
    std::rename(temporary.c_str(), m_filePath.c_str());
}

size_t ExperienceBook::GetEntryCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries.size();
}
