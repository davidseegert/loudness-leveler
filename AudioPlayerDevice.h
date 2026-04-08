#pragma once

#include <vector>
#include <memory>
#include <atomic>
#include "AudioProcessor.h"
#include "config.h"
#include "miniaudio.h"

class AudioPlayerDevice {
public:
    // Lock-free segment data: flat, contiguous, cache-friendly
    struct SegmentEntry {
        float start;
        float end;
        float gain;
    };
    using SegmentSnapshot = std::vector<SegmentEntry>;

    AudioPlayerDevice(const std::vector<float> *pcmData, int sampleRate, int channels);
    ~AudioPlayerDevice();

    void dataCallback(void* pOutput, const void* pInput, ma_uint32 frameCount);

    long long currentSampleIndex() const { return m_currentIndex.load(std::memory_order_relaxed); }
    void seekToSample(long long sampleIndex);
    
    // Thread-safe setters (UI thread writes, RT thread reads via atomic)
    void setLimiterThreshold(float db) { m_limiterThresholdDb.store(db, std::memory_order_relaxed); }
    void setLowCutEnabled(bool enabled) { m_lowCutEnabled.store(enabled, std::memory_order_relaxed); }
    void setHighCutEnabled(bool enabled) { m_highCutEnabled.store(enabled, std::memory_order_relaxed); }
    void setMidCutEnabled(bool enabled) { m_midCutEnabled.store(enabled, std::memory_order_relaxed); }
    void setPhaseRotationEnabled(bool enabled) { m_phaseRotationEnabled.store(enabled, std::memory_order_relaxed); }
    void setEffectsEnabled(bool enabled) { m_effectsEnabled.store(enabled, std::memory_order_relaxed); }
    
    void setPostCompAmount(float amount) { m_postCompAmount.store(amount, std::memory_order_relaxed); }
    void setLimiterGain(float db) { m_limiterGainDb.store(db, std::memory_order_relaxed); }
    void setNoiseGateThreshold(float db) { m_noiseGateThresholdDb.store(db, std::memory_order_relaxed); }
    void setNoiseGateReduction(float db) { m_noiseGateReductionDb.store(db, std::memory_order_relaxed); }
    void setGainRiderInverted(bool inverted) { m_gainRiderInverted.store(inverted, std::memory_order_relaxed); }
    void setCompInverted(bool inverted) { m_compInverted.store(inverted, std::memory_order_relaxed); }
    void setNoiseGateEnabled(bool enabled) { m_noiseGateEnabled.store(enabled, std::memory_order_relaxed); }
    void setCompressorEnabled(bool enabled) { m_compressorEnabled.store(enabled, std::memory_order_relaxed); }
    void setLimiterEnabled(bool enabled) { m_limiterEnabled.store(enabled, std::memory_order_relaxed); }
    void setGainRiderEnabled(bool enabled) { m_gainRiderEnabled.store(enabled, std::memory_order_relaxed); }
    
    void setFilterSettings(float lc, float hc, float mcF, float mcG) {
        m_lcFreq.store(lc, std::memory_order_relaxed);
        m_hcFreq.store(hc, std::memory_order_relaxed);
        m_mcFreq.store(mcF, std::memory_order_relaxed);
        m_mcGain.store(mcG, std::memory_order_relaxed);
        m_filtersDirty.store(true, std::memory_order_relaxed);
    }
    
    void setPhaseRotationAmount(float deg) {
        m_phaseRotationAmount.store(deg, std::memory_order_relaxed);
        m_filtersDirty.store(true, std::memory_order_relaxed);
    }

    // Advanced dynamic setters
    void setNoiseGateAttack(float ms) { m_gateAttack.store(ms, std::memory_order_relaxed); }
    void setNoiseGateHold(float ms) { m_gateHold.store(ms, std::memory_order_relaxed); }
    void setNoiseGateRelease(float ms) { m_gateRelease.store(ms, std::memory_order_relaxed); }
    
    void setCompAttack(float ms) { m_compAttack.store(ms, std::memory_order_relaxed); }
    void setCompRelease(float ms) { m_compRelease.store(ms, std::memory_order_relaxed); }
    void setCompMaxRatio(float ratio) { m_compMaxRatio.store(ratio, std::memory_order_relaxed); }
    void setCompMaxThreshold(float db) { m_compMaxThreshold.store(db, std::memory_order_relaxed); }
    
    void setLimiterAttack(float ms) { m_limiterAttack.store(ms, std::memory_order_relaxed); }
    void setLimiterRelease(float ms) { m_limiterRelease.store(ms, std::memory_order_relaxed); }
    void setLimiterLookahead(float ms) { m_limiterLookahead.store(ms, std::memory_order_relaxed); }
    
    void setGainRiderAttack(float ms) { m_gainRiderAttack.store(ms, std::memory_order_relaxed); }
    void setGainRiderRelease(float ms) { m_gainRiderRelease.store(ms, std::memory_order_relaxed); }
    void setGainRiderLookahead(float ms) { m_gainRiderLookahead.store(ms, std::memory_order_relaxed); }
    void setGainRiderWindow(float ms) { m_gainRiderWindow.store(ms, std::memory_order_relaxed); }

    // Continuous gain envelope for Gain Rider
    void updateGainEnvelope(const std::vector<float>& envelope);

private:
    const std::vector<float> *m_pcmData;
    int m_sampleRate;
    int m_channels;
    std::atomic<long long> m_currentIndex;
    
    // Thread-safe parameters — std::atomic eliminates data races between UI and RT threads
    std::atomic<float> m_limiterThresholdDb{Config::Limiter::ThresholdDb};
    std::atomic<bool> m_lowCutEnabled{false};
    std::atomic<bool> m_highCutEnabled{false};
    std::atomic<bool> m_midCutEnabled{false};
    std::atomic<bool> m_phaseRotationEnabled{false};
    std::atomic<bool> m_effectsEnabled{true};
    std::atomic<bool> m_noiseGateEnabled{Config::NoiseGate::DefaultEnabled};
    std::atomic<bool> m_compressorEnabled{Config::Compressor::DefaultEnabled};
    std::atomic<bool> m_limiterEnabled{Config::Limiter::DefaultEnabled};
    std::atomic<bool> m_gainRiderEnabled{Config::GainRider::DefaultEnabled};
    
    std::atomic<float> m_postCompAmount{0.0f};
    std::atomic<float> m_limiterGainDb{0.0f};
    std::atomic<float> m_noiseGateThresholdDb{Config::NoiseGate::ThresholdOffDb};
    std::atomic<float> m_noiseGateReductionDb{Config::NoiseGate::DefaultReductionDb};
    std::atomic<float> m_lcFreq{Config::Filter::LowCutFreq};
    std::atomic<float> m_hcFreq{Config::Filter::HighCutFreq};
    std::atomic<float> m_mcFreq{Config::Filter::MidCutFreq};
    std::atomic<float> m_phaseRotationAmount{Config::Filter::PhaseAmount};
    std::atomic<bool> m_gainRiderInverted{false};
    std::atomic<bool> m_compInverted{false};

    // Lock-free gain envelope: immutable snapshot published via atomic_store
    std::atomic<std::vector<float>*> m_latestEnvelope{nullptr};
    std::atomic<std::vector<float>*> m_hazardEnvelope{nullptr};
    std::vector<std::vector<float>*> m_garbageEnvelopes;

    std::atomic<float> m_mcGain{Config::Filter::MidCutGainDb};
    std::atomic<bool> m_filtersDirty{false};

    // Advanced dynamic values
    std::atomic<float> m_gateAttack{Config::NoiseGate::AttackMs};
    std::atomic<float> m_gateHold{Config::NoiseGate::HoldMs};
    std::atomic<float> m_gateRelease{Config::NoiseGate::ReleaseMs};

    std::atomic<float> m_compAttack{Config::Compressor::AttackMs};
    std::atomic<float> m_compRelease{Config::Compressor::ReleaseMs};
    std::atomic<float> m_compMaxRatio{Config::Compressor::MaxRatio};
    std::atomic<float> m_compMaxThreshold{Config::Compressor::MaxThresholdDb};

    std::atomic<float> m_limiterAttack{Config::Limiter::AttackMs};
    std::atomic<float> m_limiterRelease{Config::Limiter::ReleaseMs};
    std::atomic<float> m_limiterLookahead{Config::Limiter::LookaheadMs};

    std::atomic<float> m_gainRiderAttack{Config::GainRider::AttackMs};
    std::atomic<float> m_gainRiderRelease{Config::GainRider::ReleaseMs};
    std::atomic<float> m_gainRiderLookahead{Config::GainRider::LookaheadMs};
    std::atomic<float> m_gainRiderWindow{Config::GainRider::AnalysisWindowMs};

    // RT-thread-only DSP state (no synchronization needed — only touched by the callback)
    size_t m_gateHoldCounter = 0;
    float m_gateEnvelope = 1.0f;
    float m_compEnvelope = 0.0f;
    float m_limiterGain = 1.0f;
    std::vector<float> m_limiterDelayBuffer;
    size_t m_limiterDelayWriteIdx = 0;
    float m_riderGain = 1.0f;

    struct Biquad {
        float b0, b1, b2, a1, a2;
        float z1, z2;
        Biquad() : b0(0), b1(0), b2(0), a1(0), a2(0), z1(0), z2(0) {}
        float process(float in) {
            float out = in * b0 + z1;
            z1 = in * b1 + z2 - a1 * out;
            z2 = in * b2 - a2 * out;
            return out;
        }
        void reset() { z1 = z2 = 0; }
    };

    Biquad m_lcL, m_lcR;
    Biquad m_hcL, m_hcR;
    Biquad m_mcL, m_mcR;

    // Hilbert: 2 channels, 2 paths, 6 stages
    DSPState::AllPass1st m_hilbertA[2][6];
    DSPState::AllPass1st m_hilbertB[2][6];

    void setupLowCut();
    void setupHighCut();
    void setupMidCut();
    void setupPhaseRotation();
    
};
