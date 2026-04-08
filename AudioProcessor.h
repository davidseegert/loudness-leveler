#pragma once

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include "config.h"
#include "miniaudio.h"

struct AudioSegment {
    int id;
    float start;
    float end;
    float avgLoudness;
    float gain;
};

struct DSPState {
    struct BiquadState {
        float z1 = 0, z2 = 0;
    };
    struct AllPass1st {
        float z1 = 0;
        inline float process(float in, float a) {
            float out = -a * in + z1;
            z1 = in + a * out;
            return out;
        }
    };

    BiquadState lcL, lcR, hcL, hcR, mcL, mcR;
    
    // Hilbert: 2 channels, 2 paths, 6 stages
    AllPass1st hilbertA[2][6];
    AllPass1st hilbertB[2][6];

    float gateEnvelope = 1.0f;
    int gateHoldCounter = 0;
    float compEnvelope = 0.0f;
    float limiterGain = 1.0f;
    std::vector<float> limiterDelayBuffer;
    size_t limiterDelayWriteIdx = 0;
    float tpHistoryL[16] = {0}, tpHistoryR[16] = {0};
    uint32_t tpHistoryIdx = 0;
    size_t framesProcessed = 0;
    float riderGain = 1.0f;
};

class AudioProcessor {
public:
    AudioProcessor();
    ~AudioProcessor();

    // Callbacks for events (replacing Qt signals)
    std::function<void()> onDecodingStarted;
    std::function<void()> onDecodingFinished;
    std::function<void(const std::string&)> onDecodingError;
    std::function<void(float)> onDecodingProgress;
    std::function<void(float)> onExportProgress;
    std::function<void()> onSegmentsUpdated;
    std::function<void()> onPcmDataChanged;

    void decodeAudio(const std::string &filePath);
    void calculateGainRiderEnvelope();
    void updateRmsCache();
    bool exportWav(const std::string &outPath);

    const std::vector<float>& getPcmData() const { return m_pcmData; }
    const std::vector<float>& getGainEnvelope() const { return m_gainEnvelope; }
    const std::vector<AudioSegment>& getSegments() const { return m_segments; }
    
    int getSampleRate() const { return m_sampleRate; }
    int getChannels() const { return m_channels; }
    float getDuration() const;

    void setLimiterThreshold(float db) { m_limiterThresholdDb = db; }
    float getLimiterThreshold() const { return m_limiterThresholdDb; }

    void setLowCutEnabled(bool enabled) { m_lowCutEnabled = enabled; }
    bool isLowCutEnabled() const { return m_lowCutEnabled; }

    void setHighCutEnabled(bool enabled) { m_highCutEnabled = enabled; }
    bool isHighCutEnabled() const { return m_highCutEnabled; }

    void setMidCutEnabled(bool enabled) { m_midCutEnabled = enabled; }
    bool isMidCutEnabled() const { return m_midCutEnabled; }

    void setPhaseRotationEnabled(bool enabled) { m_phaseRotationEnabled = enabled; }
    bool isPhaseRotationEnabled() const { return m_phaseRotationEnabled; }

    void setSensitivity(int value) { m_sensitivity = value; }
    int getSensitivity() const { return m_sensitivity; }

    void setPostCompAmount(float amount);
    float getPostCompAmount() const { return m_postCompAmount; }

    void setLimiterGain(float db) { m_limiterGainDb = db; }
    float getLimiterGain() const { return m_limiterGainDb; }

    void setNoiseGateThreshold(float db) { m_noiseGateThresholdDb = db; }
    float getNoiseGateThreshold() const { return m_noiseGateThresholdDb; }

    void setNoiseGateReduction(float db) { m_noiseGateReductionDb = db; }
    float getNoiseGateReduction() const { return m_noiseGateReductionDb; }

    void setGainRiderRange(float upperDb, float lowerDb) { 
        m_upperRangeDb = upperDb; 
        m_lowerRangeDb = lowerDb; 
    }
    float getUpperRangeDb() const { return m_upperRangeDb; }
    float getLowerRangeDb() const { return m_lowerRangeDb; }

    void setFilterSettings(float lc, float hc, float mcF, float mcG) {
        m_lcFreq = lc;
        m_hcFreq = hc;
        m_mcFreq = mcF;
        m_mcGain = mcG;
    }

    void setPhaseRotationAmount(float deg) { m_phaseRotationAmount = deg; }
    float getPhaseRotationAmount() const { return m_phaseRotationAmount; }

    void setGainRiderTarget(float db) { m_targetDb = db; }
    float getGainRiderTarget() const { return m_targetDb; }

    void setEffectsEnabled(bool enabled) { m_effectsEnabled = enabled; }
    bool isEffectsEnabled() const { return m_effectsEnabled; }

    void setGainRiderInverted(bool inverted) { m_gainRiderInverted = inverted; }
    bool isGainRiderInverted() const { return m_gainRiderInverted; }

    void setCompInverted(bool inverted) { m_compInverted = inverted; }
    bool isCompInverted() const { return m_compInverted; }

    void setNoiseGateEnabled(bool enabled) { m_noiseGateEnabled = enabled; }
    bool isNoiseGateEnabled() const { return m_noiseGateEnabled; }

    void setCompressorEnabled(bool enabled) { m_compressorEnabled = enabled; }
    bool isCompressorEnabled() const { return m_compressorEnabled; }

    void setLimiterEnabled(bool enabled) { m_limiterEnabled = enabled; }
    bool isLimiterEnabled() const { return m_limiterEnabled; }

    void setGainRiderEnabled(bool enabled) { m_gainRiderEnabled = enabled; }
    bool isGainRiderEnabled() const { return m_gainRiderEnabled; }
    void setMidCutQ(float q) { m_midCutQ = q; }

    // Advanced dynamic setters
    void setNoiseGateAttack(float ms) { m_gateAttack = ms; }
    void setNoiseGateHold(float ms) { m_gateHold = ms; }
    void setNoiseGateRelease(float ms) { m_gateRelease = ms; }
    
    void setCompAttack(float ms) { m_compAttack = ms; }
    void setCompRelease(float ms) { m_compRelease = ms; }
    void setCompMaxRatio(float ratio) { m_compMaxRatio = ratio; }
    void setCompMaxThreshold(float db) { m_compMaxThreshold = db; }
    
    void setLimiterAttack(float ms) { m_limiterAttack = ms; }
    void setLimiterRelease(float ms) { m_limiterRelease = ms; }
    void setLimiterLookahead(float ms) { m_limiterLookahead = ms; }
    
    void setGainRiderAttack(float ms) { m_gainRiderAttack = ms; }
    void setGainRiderRelease(float ms) { m_gainRiderRelease = ms; }
    void setGainRiderLookahead(float ms) { m_gainRiderLookahead = ms; }
    void setGainRiderWindow(float ms) { m_gainRiderWindow = ms; updateRmsCache(); }
    void setGainRiderSlewRate(float alpha) { m_gainRiderSlewRate = alpha; }
    void setMidCutCompensationDb(float db) { m_midCutCompensationDb = db; }
    void setDefaultFilterQ(float q) { m_defaultFilterQ = q; }
    void setGateThresholdMinActiveDb(float db) { m_gateThresholdMinActiveDb = db; }

    // Reusable DSP logic
    void processBlock(const float* input, float* output, size_t numFrames, DSPState& state);

private:
    std::vector<float> m_pcmData;
    std::vector<float> m_windowRmsCache;
    std::vector<float> m_gainEnvelope;
    std::vector<AudioSegment> m_segments;
    
    void buildEnergyCaches();
    
    int m_sampleRate = 44100;
    int m_channels = 2;

    std::atomic<bool> m_effectsEnabled{true};
    std::atomic<float> m_limiterThresholdDb{Config::Limiter::ThresholdDb};
    std::atomic<bool> m_lowCutEnabled{false};
    std::atomic<bool> m_highCutEnabled{false};
    std::atomic<bool> m_midCutEnabled{false};
    std::atomic<bool> m_phaseRotationEnabled{false};
    std::atomic<float> m_phaseRotationAmount{Config::Filter::PhaseAmount};
    std::atomic<bool> m_gainRiderInverted{false};
    std::atomic<bool> m_compInverted{false};
    std::vector<float> m_energy1ms;  // 1ms buckets
    std::vector<float> m_energy10ms; // 10ms buckets
    std::mutex m_pcmMutex;
    std::atomic<int> m_sensitivity{Config::GainRider::DefaultSensitivity};
    std::atomic<float> m_postCompAmount{0.0f};
    std::atomic<float> m_limiterGainDb{Config::Compressor::DefaultGainDb};
    std::atomic<float> m_noiseGateThresholdDb{Config::NoiseGate::ThresholdOffDb};
    std::atomic<float> m_noiseGateReductionDb{Config::NoiseGate::DefaultReductionDb};
    std::atomic<bool> m_noiseGateEnabled{Config::NoiseGate::DefaultEnabled};
    std::atomic<bool> m_compressorEnabled{Config::Compressor::DefaultEnabled};
    std::atomic<bool> m_limiterEnabled{Config::Limiter::DefaultEnabled};
    std::atomic<bool> m_gainRiderEnabled{Config::GainRider::DefaultEnabled};
    std::atomic<float> m_targetDb{Config::GainRider::DefaultTargetDb};
    std::atomic<float> m_upperRangeDb{Config::GainRider::DefaultUpperRangeDb};
    std::atomic<float> m_lowerRangeDb{Config::GainRider::DefaultLowerRangeDb};

    // Advanced dynamic values
    std::atomic<float> m_lcFreq{Config::Filter::LowCutFreq};
    std::atomic<float> m_hcFreq{Config::Filter::HighCutFreq};
    std::atomic<float> m_mcFreq{Config::Filter::MidCutFreq};
    std::atomic<float> m_mcGain{Config::Filter::MidCutGainDb};

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
    std::atomic<float> m_gainRiderSlewRate{Config::GainRider::SlewRate};

    std::atomic<float> m_midCutQ{Config::Filter::MidCutQ};
    std::atomic<float> m_midCutCompensationDb{Config::Filter::MidCutCompensationDb};
    std::atomic<float> m_defaultFilterQ{Config::Filter::DefaultQ};
    std::atomic<float> m_gateThresholdMinActiveDb{Config::NoiseGate::ThresholdMinActiveDb};
};
