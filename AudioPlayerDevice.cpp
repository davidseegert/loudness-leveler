#include "AudioPlayerDevice.h"
#include <algorithm>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
    const float HILBERT_A[6] = { 0.045053f, 0.222306f, 0.528346f, 0.825227f, 0.963175f, 0.995960f };
    const float HILBERT_B[6] = { 0.117070f, 0.364441f, 0.697693f, 0.916843f, 0.985558f, 0.999266f };
}

AudioPlayerDevice::AudioPlayerDevice(const std::vector<float> *pcmData, int sampleRate, int channels)
    : m_pcmData(pcmData), m_sampleRate(sampleRate), m_channels(channels), m_currentIndex(0)
{
    setupLowCut();
    setupHighCut();
    setupMidCut();
    setupPhaseRotation();
    
    size_t lookaheadFrames = std::max((size_t)1, static_cast<size_t>(m_limiterLookahead.load() * 0.001f * m_sampleRate));
    m_limiterDelayBuffer.assign(lookaheadFrames * m_channels, 0.0f);
}

AudioPlayerDevice::~AudioPlayerDevice() {
}

void AudioPlayerDevice::seekToSample(long long sampleIndex) {
    if (sampleIndex < 0) sampleIndex = 0;
    if (m_pcmData && (size_t)sampleIndex >= m_pcmData->size()) {
        sampleIndex = m_pcmData->size() - (m_pcmData->size() % m_channels);
    }
    sampleIndex = sampleIndex - (sampleIndex % m_channels);
    m_currentIndex.store(sampleIndex, std::memory_order_relaxed);
}

void AudioPlayerDevice::updateGainEnvelope(const std::vector<float>& envelope) {
    // Build an immutable snapshot on the heap
    auto* env = new std::vector<float>(envelope);

    // Publish to RT thread and collect the old one as garbage
    auto* prev = m_latestEnvelope.exchange(env, std::memory_order_seq_cst);
    if (prev) {
        m_garbageEnvelopes.push_back(prev);
    }

    // Hazard pointer garbage collection (runs on UI thread)
    auto* currentHaz = m_hazardEnvelope.load(std::memory_order_seq_cst);
    m_garbageEnvelopes.erase(std::remove_if(m_garbageEnvelopes.begin(), m_garbageEnvelopes.end(),
        [currentHaz](std::vector<float>* s) {
            if (s != currentHaz) {
                delete s;
                return true;
            }
            return false;
        }), m_garbageEnvelopes.end());
}

void AudioPlayerDevice::setupLowCut() {
    float fc = m_lcFreq.load(std::memory_order_relaxed);
    float fs = static_cast<float>(m_sampleRate);
    float Q = Config::Filter::DefaultQ;
    float K = std::tan(M_PI * fc / fs);
    float norm = 1 / (1 + K / Q + K * K);
    float b0 = norm;
    float b1 = -2 * b0;
    float b2 = b0;
    float a1 = 2 * (K * K - 1) * norm;
    float a2 = (1 - K / Q + K * K) * norm;
    
    m_lcL.b0 = m_lcR.b0 = b0;
    m_lcL.b1 = m_lcR.b1 = b1;
    m_lcL.b2 = m_lcR.b2 = b2;
    m_lcL.a1 = m_lcR.a1 = a1;
    m_lcL.a2 = m_lcR.a2 = a2;
}

void AudioPlayerDevice::setupHighCut() {
    float fc = m_hcFreq.load(std::memory_order_relaxed);
    float fs = static_cast<float>(m_sampleRate);
    float Q = Config::Filter::DefaultQ;
    float K = std::tan(M_PI * fc / fs);
    float norm = 1 / (1 + K / Q + K * K);
    float b0 = K * K * norm;
    float b1 = 2 * b0;
    float b2 = b0;
    float a1 = 2 * (K * K - 1) * norm;
    float a2 = (1 - K / Q + K * K) * norm;
    
    m_hcL.b0 = m_hcR.b0 = b0;
    m_hcL.b1 = m_hcR.b1 = b1;
    m_hcL.b2 = m_hcR.b2 = b2;
    m_hcL.a1 = m_hcR.a1 = a1;
    m_hcL.a2 = m_hcR.a2 = a2;
}

void AudioPlayerDevice::setupMidCut() {
    float fs = static_cast<float>(m_sampleRate);
    float A = std::pow(10.0f, m_mcGain.load(std::memory_order_relaxed) / 40.0f);
    float w0 = 2 * M_PI * m_mcFreq.load(std::memory_order_relaxed) / fs;
    float alpha = std::sin(w0) / (2 * Config::Filter::MidCutQ);
    float norm = 1 / (1 + alpha / A);
    float compGain = std::pow(10.0f, Config::Filter::MidCutCompensationDb / 20.0f);
    float b0 = (1 + alpha * A) * norm * compGain;
    float b1 = (-2 * std::cos(w0)) * norm * compGain;
    float b2 = (1 - alpha * A) * norm * compGain;
    float a1 = (-2 * std::cos(w0)) * norm;
    float a2 = (1 - alpha / A) * norm;
    
    m_mcL.b0 = m_mcR.b0 = b0;
    m_mcL.b1 = m_mcR.b1 = b1;
    m_mcL.b2 = m_mcR.b2 = b2;
    m_mcL.a1 = m_mcR.a1 = a1;
    m_mcL.a2 = m_mcR.a2 = a2;
}

void AudioPlayerDevice::setupPhaseRotation() {
    // Current wide-band Hilbert doesn't need frequency-specific pole calculation,
    // as it uses pre-optimized 50Hz-15kHz coefficients.
}


void AudioPlayerDevice::dataCallback(void* pOutput, const void* pInput, ma_uint32 frameCount) {
    (void)pInput;
    if (!m_pcmData || m_pcmData->empty()) return;

    float* out = (float*)pOutput;
    long long index = m_currentIndex.load(std::memory_order_relaxed);
    size_t samplesAvailable = m_pcmData->size() - index;
    ma_uint32 framesAvailable = (ma_uint32)(samplesAvailable / m_channels);
    ma_uint32 framesToProcess = std::min(frameCount, framesAvailable);

    // ========================================================================
    // LOAD ALL ATOMIC PARAMS ONCE PER CALLBACK (not per frame)
    // ========================================================================
    const float limiterThresholdDb = m_limiterThresholdDb.load(std::memory_order_relaxed);
    const bool lowCutOn   = m_lowCutEnabled.load(std::memory_order_relaxed);
    const bool highCutOn    = m_highCutEnabled.load(std::memory_order_relaxed);
    const bool midCutOn     = m_midCutEnabled.load(std::memory_order_relaxed);
    const bool phaseRotOn   = m_phaseRotationEnabled.load(std::memory_order_relaxed);
    const float limiterGainDb = m_limiterGainDb.load(std::memory_order_relaxed);
    const float noiseGateThreshDb = m_noiseGateThresholdDb.load(std::memory_order_relaxed);
    const float noiseGateReducDb  = m_noiseGateReductionDb.load(std::memory_order_relaxed);
    const float postCompAmt = m_postCompAmount.load(std::memory_order_relaxed);
    const bool effectsEnabled = m_effectsEnabled.load(std::memory_order_relaxed);
    const bool gateEnabled = m_noiseGateEnabled.load(std::memory_order_relaxed);
    const bool compEnabled = m_compressorEnabled.load(std::memory_order_relaxed);
    const bool limEnabled = m_limiterEnabled.load(std::memory_order_relaxed);
    const bool riderEnabled = m_gainRiderEnabled.load(std::memory_order_relaxed);
    const bool gainRiderInverted = m_gainRiderInverted.load(std::memory_order_relaxed);
    const bool compInverted = m_compInverted.load(std::memory_order_relaxed);

    // Load Advanced Params
    const float gateAttackMs = m_gateAttack.load(std::memory_order_relaxed);
    const float gateHoldMs = m_gateHold.load(std::memory_order_relaxed);
    const float gateReleaseMs = m_gateRelease.load(std::memory_order_relaxed);
    const float compAttackMs = m_compAttack.load(std::memory_order_relaxed);
    const float compReleaseMs = m_compRelease.load(std::memory_order_relaxed);
    const float compMaxRatio = m_compMaxRatio.load(std::memory_order_relaxed);
    const float compMaxThresholdDb = m_compMaxThreshold.load(std::memory_order_relaxed);
    const float limAttackMs = m_limiterAttack.load(std::memory_order_relaxed);
    const float limReleaseMs = m_limiterRelease.load(std::memory_order_relaxed);
    const float limLookaheadMs = m_limiterLookahead.load(std::memory_order_relaxed);
    const float riderWindowMs = m_gainRiderWindow.load(std::memory_order_relaxed);

    if (m_filtersDirty.load(std::memory_order_relaxed)) {
        setupLowCut();
        setupHighCut();
        setupMidCut();
        setupPhaseRotation();
        m_filtersDirty.store(false, std::memory_order_relaxed);
    }

    if (!effectsEnabled) {
        for (ma_uint32 f = 0; f < framesToProcess; ++f) {
            for (int c = 0; c < m_channels; ++c) {
                out[f * m_channels + c] = (*m_pcmData)[index + (f * m_channels) + c];
            }
        }
        m_currentIndex.fetch_add(framesToProcess * m_channels, std::memory_order_relaxed);
        // Zero out remaining if needed
        if (framesToProcess < frameCount) {
            std::fill(out + (framesToProcess * m_channels), out + (frameCount * m_channels), 0.0f);
        }
        return;
    }

    // ========================================================================
    // LOAD GAIN ENVELOPE USING HAZARD POINTER
    // ========================================================================
    std::vector<float>* env;
    do {
        env = m_latestEnvelope.load(std::memory_order_seq_cst);
        m_hazardEnvelope.store(env, std::memory_order_seq_cst);
    } while (m_latestEnvelope.load(std::memory_order_seq_cst) != env);

    // ========================================================================
    // HOIST ALL INVARIANT MATH OUT OF THE FRAME LOOP
    // ========================================================================

    // Noise Gate coefficients
    const bool gateActive = (noiseGateThreshDb > Config::NoiseGate::ThresholdMinActiveDb);
    const float gateThreshold    = std::pow(10.0f, noiseGateThreshDb / 20.0f);
    // Use exp2f directly instead of pow for linear conversion
    const float gateReductionLin = std::exp2(-noiseGateReducDb * 0.16609640474f); 
    const float gateAttackCoef   = std::exp(-1.0f / (gateAttackMs * 0.001f * m_sampleRate));
    const float gateReleaseCoef  = std::exp(-1.0f / (gateReleaseMs * 0.001f * m_sampleRate));
    const int holdSamples        = static_cast<int>(gateHoldMs * 0.001f * m_sampleRate);

    // Compressor coefficients
    const bool compActive       = (postCompAmt > 0.01f);
    const float thresholdDb     = compMaxThresholdDb * postCompAmt;
    const float compThreshold   = std::pow(10.0f, thresholdDb / 20.0f);
    const float ratio           = 1.0f + ((compMaxRatio - 1.0f) * postCompAmt);
    const float invRatio        = 1.0f / ratio;
    const float compAttackCoef  = std::exp(-1.0f / (compAttackMs * 0.001f * m_sampleRate));
    const float compReleaseCoef = std::exp(-1.0f / (compReleaseMs * 0.001f * m_sampleRate));
    
    // Optimized Compressor Inner Loop Math:
    // compGain = std::pow(m_compEnvelope, slope) * constantGain;
    const float slope = invRatio - 1.0f;
    const float gainMultiplier = std::pow(10.0f, (-thresholdDb * slope) / 20.0f);

    // Limiter settings
    const float limAttackCoef = std::exp(-1.0f / (limAttackMs * 0.001f * m_sampleRate));
    const float limReleaseCoef = std::exp(-1.0f / (limReleaseMs * 0.001f * m_sampleRate));
    
    // Limiter gain slider
    const float limiterGainSlider = std::pow(10.0f, limiterGainDb / 20.0f);

    const float invSrCh = 1.0f / static_cast<float>(m_sampleRate * m_channels);

    const size_t samplesPerWindow = static_cast<size_t>(riderWindowMs * 0.001f * m_sampleRate);

    // ========================================================================
    // MAIN PROCESSING LOOP 
    // ========================================================================
    for (ma_uint32 f = 0; f < framesToProcess; ++f) {
        size_t samplesCurrent = index + (f * m_channels);
        size_t windowIdx = (samplesCurrent / m_channels) / samplesPerWindow;
        
        float targetGain = 1.0f;
        if (env && windowIdx < env->size()) {
            targetGain = (*env)[windowIdx];
        }

        // Inter-sample gain smoothing
        m_riderGain = 0.95f * m_riderGain + 0.05f * targetGain;
        float actualRiderGain = riderEnabled ? m_riderGain : 1.0f;
        if (riderEnabled && gainRiderInverted) actualRiderGain = 1.0f / (actualRiderGain + 1e-6f);

        float frame[2] = {0.0f, 0.0f};
        for (int c = 0; c < m_channels && c < 2; ++c) {
            float s = (*m_pcmData)[index + (f * m_channels) + c];
            if (effectsEnabled) {
                if (lowCutOn) s = (c == 0) ? m_lcL.process(s) : m_lcR.process(s);
                if (highCutOn) s = (c == 0) ? m_hcL.process(s) : m_hcR.process(s);
                if (midCutOn) s = (c == 0) ? m_mcL.process(s) : m_mcR.process(s);
                if (phaseRotOn) {
                    float deg = m_phaseRotationAmount.load(std::memory_order_relaxed);
                    float rad = deg * 3.1415926535f / 180.0f;
                    float cosP = std::cos(rad);
                    float sinP = std::sin(rad);
                    float in = s;
                    float ia = in;
                    for (int i = 0; i < 6; ++i) ia = m_hilbertA[c][i].process(ia, HILBERT_A[i]);
                    float qb = in;
                    for (int i = 0; i < 6; ++i) qb = m_hilbertB[c][i].process(qb, HILBERT_B[i]);
                    s = ia * cosP - qb * sinP;
                }
            }
            frame[c] = s;
        }

        if (gateEnabled && gateActive) {
            float framePeakGate = 0.0f;
            for (int c = 0; c < m_channels && c < 2; ++c)
                framePeakGate = std::max(framePeakGate, std::abs(frame[c]));
            
            float targetGateGain = gateReductionLin;
            if (framePeakGate > gateThreshold) {
                targetGateGain = 1.0f;
                m_gateHoldCounter = holdSamples;
            } else if (m_gateHoldCounter > 0) {
                targetGateGain = 1.0f;
                m_gateHoldCounter--;
            }

            if (targetGateGain > m_gateEnvelope)
                m_gateEnvelope = gateAttackCoef * m_gateEnvelope + (1.0f - gateAttackCoef) * targetGateGain;
            else
                m_gateEnvelope = gateReleaseCoef * m_gateEnvelope + (1.0f - gateReleaseCoef) * targetGateGain;

            for (int c = 0; c < m_channels && c < 2; ++c)
                frame[c] *= m_gateEnvelope;
        }

        float framePeak = 0.0f;
        for (int c = 0; c < m_channels && c < 2; ++c) {
            frame[c] *= actualRiderGain;
            framePeak = std::max(framePeak, std::abs(frame[c]));
        }

        if (compEnabled && compActive) {
            if (framePeak > m_compEnvelope)
                m_compEnvelope = compAttackCoef * m_compEnvelope + (1.0f - compAttackCoef) * framePeak;
            else
                m_compEnvelope = compReleaseCoef * m_compEnvelope + (1.0f - compReleaseCoef) * framePeak;
            
            float compGain = 1.0f;
            if (m_compEnvelope > compThreshold) {
                compGain = std::exp2(std::log2(m_compEnvelope + 1e-6f) * slope) * gainMultiplier;
            } else {
                compGain = 1.0f;
            }
            if (compInverted) compGain = 1.0f / (compGain + 1e-6f);
            for (int c = 0; c < m_channels && c < 2; ++c)
                frame[c] *= compGain;
        }


        // Limiter Logic (Always Active, dynamic threshold)
        {
            float framePeak = 0.0f;
            for (int c = 0; c < m_channels && c < 2; ++c) framePeak = std::max(framePeak, std::abs(frame[c]));
            const float limThresh = std::pow(10.0f, limiterThresholdDb / 20.0f);
            float targetLimGain = (framePeak > limThresh) ? (limThresh / (framePeak + 1e-9f)) : 1.0f;
            float limCoef = (targetLimGain < m_limiterGain) ? limAttackCoef : limReleaseCoef; 
            m_limiterGain = limCoef * m_limiterGain + (1.0f - limCoef) * targetLimGain;
        }

        for (int c = 0; c < m_channels && c < 2; ++c) {
            float delayed = m_limiterDelayBuffer[m_limiterDelayWriteIdx + c];
            m_limiterDelayBuffer[m_limiterDelayWriteIdx + c] = frame[c];
            float currentLimGain = limEnabled ? m_limiterGain : 1.0f;
            float finalVal = delayed * currentLimGain * limiterGainSlider;
            out[f * m_channels + c] = (finalVal < -1.0f) ? -1.0f : (finalVal > 1.0f ? 1.0f : finalVal);
        }
        m_limiterDelayWriteIdx = (m_limiterDelayWriteIdx + m_channels) % m_limiterDelayBuffer.size();
    }

    // Release Hazard Pointer
    m_hazardEnvelope.store(nullptr, std::memory_order_release);

    // Zero out remaining buffer if we reached end of data
    if (framesToProcess < frameCount) {
        std::fill(out + (framesToProcess * m_channels), out + (frameCount * m_channels), 0.0f);
    }

    m_currentIndex.fetch_add(framesToProcess * m_channels, std::memory_order_relaxed);
}
