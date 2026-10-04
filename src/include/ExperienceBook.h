#pragma once
#include "ChessBoard.h"
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct ExperienceSample {
    uint64_t positionHash;
    ChessMove move;
};

class ExperienceBook {
public:
    explicit ExperienceBook(const std::string& filePath = "");

    int GetMoveBonus(uint64_t positionHash, const ChessMove& move) const;
    void RecordGame(const std::vector<ExperienceSample>& samples, bool aiWon);
    size_t GetEntryCount() const;

private:
    struct Entry {
        ChessMove move;
        uint32_t wins = 0;
        uint32_t losses = 0;
    };

    static const size_t MAX_MEMORY_ENTRIES = 200000;
    static const uint64_t MAX_FILE_BYTES = 128ull * 1024ull * 1024ull;

    void Load();
    void Append(uint64_t hash, const Entry& entry);
    void Compact();
    Entry* Find(uint64_t hash, const ChessMove& move);
    const Entry* Find(uint64_t hash, const ChessMove& move) const;
    bool SameMove(const ChessMove& left, const ChessMove& right) const;
    std::string DefaultPath() const;

    std::string m_filePath;
    std::unordered_multimap<uint64_t, Entry> m_entries;
    mutable std::mutex m_mutex;
};
