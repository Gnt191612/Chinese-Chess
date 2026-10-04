#pragma once
#include <windows.h>
#include <mmsystem.h>
#include <cmath>
#include <cstdint>
#include <vector>
#pragma comment(lib, "winmm.lib")

// 自行合成的PCM短音效，内存缓冲在异步播放期间保持有效。
namespace SoundEffects {
inline std::vector<char> MakeWave(int event) {
    constexpr int rate = 22050;
    const bool capture = (event & 2) != 0;
    const bool check = (event & 4) != 0;
    const bool victory = event == 8;
    const int count = rate * (victory ? 1400 : check ? 640 : capture ? 280 : 110) / 1000;
    std::vector<char> wave(44 + count * 2);
    auto number = [&wave](int offset, uint32_t value, int bytes) {
        for (int index = 0; index < bytes; ++index)
            wave[offset + index] = static_cast<char>((value >> (8 * index)) & 255);
    };
    const char* riff = "RIFF", *wav = "WAVEfmt ", *data = "data";
    for (int index = 0; index < 4; ++index) {
        wave[index] = riff[index];
        wave[36 + index] = data[index];
    }
    for (int index = 0; index < 8; ++index) wave[8 + index] = wav[index];
    number(4, 36 + count * 2, 4);
    number(16, 16, 4);
    number(20, 1, 2);
    number(22, 1, 2);
    number(24, rate, 4);
    number(28, rate * 2, 4);
    number(32, 2, 2);
    number(34, 16, 2);
    number(40, count * 2, 4);
    uint32_t noise = 137;
    for (int index = 0; index < count; ++index) {
        const double time = static_cast<double>(index) / rate;
        noise = noise * 1664525u + 1013904223u;
        const double random = (noise & 65535) / 32767.5 - 1.0;
        const double frequency = capture ? 430.0 : 680.0;
        double sample = 0.32 * std::exp(-time * (capture ? 32 : 48)) *
            (0.7 * std::sin(6.28318530718 * frequency * time) + 0.3 * random);
        if (capture && time >= 0.055) {
            const double local = time - 0.055;
            sample += 0.14 * std::exp(-local * 28) * std::sin(6.28318530718 * 230 * local);
        }
        if (victory) {
            const double notes[5] = {523.25, 659.25, 783.99, 987.77, 1046.50};
            const int noteIndex = static_cast<int>(time / 0.28);
            const double local = time - noteIndex * 0.28;
            sample = 0.20 * (std::sin(6.28318530718 * notes[noteIndex] * local) +
                0.32 * std::sin(6.28318530718 * notes[noteIndex] * 2 * local)) *
                std::sin(3.14159265359 * local / 0.28);
        }
        if (check && time >= 0.16) {
            const double noteTime = time - 0.16;
            const int pulse = static_cast<int>(noteTime / 0.24);
            const double local = noteTime - pulse * 0.24;
            const double note = pulse == 0 ? 659.25 : 880.0;
            sample += 0.23 * std::sin(6.28318530718 * note * local) *
                std::sin(3.14159265359 * local / 0.24);
        }
        const auto pcm = static_cast<int16_t>(sample * 32767);
        number(44 + index * 2, static_cast<uint16_t>(pcm), 2);
    }
    return wave;
}

inline void Play(int event) {
    if (event <= 0 || event >= 9) return;
    static std::vector<char> sounds[9];
    if (sounds[event].empty()) sounds[event] = MakeWave(event);
    PlaySoundA(sounds[event].data(), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}
inline void Stop() { PlaySoundA(nullptr, nullptr, 0); }
}
