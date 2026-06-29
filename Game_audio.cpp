#include "Game.h"
#include "Game_constants.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <vector>
using namespace std;
using namespace bagel;

namespace ci
{
    void Game::play_sfx(int type) const
    {
        if (!audio_stream || _muted) return;
        // Anthems (types 7-18) bypass queue throttle and clear the stream first.
        // Type 19 (WRONG!) is a priority in-game SFX: skip throttle, but don't clear stream.
        if (type >= 7 && type <= 18) {
            SDL_ClearAudioStream(audio_stream);
        } else if (type != 19) {
            if (SDL_GetAudioStreamQueued(audio_stream) > 44100 * 150 / 1000 * (int)sizeof(Sint16)) return;
        }

        std::vector<Sint16> buf;
        const int SR = 44100;
        auto tone = [&](float freq, float dur, float vol = 0.28f, float endVol = 0.0f) {
            int n = (int)(SR * dur); buf.resize(n);
            for (int i = 0; i < n; i++) {
                float fade = vol + (endVol - vol) * ((float)i / n);
                buf[i] = (Sint16)(32767 * fade * std::sin(2 * M_PI * freq * (float)i / SR));
            }
        };
        switch (type) {
            case 0: tone(880.f, 0.055f, 0.22f, 0.0f); break;           // shoot
            case 1: tone(520.f, 0.08f,  0.20f, 0.0f); break;           // enemy hit
            case 2: {   // enemy death: sweep down
                int n = (int)(SR * 0.14f); buf.resize(n);
                for (int i = 0; i < n; i++) {
                    float f = 700.f - 550.f * ((float)i / n);
                    float v = 0.26f * (1.f - (float)i / n);
                    buf[i] = (Sint16)(32767 * v * std::sin(2 * M_PI * f * (float)i / SR));
                }
                break;
            }
            case 3: tone(90.f,  0.22f, 0.38f, 0.05f); break;           // player hit
            case 4: {   // pickup / level clear: jingle
                int n = (int)(SR * 0.35f); buf.resize(n);
                const float notes[] = {330.f, 440.f, 550.f, 660.f};
                for (int i = 0; i < n; i++) {
                    int ni = std::min((int)(4.f * i / n), 3);
                    float v = 0.22f * (1.f - (float)i / n);
                    buf[i] = (Sint16)(32767 * v * std::sin(2 * M_PI * notes[ni] * (float)i / SR));
                }
                break;
            }
            case 5: {   // game over: descend
                int n = (int)(SR * 0.9f); buf.resize(n);
                for (int i = 0; i < n; i++) {
                    float f = 280.f - 200.f * ((float)i / n);
                    float v = 0.30f * (1.f - (float)i / n);
                    buf[i] = (Sint16)(32767 * v * std::sin(2 * M_PI * f * (float)i / SR));
                }
                break;
            }
            case 6: {   // bullet-time activation: whoosh
                int n = (int)(SR * 0.18f); buf.resize(n);
                for (int i = 0; i < n; i++) {
                    float f = 200.f + 400.f * ((float)i / n);
                    float v = 0.20f * std::sin(M_PI * (float)i / n);
                    buf[i] = (Sint16)(32767 * v * std::sin(2 * M_PI * f * (float)i / SR));
                }
                break;
            }
            case 19: {  // "WRONG!" — low blaring tone when wall blocks a bullet
                tone(220.f, 0.28f, 0.40f, 0.05f);
                break;
            }
            case 20: {  // wall-spawn thud
                tone(110.f, 0.18f, 0.35f, 0.0f);
                break;
            }
            case 7:  case 8:  case 9:  case 10:
            case 11: case 12: case 13: case 14:
            case 15: case 16: case 17: case 18: {
                static const char* wav_files[] = {
                    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                    "res/hatikva.wav",         //  7
                    "res/ssb.wav",             //  8
                    "res/ani_maamin.wav",       //  9
                    "res/ukrainian_anthem.wav", // 10
                    "res/russian_anthem.wav",   // 11
                    "res/slim_shady.wav",       // 12
                    "res/like_a_virgin.wav",    // 13
                    "res/thriller.wav",         // 14
                    "res/ba_ma.wav",            // 15 Sara
                    "res/moscow.wav",           // 16 Stalin
                    "res/ma_sheat_ohevet.wav",  // 17 Yoamashit
                    "res/f_the_police.wav",     // 18 Obama
                };
                SDL_AudioSpec wavSpec{};
                Uint8* wavBuf = nullptr;
                Uint32 wavLen = 0;
                if (SDL_LoadWAV(wav_files[type], &wavSpec, &wavBuf, &wavLen) && wavBuf) {
                    // Auto-convert to stream format (mono S16 44100) so stereo files work too
                    const SDL_AudioSpec streamSpec{ SDL_AUDIO_S16, 1, 44100 };
                    Uint8* convBuf = nullptr;
                    int    convLen = 0;
                    if (SDL_ConvertAudioSamples(&wavSpec, wavBuf, (int)wavLen,
                                               &streamSpec, &convBuf, &convLen) && convBuf) {
                        SDL_PutAudioStreamData(audio_stream, convBuf, convLen);
                        SDL_free(convBuf);
                    }
                    SDL_free(wavBuf);
                }
                return;
            }
            default: return;
        }
        if (!buf.empty())
            SDL_PutAudioStreamData(audio_stream, buf.data(), (int)(buf.size() * sizeof(Sint16)));
    }

    void Game::sound_system() const
    {
        static const Mask mask = MaskBuilder().set<SoundEvent>().build();
        static const int  q    = World::createQuery(mask);
        for (Entity e = Entity::firstQ(q); !e.eofQ(q); e.nextQ(q)) {
            play_sfx(e.get<SoundEvent>().type);
            e.destroy();
        }
    }

} // namespace ci
