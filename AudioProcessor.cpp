#define MINIAUDIO_IMPLEMENTATION
#include "AudioProcessor.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <fstream>
#include "configAdvanced.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
    const float HILBERT_A[6] = { 0.045053f, 0.222306f, 0.528346f, 0.825227f, 0.963175f, 0.995960f };
    const float HILBERT_B[6] = { 0.117070f, 0.364441f, 0.697693f, 0.916843f, 0.985558f, 0.999266f };

    static const float TP_COEFFS[3][12] = {
        { 0.0017f, -0.0129f,  0.0434f, -0.0984f,  0.1983f,  0.9205f, -0.0984f,  0.0434f, -0.0129f,  0.0017f,  0.0000f,  0.0000f },
        { 0.0028f, -0.0210f,  0.0682f, -0.1601f,  0.6101f,  0.6101f, -0.1601f,  0.0682f, -0.0210f,  0.0028f,  0.0000f,  0.0000f },
        { 0.0017f, -0.0129f,  0.0434f, -0.0984f,  0.9205f,  0.1983f, -0.0984f,  0.0434f, -0.0129f,  0.0017f,  0.0000f,  0.0000f }
    };
}

AudioProcessor::AudioProcessor() 
{
}

AudioProcessor::~AudioProcessor() {
}

void AudioProcessor::decodeAudio(const std::string &filePath) {
    if (onDecodingStarted) onDecodingStarted();
    // Use 0 for channels to use the native channel count (Mono/Stereo)
    ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 0, 44100);
    ma_decoder decoder;
    if (ma_decoder_init_file(filePath.c_str(), &decoderConfig, &decoder) != MA_SUCCESS) {
        if (onDecodingError) onDecodingError("Failed to open audio file.");
        return;
    }
    m_sampleRate = decoder.outputSampleRate;
    m_channels = decoder.outputChannels;
    ma_uint64 totalExpectedFrames = 0;
    ma_decoder_get_length_in_pcm_frames(&decoder, &totalExpectedFrames);
    m_pcmData.clear();
    m_segments.clear();
    if (totalExpectedFrames > 0) m_pcmData.reserve(static_cast<size_t>(totalExpectedFrames * m_channels));
    float buffer[4096];
    ma_uint64 totalFramesRead = 0;
    while (true) {
        ma_uint64 framesRead;
        ma_result res = ma_decoder_read_pcm_frames(&decoder, buffer, 4096 / m_channels, &framesRead);
        if (res != MA_SUCCESS || framesRead == 0) break;
        m_pcmData.insert(m_pcmData.end(), buffer, buffer + (framesRead * m_channels));
        totalFramesRead += framesRead;
        if (totalExpectedFrames > 0 && onDecodingProgress) onDecodingProgress(static_cast<float>(totalFramesRead) / totalExpectedFrames);
    }
    ma_decoder_uninit(&decoder);
    
    if (onPcmDataChanged) onPcmDataChanged();
    if (onDecodingFinished) onDecodingFinished();
    
    buildEnergyCaches();
}

void AudioProcessor::buildEnergyCaches() {
    m_energy1ms.clear();
    m_energy10ms.clear();
    if (m_pcmData.empty() || m_sampleRate == 0) return;

    int totalFrames = m_pcmData.size() / m_channels;
    
    // High Res (1ms)
    int frames1ms = static_cast<int>(0.001f * m_sampleRate);
    if (frames1ms < 1) frames1ms = 1;
    m_energy1ms.reserve(totalFrames / frames1ms + 1);

    // Low Res (10ms)
    int frames10ms = static_cast<int>(0.01f * m_sampleRate);
    if (frames10ms < 1) frames10ms = 1;
    m_energy10ms.reserve(totalFrames / frames10ms + 1);

    for (int i = 0; i < totalFrames; ++i) {
        float peakSq = 0.0f;
        for (int c = 0; c < m_channels; ++c) {
            float val = std::abs(m_pcmData[i * m_channels + c]);
            peakSq = std::max(peakSq, val * val);
        }

        static float sum1ms = 0;
        static int count1ms = 0;
        sum1ms += peakSq;
        count1ms++;
        if (count1ms >= frames1ms) {
            m_energy1ms.push_back(sum1ms / count1ms);
            sum1ms = 0; count1ms = 0;
        }

        static float sum10ms = 0;
        static int count10ms = 0;
        sum10ms += peakSq;
        count10ms++;
        if (count10ms >= frames10ms) {
            m_energy10ms.push_back(sum10ms / count10ms);
            sum10ms = 0; count10ms = 0;
        }
    }
    
    calculateGainRiderEnvelope();
}

void AudioProcessor::updateRmsCache() {
    m_windowRmsCache.clear();
    
    // Auto-select resolution: Use 1ms for small windows, 10ms for large windows
    bool useHighRes = m_gainRiderWindow < ConfigAdvanced::Ranges::RiderResThreshold;
    const std::vector<float>& cache = useHighRes ? m_energy1ms : m_energy10ms;
    float stepSec = useHighRes ? 0.001f : 0.01f;

    if (cache.empty()) return;

    float windowSec = m_gainRiderWindow / 1000.0f;
    int windowBuckets = static_cast<int>(windowSec / stepSec);
    if (windowBuckets < 1) windowBuckets = 1;

    double runningSumSq = 0.0;
    int totalBuckets = cache.size();
    m_windowRmsCache.reserve(totalBuckets);

    for (int i = 0; i < totalBuckets; ++i) {
        runningSumSq += (double)cache[i];
        if (i >= windowBuckets) {
            runningSumSq -= (double)cache[i - windowBuckets];
        }
        m_windowRmsCache.push_back(std::sqrt((float)(runningSumSq / windowBuckets)));
    }
}

float AudioProcessor::getDuration() const {
    if (m_sampleRate == 0 || m_channels == 0) return 0;
    return static_cast<float>(m_pcmData.size()) / (m_sampleRate * m_channels);
}

void AudioProcessor::calculateGainRiderEnvelope() {
    updateRmsCache();

    if (m_pcmData.empty() || m_windowRmsCache.empty()) {
        m_gainEnvelope.clear();
        if (onSegmentsUpdated) onSegmentsUpdated();
        return;
    }

    const size_t numWindows = m_windowRmsCache.size();
    m_gainEnvelope.resize(numWindows);

    float targetDb = m_targetDb;
    float upperRange = m_upperRangeDb;
    float lowerRange = m_lowerRangeDb;
    float sens = m_sensitivity / 100.0f;

    // fader speeds
    float attackSec = m_gainRiderAttack / 1000.0f;
    float releaseSec = m_gainRiderRelease / 1000.0f;
    
    // Pick resolution
    bool useHighRes = m_gainRiderWindow < ConfigAdvanced::Ranges::RiderResThreshold;
    float stepSec = useHighRes ? 0.001f : 0.01f;
    
    // One-pole smoothing coefficients for the dynamic step size
    float alphaAttack = (attackSec <= 0.0f) ? 0.0f : std::exp(-stepSec / attackSec);
    float alphaRelease = (releaseSec <= 0.0f) ? 0.0f : std::exp(-stepSec / releaseSec);

    // Floor for sensitivity (ignore quiet parts)
    float floorDb = Config::GainRider::SensitivityFloorBaseDb + (1.0f - sens) * Config::GainRider::SensitivityFloorRangeDb; 

    float currentSmoothedGainDb = 0.0f;

    for (size_t i = 0; i < numWindows; ++i) {
        float rms = m_windowRmsCache[i];
        float rmsDb = 20.0f * std::log10(rms + 1e-6f);
        
        float targetGainDb = 0.0f;
        if (rmsDb > floorDb) {
            targetGainDb = targetDb - rmsDb;
            targetGainDb = (targetGainDb < lowerRange) ? lowerRange : (targetGainDb > upperRange ? upperRange : targetGainDb);
        } else {
            // Signal too quiet, rider returns to 0dB gain
            targetGainDb = 0.0f;
        }

        // Apply smoothing
        float alpha = (targetGainDb > currentSmoothedGainDb) ? alphaAttack : alphaRelease;
        currentSmoothedGainDb = alpha * currentSmoothedGainDb + (1.0f - alpha) * targetGainDb;
        
        m_gainEnvelope[i] = std::pow(10.0f, currentSmoothedGainDb / 20.0f);
    }

    // Apply Lookahead (shift envelope back by N steps)
    int lookaheadSteps = static_cast<int>(m_gainRiderLookahead / (stepSec * 1000.0f)); 
    if (lookaheadSteps > 0) {
        std::vector<float> shifted = m_gainEnvelope;
        for (int i = 0; i < (int)numWindows; ++i) {
            int srcIdx = i + lookaheadSteps;
            if (srcIdx < (int)numWindows) {
                m_gainEnvelope[i] = shifted[srcIdx];
            } else {
                m_gainEnvelope[i] = shifted.back();
            }
        }
    }

    if (onSegmentsUpdated) onSegmentsUpdated();
}


void AudioProcessor::setPostCompAmount(float amount) {
    m_postCompAmount = amount;
}

void AudioProcessor::processBlock(const float* input, float* output, size_t numFrames, DSPState& state) {
    if (!m_effectsEnabled) { std::copy(input, input + numFrames * m_channels, output); return; }
    
    // 1. Hoist all coefficient calculations outside the sample loop
    const float fs = static_cast<float>(m_sampleRate);
    const float invFs = 1.0f / fs;
    const float Q = m_defaultFilterQ;
    
    float lc_b0, lc_b1, lc_b2, lc_a1, lc_a2;
    float hc_b0, hc_b1, hc_b2, hc_a1, hc_a2;
    float mc_b0, mc_b1, mc_b2, mc_a1, mc_a2;
    
    {
        float K = std::tan(M_PI * m_lcFreq / fs);
        float norm = 1 / (1 + K / Q + K * K);
        lc_b0 = norm; lc_b1 = -2 * norm; lc_b2 = norm; lc_a1 = 2 * (K * K - 1) * norm; lc_a2 = (1 - K / Q + K * K) * norm;
        
        K = std::tan(M_PI * m_hcFreq / fs);
        norm = 1 / (1 + K / Q + K * K);
        hc_b0 = K * K * norm; hc_b1 = 2 * hc_b0; hc_b2 = hc_b0; hc_a1 = 2 * (K * K - 1) * norm; hc_a2 = (1 - K / Q + K * K) * norm;
        
        float A = std::pow(10.0f, m_mcGain / 40.0f);
        float w0 = 2 * M_PI * m_mcFreq / fs;
        float alpha = std::sin(w0) / (2 * m_midCutQ.load());
        float norm_mc = 1 / (1 + alpha / A);
        float compGain = std::pow(10.0f, m_midCutCompensationDb / 20.0f);
        mc_b0 = (1 + alpha * A) * norm_mc * compGain; mc_b1 = (-2 * std::cos(w0)) * norm_mc * compGain; mc_b2 = (1 - alpha * A) * norm_mc * compGain;
        mc_a1 = (-2 * std::cos(w0)) * norm_mc; mc_a2 = (1 - alpha / A) * norm_mc;
    }

    const float gateThreshold = std::pow(10.0f, m_noiseGateThresholdDb / 20.0f);
    const float gateAttackCoef = (m_gateAttack <= 0.0f) ? 0.0f : std::exp(-1.0f / (m_gateAttack * 0.001f * m_sampleRate));
    const float gateReleaseCoef = (m_gateRelease <= 0.0f) ? 0.0f : std::exp(-1.0f / (m_gateRelease * 0.001f * m_sampleRate));
    const int gateHoldSamples = static_cast<int>(m_gateHold * 0.001f * m_sampleRate);
    const float gateReduction = std::pow(10.0f, -m_noiseGateReductionDb / 20.0f);

    const float thresholdDb = m_compMaxThreshold * m_postCompAmount;
    const float compThreshold = std::pow(10.0f, thresholdDb / 20.0f);
    const float ratio = 1.0f + ((m_compMaxRatio - 1.0f) * m_postCompAmount);
    float attackCoef = 0.0f;
    float releaseCoef = 0.0f;
    if (m_compressorEnabled) {
        attackCoef = (m_compAttack <= 0.0f) ? 0.0f : std::exp(-1.0f / (m_compAttack * 0.001f * m_sampleRate));
        releaseCoef = (m_compRelease <= 0.0f) ? 0.0f : std::exp(-1.0f / (m_compRelease * 0.001f * m_sampleRate));
    }
    const float compPower = (1.0f / ratio) - 1.0f;
    const float compConstScale = std::pow(10.0f, (-thresholdDb * compPower) / 20.0f);

    const float limiterThreshold = std::pow(10.0f, m_limiterThresholdDb / 20.0f);
    const size_t samplesPerWindow = static_cast<size_t>(m_gainRiderWindow * 0.001f * m_sampleRate);

    size_t framesProcessedInBlock = 0;
    while (framesProcessedInBlock < numFrames) {
        size_t windowIdx = state.framesProcessed / (samplesPerWindow > 0 ? samplesPerWindow : 1);
        float targetGain = 1.0f;
        
        if (windowIdx < m_gainEnvelope.size()) {
            targetGain = m_gainEnvelope[windowIdx];
        }
        
        // Smoothed gain interpolation to prevent zipper noise
        // state.riderGain already holds the gain from the previous sample
        const float slewRate = 0.9995f; // Very fast smoothing for inter-sample transitions

        size_t framesToProcessNow = std::min(numFrames - framesProcessedInBlock, (samplesPerWindow > 0 ? samplesPerWindow : 1) - (state.framesProcessed % (samplesPerWindow > 0 ? samplesPerWindow : 1)));
        
        size_t endFrame = framesProcessedInBlock + framesToProcessNow;
        for (size_t f = framesProcessedInBlock; f < endFrame; ++f) {
            // Apply inter-sample smoothing
            state.riderGain = m_gainRiderSlewRate * state.riderGain + (1.0f - m_gainRiderSlewRate) * targetGain;

            float frame[8]; 
            for (int c = 0; c < m_channels; ++c) {
                float s = input[f * m_channels + c];
                // Inline biquad for speed
                if (m_lowCutEnabled) {
                    auto& st = (c == 0) ? state.lcL : state.lcR;
                    float out = s * lc_b0 + st.z1;
                    st.z1 = s * lc_b1 + st.z2 - lc_a1 * out;
                    st.z2 = s * lc_b2 - lc_a2 * out;
                    s = out;
                }
                if (m_highCutEnabled) {
                    auto& st = (c == 0) ? state.hcL : state.hcR;
                    float out = s * hc_b0 + st.z1;
                    st.z1 = s * hc_b1 + st.z2 - hc_a1 * out;
                    st.z2 = s * hc_b2 - hc_a2 * out;
                    s = out;
                }
                if (m_midCutEnabled) {
                    auto& st = (c == 0) ? state.mcL : state.mcR;
                    float out = s * mc_b0 + st.z1;
                    st.z1 = s * mc_b1 + st.z2 - mc_a1 * out;
                    st.z2 = s * mc_b2 - mc_a2 * out;
                    s = out;
                }
                if (m_phaseRotationEnabled) {
                    float deg = m_phaseRotationAmount;
                    float rad = deg * (float)M_PI / 180.0f;
                    float cosPhi = std::cos(rad);
                    float sinPhi = std::sin(rad);

                    float in = s;
                    float ia = in;
                    for (int i = 0; i < 6; ++i) {
                        ia = state.hilbertA[c][i].process(ia, HILBERT_A[i]);
                    }
                    float qb = in;
                    for (int i = 0; i < 6; ++i) {
                        qb = state.hilbertB[c][i].process(qb, HILBERT_B[i]);
                    }

                    // Rotate (IQ) by deg
                    // Path A gives I, Path B gives Q (with 90 deg difference)
                    s = ia * cosPhi - qb * sinPhi;
                }
                frame[c] = s;
            }

            if (m_noiseGateEnabled && m_noiseGateThresholdDb > m_gateThresholdMinActiveDb) {
                float peak = 0.0f;
                for (int c = 0; c < m_channels; ++c) peak = std::max(peak, std::abs(frame[c]));
                
                float target = (peak > gateThreshold) ? 1.0f : gateReduction;
                if (peak > gateThreshold) {
                    state.gateHoldCounter = gateHoldSamples;
                } else if (state.gateHoldCounter > 0) {
                    target = 1.0f;
                    state.gateHoldCounter--;
                }

                // Attack: time to open (gain recovery). Release: time to close (gain reduction).
                float coef = (target > state.gateEnvelope) ? gateAttackCoef : gateReleaseCoef;
                state.gateEnvelope = coef * state.gateEnvelope + (1.0f - coef) * target;
                
                for (int c = 0; c < m_channels; ++c) frame[c] *= state.gateEnvelope;
            }

            float riderGain = state.riderGain;
            if (!m_gainRiderEnabled) riderGain = 1.0f;
            else if (m_gainRiderInverted) riderGain = 1.0f / (riderGain + 1e-6f);

            std::vector<float> leveledSamples(m_channels);
            for (int c = 0; c < m_channels; ++c) leveledSamples[c] = frame[c] * riderGain;

            if (m_compressorEnabled && m_postCompAmount > 0.01f) {
                float peak = 0.0f;
                for (int c = 0; c < m_channels; ++c) peak = std::max(peak, std::abs(leveledSamples[c]));
                
                float coef = (peak > state.compEnvelope) ? attackCoef : releaseCoef;
                state.compEnvelope = coef * state.compEnvelope + (1.0f - coef) * peak;
                
                float compGain = 1.0f;
                if (state.compEnvelope > compThreshold) {
                    compGain = std::pow(state.compEnvelope + 1e-9f, compPower) * compConstScale;
                } else {
                    compGain = 1.0f;
                }
                
                if (m_compInverted) compGain = 1.0f / (compGain + 1e-6f);
                
                for (int c = 0; c < m_channels; ++c) leveledSamples[c] *= compGain;
            }


            // Manual Gain (consistent with playback/widget)
            float manualGain = std::pow(10.0f, m_limiterGainDb / 20.0f);
            for (int c = 0; c < m_channels; ++c) leveledSamples[c] *= manualGain;

            // Limiter Logic
            {
                const float limThreshold = std::pow(10.0f, m_limiterThresholdDb / 20.0f);
                const float limAttackCoef = m_limiterAttack <= 0.0f ? 0.0f : std::exp(-1.0f / (m_limiterAttack * 0.001f * m_sampleRate));
                const float limReleaseCoef = m_limiterRelease <= 0.0f ? 0.0f : std::exp(-1.0f / (m_limiterRelease * 0.001f * m_sampleRate));
                
                size_t lookaheadFrames = std::max((size_t)1, static_cast<size_t>(m_limiterLookahead * 0.001f * m_sampleRate));
                size_t lookaheadSamples = lookaheadFrames * m_channels;
                
                if (state.limiterDelayBuffer.size() != lookaheadSamples) {
                    state.limiterDelayBuffer.assign(lookaheadSamples, 0.0f);
                    state.limiterDelayWriteIdx = 0;
                    state.limiterGain = 1.0f;
                }

                float peakLim = 0.0f;
                const uint32_t currentIdx = state.tpHistoryIdx;
                
                for (int c = 0; c < m_channels; ++c) {
                    float s = leveledSamples[c];
                    float* history = (c == 0) ? state.tpHistoryL : state.tpHistoryR;
                    history[currentIdx] = s;

                    peakLim = std::max(peakLim, std::abs(s));

                    // 4x True Peak interpolation (using 3 interpolated points)
                    for (int p = 0; p < 3; ++p) {
                        float interp = history[(currentIdx - 11) & 15] * TP_COEFFS[p][0] +
                                       history[(currentIdx - 10) & 15] * TP_COEFFS[p][1] +
                                       history[(currentIdx - 9) & 15] * TP_COEFFS[p][2] +
                                       history[(currentIdx - 8) & 15] * TP_COEFFS[p][3] +
                                       history[(currentIdx - 7) & 15] * TP_COEFFS[p][4] +
                                       history[(currentIdx - 6) & 15] * TP_COEFFS[p][5] +
                                       history[(currentIdx - 5) & 15] * TP_COEFFS[p][6] +
                                       history[(currentIdx - 4) & 15] * TP_COEFFS[p][7] +
                                       history[(currentIdx - 3) & 15] * TP_COEFFS[p][8] +
                                       history[(currentIdx - 2) & 15] * TP_COEFFS[p][9] +
                                       history[(currentIdx - 1) & 15] * TP_COEFFS[p][10] +
                                       history[currentIdx] * TP_COEFFS[p][11];
                        peakLim = std::max(peakLim, std::abs(interp));
                    }
                }
                state.tpHistoryIdx = (currentIdx + 1) & 15;

                float targetLimGain = (peakLim > limThreshold) ? (limThreshold / (peakLim + 1e-9f)) : 1.0f;
                float limCoef = (targetLimGain < state.limiterGain) ? limAttackCoef : limReleaseCoef; 
                state.limiterGain = limCoef * state.limiterGain + (1.0f - limCoef) * targetLimGain;
                
                float currentLimGain = m_limiterEnabled ? state.limiterGain : 1.0f;

                for (int c = 0; c < m_channels; ++c) {
                    float delayed = state.limiterDelayBuffer[state.limiterDelayWriteIdx + c];
                    state.limiterDelayBuffer[state.limiterDelayWriteIdx + c] = leveledSamples[c];
                    float outVal = delayed * currentLimGain;
                    // Final safety brickwall clamp to prevents TP: 7.1 overshoots
                    output[f * m_channels + c] = (outVal < -1.0f) ? -1.0f : (outVal > 1.0f ? 1.0f : outVal);
                }
                state.limiterDelayWriteIdx = (state.limiterDelayWriteIdx + m_channels) % lookaheadSamples;
            }
        }
        
        size_t framesDone = endFrame - framesProcessedInBlock;
        state.framesProcessed += framesDone;
        framesProcessedInBlock = endFrame;
    }
}

bool AudioProcessor::exportWav(const std::string &outPath) {
    if (m_pcmData.empty()) return false;
    ma_encoder_config config = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, m_channels, m_sampleRate);
    ma_encoder encoder;
    if (ma_encoder_init_file(outPath.c_str(), &config, &encoder) != MA_SUCCESS) return false;
    DSPState state; size_t totalFrames = m_pcmData.size() / m_channels; size_t framesDone = 0;
    while (framesDone < totalFrames) {
        size_t toProcess = std::min((size_t)4096, totalFrames - framesDone);
        std::vector<float> buffer(toProcess * m_channels);
        processBlock(&m_pcmData[framesDone * m_channels], buffer.data(), toProcess, state);
        ma_encoder_write_pcm_frames(&encoder, buffer.data(), toProcess, nullptr);
        framesDone += toProcess;
        if (onExportProgress) onExportProgress(static_cast<float>(framesDone) / totalFrames);
    }
    ma_encoder_uninit(&encoder); return true;
}
