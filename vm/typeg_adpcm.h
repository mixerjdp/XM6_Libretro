// Integer ADPCM signal path recovered from XM6 TypeG x64 (see tests/provenance).
// No host API or FM dependency. All rings count stereo FRAMES, not scalar samples.
#ifndef XM6_TYPEG_ADPCM_H
#define XM6_TYPEG_ADPCM_H
#include <stdint.h>
#include <string.h>
#include <math.h>

namespace TypeGAdpcm {
enum { RingFrames = 65536, TraceFrames = 512 };
struct State {
    int32_t panpot, play, rec, active, started;
    uint32_t clock, ratio, speed, data;
    int32_t offset, sample, out, vol, enable, sound;
    uint32_t readpoint, writepoint, number;
    int32_t wait;
    uint32_t sync_cnt, sync_rate, sync_step;
    int32_t interp, clock_select, analog, lpf;
    int32_t repeated, stop_count;
    uint32_t period;
    int32_t smooth_output, smooth_input, reserved[3];
    int32_t recursive, input, previous_input, history[12], history_index;
    int32_t trace[TraceFrames], trace_index;
};
struct Tables { int32_t diff[49 * 16], encode[49 * 256]; };
static_assert(sizeof(State) == 2252, "TypeG state must have fixed-width layout");

inline int32_t add(int32_t a, int32_t b) { return int32_t(uint32_t(a) + uint32_t(b)); }
inline int32_t mul(int32_t a, int32_t b) { return int32_t(uint32_t(a) * uint32_t(b)); }
inline int32_t sar(int32_t a, unsigned n) {
    return a >= 0 ? int32_t(uint32_t(a) >> n) : int32_t(~(uint32_t(~a) >> n));
}
inline int32_t clamp(int32_t a, int32_t lo, int32_t hi) { return a < lo ? lo : a > hi ? hi : a; }

inline void MakeTables(Tables &t) {
    for (int step = 0; step < 49; ++step) {
        const int base = int(floor(16.0 * pow(1.1, step)));
        for (int nibble = 0; nibble < 16; ++nibble) {
            int d = (base >> 3) + ((nibble & 4) ? base : 0)
                + ((nibble & 2) ? (base >> 1) : 0) + ((nibble & 1) ? (base >> 2) : 0);
            t.diff[step * 16 + nibble] = (nibble & 8) ? -d : d;
        }
        for (int byte = 0; byte < 256; ++byte) {
            int d = byte < 128 ? byte : byte - 256;
            int n = d < 0 ? 8 : 0;
            if (d < 0) d = -d;
            if (d >= base) { d -= base; n |= 4; }
            if (d >= (base >> 1)) { d -= base >> 1; n |= 2; }
            if (d >= (base >> 2)) n |= 1;
            t.encode[step * 256 + byte] = n;
        }
    }
}
inline void ResetBuffer(State &s, int32_t *ring, uint32_t rate) {
    s.number = s.readpoint = s.writepoint = s.sync_cnt = 0;
    s.wait = 0;
    s.sync_rate = rate / 100;
    s.sync_step = s.sync_rate ? 0x9c4000 / s.sync_rate : 0;
    if (ring) ring[0] = ring[1] = 0;
    s.repeated = 0;
    s.smooth_output = s.smooth_input = s.recursive = s.input = s.previous_input = 0;
    memset(s.reserved, 0, sizeof(s.reserved));
    memset(s.history, 0, sizeof(s.history));
    s.history_index = 0;
    memset(s.trace, 0, sizeof(s.trace));
    s.trace_index = 0;
}
inline void CalcSpeed(State &s) {
    const uint32_t unit = 1024 / s.clock;
    s.speed = (4 - s.ratio) * unit;
    if (s.period < s.speed) {
        const uint32_t d = s.speed - s.period;
        s.period += d > (unit >> 1) ? (unit >> 1) : d;
    } else if (s.period > s.speed) {
        const uint32_t d = s.period - s.speed;
        s.period -= d > (unit >> 1) ? (unit >> 1) : d;
    }
}
inline int32_t Filter(State &s, int32_t sample) {
    static const int32_t coefs[6] = {0, -14472, 0, 16384, 0, -9568};
    s.input = sample;
    s.recursive = sar(add(mul(add(sample, int32_t(0u - uint32_t(s.previous_input))), 512), mul(s.recursive, 43)), 9);
    s.previous_input = sample;
    const int32_t feedback = sar(mul(s.recursive, 320), 8);
    int32_t filtered = sample;
    for (int i = 0; i < 6; ++i)
        filtered = add(filtered, sar(mul(s.history[s.history_index + i], coefs[i]), 16));
    s.history[s.history_index] = s.history[s.history_index + 6] = sample;
    if (++s.history_index == 6) s.history_index = 0;
    filtered = add(sar(mul(add(filtered, feedback), 471), 9), s.smooth_input) / 2;
    s.smooth_input = filtered;
    const int32_t output = add(filtered, s.smooth_output) / 2;
    s.smooth_output = add(output, s.smooth_output) / 2;
    return clamp(output, -32768, 32767);
}
inline void Trace(State &s, int32_t sample, uint32_t frames) {
    for (uint32_t i = 0; i < frames; ++i) {
        s.trace[s.trace_index] = sample;
        s.trace_index = (s.trace_index + 1) & 511;
    }
}
inline void Decode(State &s, const Tables &t, int32_t *ring, uint32_t nibble, uint32_t frames) {
    static const int next[16] = {-1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8};
    if (!s.enable) return;
    nibble &= 15;
    const int32_t delta = t.diff[s.offset * 16 + nibble];
    s.offset = clamp(s.offset + next[nibble], 0, 48);
    const int32_t decoded = clamp(sar(add(mul(s.sample, 248), mul(delta, 256)), 8), -2048, 2047);
    s.repeated = s.sample == decoded ? (s.repeated < 20 ? s.repeated + 1 : s.repeated) : 0;
    s.sample = decoded;
    if (!s.analog) s.out = decoded;
    else if (s.repeated < 20) s.out = int32_t(uint32_t(decoded) & ~3u);
    else if (s.out > 0) --s.out;
    else if (s.out < 0) ++s.out;
    const int32_t volume_sample = s.play ? sar(mul(s.out, s.vol), 9) : 0;
    for (uint32_t i = 0; i < frames; ++i) {
        const int32_t filtered = s.lpf ? Filter(s, s.panpot == 3 ? 0 : volume_sample) : volume_sample;
        ring[s.writepoint * 2] = (s.panpot == 0 || s.panpot == 1) ? filtered : 0;
        ring[s.writepoint * 2 + 1] = (s.panpot == 0 || s.panpot == 2) ? filtered : 0;
        s.writepoint = (s.writepoint + 1) & 65535;
    }
    Trace(s, s.panpot == 3 ? 0 : volume_sample, frames);
    s.number += frames;
    if (s.number > RingFrames) { s.readpoint = s.writepoint; s.number = RingFrames; }
}
inline uint32_t Encode(State &s, const Tables &t, const int32_t *ring, uint32_t fallback, uint32_t frames) {
    static const int next[16] = {-1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8};
    if (!s.enable) return fallback;
    uint32_t result = fallback;
    for (uint32_t i = 0; i < frames && s.number; ++i) {
        const uint32_t at = s.readpoint * 2;
        if (!i) {
            const int32_t input = clamp(sar(add(ring[at], ring[at + 1]), 8), -128, 127);
            result = t.encode[s.offset * 256 + (uint32_t(input - sar(s.sample, 4)) & 255)];
            s.sample = sar(add(mul(s.sample, 248), mul(t.diff[s.offset * 16 + result], 256)), 8);
            s.offset = clamp(s.offset + next[result], 0, 48);
            s.out = sar(mul(s.sample, s.vol), 9);
            Trace(s, s.out, frames);
        }
        s.readpoint = (s.readpoint + 1) & 65535;
        --s.number;
    }
    return result;
}

// Host capture uses the selected output rate. This is TypeG's original phase
// accumulator, including its one-write-per-input-frame behavior below 62.5kHz.
inline void Capture(State &s, int32_t *ring, const int16_t *stereo, uint32_t frames) {
    if (!s.active || !s.rec || !s.enable || !s.sound) return;
    for (uint32_t i = 0; i < frames; ++i) {
        s.sync_cnt += s.sync_step;
        if (s.sync_cnt > 0x3fff) {
            s.sync_cnt &= 0x3fff;
            ring[s.writepoint * 2] = stereo[i * 2];
            ring[s.writepoint * 2 + 1] = stereo[i * 2 + 1];
            s.writepoint = (s.writepoint + 1) & 65535;
            if (++s.number > RingFrames) { s.readpoint = s.writepoint; s.number = RingFrames; }
        }
    }
}

// Resample is the complete previously byte-matched TypeG routine, adapted only
// to this portable state and an explicit ring pointer. Added output wraps as x86.
inline void Resample(State &state_ref, int32_t *ring, uint32_t *buffer, int samples)
{
    State *state = &state_ref;
    int i, j, left, right, *output;
    uint32_t next;
    if (!state->enable || !state->sound || !state->play) {
        ResetBuffer(*state, ring, state->sync_rate * 100);
        return;
    }
    output = (int *)buffer;
    if (state->number <= 1) {
        left = ring[state->readpoint * 2];
        right = ring[state->readpoint * 2 + 1];
        for (i = samples; i > 0; --i) {
            *output = add(*output, left); ++output;
            *output = add(*output, right); ++output;
        }
        return;
    }
    if (state->interp) {
        if (state->sync_step > 0x4000) {
            for (i = samples; i > 0; --i) {
                while (state->sync_cnt < state->sync_step) {
                    if (state->number >= 2) {
                        ++state->readpoint;
                        state->readpoint &= 0xffff;
                        --state->number;
                    }
                    state->sync_cnt += 0x4000;
                }
                state->sync_cnt -= state->sync_step;
                if (state->number < 2) {
                    *output = add(*output, ring[state->readpoint * 2]); ++output;
                    *output = add(*output, ring[state->readpoint * 2 + 1]); ++output;
                } else {
                    next = (state->readpoint + 1) & 0xffff;
                    left = mul(ring[next * 2], state->sync_cnt);
                    right = mul(ring[state->readpoint * 2], 0x4000 - state->sync_cnt);
                    *output = add(*output, sar(add(left, right), 14)); ++output;
                    left = mul(ring[next * 2 + 1], state->sync_cnt);
                    right = mul(ring[state->readpoint * 2 + 1], 0x4000 - state->sync_cnt);
                    *output = add(*output, sar(add(left, right), 14)); ++output;
                }
            }
        } else {
            for (i = samples; i > 0; --i) {
                state->sync_cnt += state->sync_step;
                if (state->sync_cnt >= 0x4000) {
                    state->sync_cnt &= 0x3fff;
                    if (state->number >= 2) {
                        ++state->readpoint;
                        state->readpoint &= 0xffff;
                        --state->number;
                    }
                }
                if (state->number < 2) {
                    *output = add(*output, ring[state->readpoint * 2]); ++output;
                    *output = add(*output, ring[state->readpoint * 2 + 1]); ++output;
                } else {
                    next = (state->readpoint + 1) & 0xffff;
                    left = mul(ring[next * 2], state->sync_cnt);
                    right = mul(ring[state->readpoint * 2], 0x4000 - state->sync_cnt);
                    *output = add(*output, sar(add(left, right), 14)); ++output;
                    left = mul(ring[next * 2 + 1], state->sync_cnt);
                    right = mul(ring[state->readpoint * 2 + 1], 0x4000 - state->sync_cnt);
                    *output = add(*output, sar(add(left, right), 14)); ++output;
                }
            }
        }
    } else {
        if (state->sync_step > 0x4000) {
            for (i = samples; i > 0; --i) {
                *buffer++ += ring[state->readpoint * 2];
                *buffer++ += ring[state->readpoint * 2 + 1];
                while (state->sync_cnt < state->sync_step) {
                    ++state->readpoint;
                    state->readpoint &= 0xffff;
                    --state->number;
                    if (state->number <= 1) {
                        left = ring[state->readpoint * 2];
                        right = ring[state->readpoint * 2 + 1];
                        for (j = i - 1; j > 0; --j) {
                            *buffer++ += left;
                            *buffer++ += right;
                            state->sync_cnt += 0x4000;
                            if (state->sync_cnt >= state->sync_step) state->sync_cnt -= state->sync_step;
                        }
                        return;
                    }
                    state->sync_cnt += 0x4000;
                }
                state->sync_cnt -= state->sync_step;
            }
        } else {
            for (i = samples; i > 0; --i) {
                *buffer++ += ring[state->readpoint * 2];
                *buffer++ += ring[state->readpoint * 2 + 1];
                state->sync_cnt += state->sync_step;
                if (state->sync_cnt < 0x4000) continue;
                state->sync_cnt &= 0x3fff;
                if (state->number <= 1) {
                    left = ring[state->readpoint * 2];
                    right = ring[state->readpoint * 2 + 1];
                    for (j = i - 1; j > 0; --j) {
                        *buffer++ += left;
                        *buffer++ += right;
                        state->sync_cnt += state->sync_step;
                    }
                    state->sync_cnt &= 0x3fff;
                    return;
                }
                ++state->readpoint;
                state->readpoint &= 0xffff;
                --state->number;
            }
        }
    }
}

}
#endif
