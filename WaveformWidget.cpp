#include "WaveformWidget.h"
#include <cmath>
#include <algorithm>
#include <wx/dcclient.h>
#include <wx/graphics.h>
#include <wx/dcbuffer.h>
#include "config.h"
#include "configAdvanced.h"
#include <memory>

namespace {
    const float HILBERT_A[6] = { 0.045053f, 0.222306f, 0.528346f, 0.825227f, 0.963175f, 0.995960f };
    const float HILBERT_B[6] = { 0.117070f, 0.364441f, 0.697693f, 0.916843f, 0.985558f, 0.999266f };
}

WaveformWidget::WaveformWidget(wxWindow* parent) 
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, 150), wxBORDER_NONE | wxFULL_REPAINT_ON_RESIZE)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    
    Bind(wxEVT_PAINT, &WaveformWidget::OnPaint, this);
    Bind(wxEVT_SIZE, &WaveformWidget::OnSize, this);
    Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent&) {}); // Prevent flickering on Windows
    Bind(wxEVT_LEFT_DOWN, &WaveformWidget::OnMouseLeftDown, this);
    Bind(wxEVT_MOTION, &WaveformWidget::OnMouseMove, this);
    Bind(wxEVT_ENTER_WINDOW, &WaveformWidget::OnMouseEnter, this);
    Bind(wxEVT_LEAVE_WINDOW, &WaveformWidget::OnMouseLeave, this);
}

void WaveformWidget::setAudioData(const std::vector<float> *pcmData, const std::vector<float> *gainEnvelope, float duration, int sampleRate, int channels) {
    m_pcmData = pcmData;
    m_gainEnvelope = gainEnvelope;
    m_duration = duration;
    m_sampleRate = sampleRate;
    m_channels = channels;
    m_playheadTime = 0;
    setupFilters();
    updateCache();
    Refresh();
}

void WaveformWidget::setPlayhead(float timeSeconds) {
    if (std::abs(m_playheadTime - timeSeconds) > 0.001f) {
        m_playheadTime = timeSeconds;
        Refresh();
    }
}


void WaveformWidget::setLowCutEnabled(bool enabled) {
    if (m_lowCutEnabled != enabled) {
        m_lowCutEnabled = enabled;
        updateCache();
        Refresh();
    }
}

void WaveformWidget::setHighCutEnabled(bool enabled) {
    if (m_highCutEnabled != enabled) {
        m_highCutEnabled = enabled;
        updateCache();
        Refresh();
    }
}


void WaveformWidget::setMidCutEnabled(bool enabled) {
    if (m_midCutEnabled != enabled) {
        m_midCutEnabled = enabled;
        updateCache();
        Refresh();
    }
}

void WaveformWidget::setPostCompAmount(float amount) {
    if (std::abs(m_postCompAmount - amount) > 0.001f) {
        m_postCompAmount = amount;
        updateCache();
        Refresh();
    }
}

void WaveformWidget::setLimiterGain(float gainDb) {
    if (std::abs(m_limiterGainDb - gainDb) > 0.01f) {
        m_limiterGainDb = gainDb;
        updateCache();
        Refresh();
    }
}

void WaveformWidget::setNoiseGateThreshold(float db) {
    if (std::abs(m_noiseGateThresholdDb - db) > 0.01f) {
        m_noiseGateThresholdDb = db;
        updateCache();
        Refresh();
    }
}

void WaveformWidget::setNoiseGateReduction(float db) {
    if (std::abs(m_noiseGateReductionDb - db) > 0.01f) {
        m_noiseGateReductionDb = db;
        updateCache();
        Refresh();
    }
}

void WaveformWidget::setEffectsActive(bool active) {
    if (m_effectsEnabled != active) {
        m_effectsEnabled = active;
        updateCache();
        Refresh();
    }
}

void WaveformWidget::setGainRiderInverted(bool inverted) {
    if (m_gainRiderInverted != inverted) {
        m_gainRiderInverted = inverted;
        updateCache();
        Refresh();
    }
}

void WaveformWidget::setCompInverted(bool inverted) {
    if (m_compInverted != inverted) {
        m_compInverted = inverted;
        updateCache();
        Refresh();
    }
}

void WaveformWidget::setLimiterThreshold(float db) {
    if (std::abs(m_limiterThresholdDb - db) > 0.01f) {
        m_limiterThresholdDb = db;
        updateCache();
        Refresh();
    }
}

void WaveformWidget::setPhaseRotationEnabled(bool enabled) {
    if (m_phaseRotationEnabled != enabled) {
        m_phaseRotationEnabled = enabled;
        updateCache();
        Refresh();
    }
}

void WaveformWidget::setPhaseRotationAmount(float deg) {
    if (std::abs(m_phaseRotationAmount - deg) > 0.01f) {
        m_phaseRotationAmount = deg;
        updateCache();
        Refresh();
    }
}
void WaveformWidget::setNoiseGateEnabled(bool enabled) { m_noiseGateEnabled = enabled; updateCache(); Refresh(); }
void WaveformWidget::setCompressorEnabled(bool enabled) { m_compressorEnabled = enabled; updateCache(); Refresh(); }
void WaveformWidget::setLimiterEnabled(bool enabled) { m_limiterEnabled = enabled; updateCache(); Refresh(); }
void WaveformWidget::setGainRiderEnabled(bool enabled) { m_gainRiderEnabled = enabled; updateCache(); Refresh(); }

void WaveformWidget::setGateAttack(float ms) { m_gateAttack = ms; updateCache(); Refresh(); }
void WaveformWidget::setGateHold(float ms) { m_gateHold = ms; updateCache(); Refresh(); }
void WaveformWidget::setGateRelease(float ms) { m_gateRelease = ms; updateCache(); Refresh(); }
void WaveformWidget::setCompAttack(float ms) { m_compAttack = ms; updateCache(); Refresh(); }
void WaveformWidget::setCompRelease(float ms) { m_compRelease = ms; updateCache(); Refresh(); }
void WaveformWidget::setCompMaxRatio(float ratio) { m_compMaxRatio = ratio; updateCache(); Refresh(); }
void WaveformWidget::setCompMaxThreshold(float db) { m_compMaxThreshold = db; updateCache(); Refresh(); }
void WaveformWidget::setLimiterAttack(float ms) { m_limiterAttack = ms; updateCache(); Refresh(); }
void WaveformWidget::setLimiterRelease(float ms) { m_limiterRelease = ms; updateCache(); Refresh(); }
void WaveformWidget::setGainRiderAttack(float ms) { m_gainRiderAttack = ms; updateCache(); Refresh(); }
void WaveformWidget::setGainRiderRelease(float ms) { m_gainRiderRelease = ms; updateCache(); Refresh(); }
void WaveformWidget::setGainRiderWindow(float ms) { m_gainRiderWindow = ms; updateCache(); Refresh(); }
void WaveformWidget::setGainRiderSlewRate(float alpha) { m_gainRiderSlewRate = alpha; updateCache(); Refresh(); }
void WaveformWidget::setGainRiderLookahead(float ms) { m_gainRiderLookahead = ms; updateCache(); Refresh(); }

void WaveformWidget::setLcFreq(float freq) { m_lcFreq = freq; setupFilters(); updateCache(); Refresh(); }
void WaveformWidget::setHcFreq(float freq) { m_hcFreq = freq; setupFilters(); updateCache(); Refresh(); }
void WaveformWidget::setMcFreq(float freq) { m_mcFreq = freq; setupFilters(); updateCache(); Refresh(); }
void WaveformWidget::setMcGain(float db) { m_mcGain = db; setupFilters(); updateCache(); Refresh(); }
void WaveformWidget::setMcQ(float q) { m_mcQ = q; setupFilters(); updateCache(); Refresh(); }
void WaveformWidget::setMcCompensation(float db) { m_mcCompensation = db; setupFilters(); updateCache(); Refresh(); }
void WaveformWidget::setDefaultFilterQ(float q) { m_defaultFilterQ = q; setupFilters(); updateCache(); Refresh(); }
void WaveformWidget::setLimiterLookahead(float ms) { m_limiterLookahead = ms; updateCache(); Refresh(); }

void WaveformWidget::setupFilters() {
    float fs = static_cast<float>(m_sampleRate);
    float Q = m_defaultFilterQ;
    
    // Low Cut
    float K_lc = std::tan(M_PI * m_lcFreq / fs);
    float norm_lc = 1 / (1 + K_lc / Q + K_lc * K_lc);
    m_lcL.b0 = m_lcR.b0 = norm_lc;
    m_lcL.b1 = m_lcR.b1 = -2 * norm_lc;
    m_lcL.b2 = m_lcR.b2 = norm_lc;
    m_lcL.a1 = m_lcR.a1 = 2 * (K_lc * K_lc - 1) * norm_lc;
    m_lcL.a2 = m_lcR.a2 = (1 - K_lc / Q + K_lc * K_lc) * norm_lc;

    // High Cut
    float K_hc = std::tan(M_PI * m_hcFreq / fs);
    float norm_hc = 1 / (1 + K_hc / Q + K_hc * K_hc);
    m_hcL.b0 = m_hcR.b0 = K_hc * K_hc * norm_hc;
    m_hcL.b1 = m_hcR.b1 = 2 * m_hcL.b0;
    m_hcL.b2 = m_hcR.b2 = m_hcL.b0;
    m_hcL.a1 = m_hcR.a1 = 2 * (K_hc * K_hc - 1) * norm_hc;
    m_hcL.a2 = m_hcR.a2 = (1 - K_hc / Q + K_hc * K_hc) * norm_hc;

    // Mid Cut (Peaking EQ)
    float A = std::pow(10.0f, m_mcGain / 40.0f);
    float w0 = 2 * M_PI * m_mcFreq / fs;
    float alpha = std::sin(w0) / (2 * m_mcQ);
    float norm_mc = 1 / (1 + alpha / A);
    float compGain = std::pow(10.0f, m_mcCompensation / 20.0f);
    m_mcL.b0 = m_mcR.b0 = (1 + alpha * A) * norm_mc * compGain;
    m_mcL.b1 = m_mcR.b1 = (-2 * std::cos(w0)) * norm_mc * compGain;
    m_mcL.b2 = m_mcR.b2 = (1 - alpha * A) * norm_mc * compGain;
    m_mcL.a1 = m_mcR.a1 = (-2 * std::cos(w0)) * norm_mc;
    m_mcL.a2 = m_mcR.a2 = (1 - alpha / A) * norm_mc;

    // Hilbert Phase rotation uses static pre-calculated coefficients.
    for (int c = 0; c < 2; ++c) {
        for (int i = 0; i < 6; ++i) {
            m_hilbertA[c][i].z1 = 0;
            m_hilbertB[c][i].z1 = 0;
        }
    }
}

void WaveformWidget::updateCache() {
    m_cachedGains.clear();
    int fullW = GetClientSize().GetWidth();
    int w = fullW - ConfigAdvanced::Visualization::DbScaleWidth;
    if (!m_pcmData || m_pcmData->empty() || w <= 0) return;

    for (int c = 0; c < 2; ++c) {
        m_cachedMinPeaks[c].resize(w);
        m_cachedMaxPeaks[c].resize(w);
    }
    m_cachedGains.resize(w, 1.0f);

    size_t totalSamples = m_pcmData->size();
    float viewDuration = m_duration / m_zoomLevel;
    float startTime = m_viewportOffset;
    float endTime = std::min(m_duration, startTime + viewDuration);
    
    size_t startSample = static_cast<size_t>(startTime * m_sampleRate) * m_channels;
    size_t endSample = static_cast<size_t>(endTime * m_sampleRate) * m_channels;
    endSample = std::min(endSample, totalSamples);
    
    size_t samplesInView = endSample - startSample;
    size_t samplesPerPixel = std::max((size_t)1, samplesInView / w);
    samplesPerPixel -= samplesPerPixel % m_channels;
    if (samplesPerPixel == 0) samplesPerPixel = m_channels;

    m_lcL.reset(); m_lcR.reset();
    m_hcL.reset(); m_hcR.reset();
    m_mcL.reset(); m_mcR.reset();
    for (int c = 0; c < 2; ++c) {
        for (int i = 0; i < 6; ++i) {
            m_hilbertA[c][i].z1 = 0;
            m_hilbertB[c][i].z1 = 0;
        }
    }

    const bool effects = m_effectsEnabled;
    
    // Noise Gate Settings
    float gateThreshold = std::pow(10.0f, m_noiseGateThresholdDb / 20.0f);
    float gateReduction = std::pow(10.0f, -m_noiseGateReductionDb / 20.0f);
    bool gateActive = (m_noiseGateThresholdDb > Config::NoiseGate::ThresholdMinActiveDb);
    float gateAttackCoef = std::exp(-1.0f / (m_gateAttack * 0.001f * m_sampleRate));
    float gateReleaseCoef = std::exp(-1.0f / (m_gateRelease * 0.001f * m_sampleRate));
    int gateHoldSamples = static_cast<int>(m_gateHold * 0.001f * m_sampleRate);
    float gateEnvelope = 1.0f;
    int gateHoldCounter = 0;

    // Compressor Settings
    float compThresholdDb = m_compMaxThreshold * m_postCompAmount;
    float compThreshold = std::pow(10.0f, compThresholdDb / 20.0f);
    float compRatio = 1.0f + ((m_compMaxRatio - 1.0f) * m_postCompAmount);
    float invRatio = 1.0f / compRatio;
    float compAttackCoef = std::exp(-1.0f / (m_compAttack * 0.001f * m_sampleRate));
    float compReleaseCoef = std::exp(-1.0f / (m_compRelease * 0.001f * m_sampleRate));
    const float slope = invRatio - 1.0f;
    const float gainMultiplier = std::pow(10.0f, (-compThresholdDb * slope) / 20.0f);
    float compEnvelope = 0.0f;


    // Limiter Settings
    float limThreshold = std::pow(10.0f, m_limiterThresholdDb / 20.0f);

    // Manual Gain
    float manualGainMultiplier = std::pow(10.0f, m_limiterGainDb / 20.0f);

    bool useHighRes = m_gainRiderWindow < ConfigAdvanced::Ranges::RiderResThreshold;
    float windowSec = useHighRes ? 0.001f : 0.01f;

    for (int x = 0; x < w; ++x) {
        size_t startIdx = startSample + (size_t)x * samplesPerPixel;
        size_t endIdx = std::min(startIdx + samplesPerPixel, endSample);
        float t = startTime + (static_cast<float>(x) / w) * viewDuration;
        
        float riderGain = 1.0f;
        if (effects && m_gainEnvelope && !m_gainEnvelope->empty()) {
            float winPos = t / windowSec;
            size_t winIdx = static_cast<size_t>(winPos);
            float frac = winPos - static_cast<float>(winIdx);

            if (winIdx + 1 < m_gainEnvelope->size()) {
                float g1 = (*m_gainEnvelope)[winIdx];
                float g2 = (*m_gainEnvelope)[winIdx + 1];
                riderGain = g1 + frac * (g2 - g1);
            } else if (winIdx < m_gainEnvelope->size()) {
                riderGain = (*m_gainEnvelope)[winIdx];
            }
            
            if (m_gainRiderInverted) riderGain = 1.0f / (riderGain + 1e-6f);
        }

        float minP[2] = {1.0f, 1.0f};
        float maxP[2] = {-1.0f, -1.0f};
        float rawPeakPixel = 0.0f;
        float procPeakPixel = 0.0f;
        float pixelGainSolo = 1.0f; 

        // Define the processing logic as a lambda to avoid duplication
        auto processSampleAt = [&](size_t i) {
            if (i >= totalSamples) return;
            
            float rawFramePeak = 0.0f;
            for (int c = 0; c < m_channels && c < 2; ++c) {
                if (i + c < totalSamples) rawFramePeak = std::max(rawFramePeak, std::abs((*m_pcmData)[i + c]));
            }
            rawPeakPixel = std::max(rawPeakPixel, rawFramePeak);

            float frameVals[2] = {0.0f, 0.0f};
            float framePeak = 0.0f;

            // 1. Filters
            for (int c = 0; c < m_channels && c < 2; ++c) {
                if (i + c >= totalSamples) break;
                float val = (*m_pcmData)[i + c];
                if (effects) {
                    if (m_lowCutEnabled) val = (c == 0) ? m_lcL.process(val) : m_lcR.process(val);
                    if (m_highCutEnabled) val = (c == 0) ? m_hcL.process(val) : m_hcR.process(val);
                    if (m_midCutEnabled) val = (c == 0) ? m_mcL.process(val) : m_mcR.process(val);
                    if (m_phaseRotationEnabled) {
                        float deg = m_phaseRotationAmount;
                        float rad = deg * 3.1415926535f / 180.0f;
                        float cosP = std::cos(rad);
                        float sinP = std::sin(rad);
                        float in = val;
                        float ia = in;
                        for (int k = 0; k < 6; ++k) ia = m_hilbertA[c][k].process(ia, HILBERT_A[k]);
                        float qb = in;
                        for (int k = 0; k < 6; ++k) qb = m_hilbertB[c][k].process(qb, HILBERT_B[k]);
                        val = ia * cosP - qb * sinP;
                    }
                }
                frameVals[c] = val;
                framePeak = std::max(framePeak, std::abs(val));
            }

            // 2. Noise Gate
            if (effects && m_noiseGateEnabled && gateActive) {
                float target = (framePeak > gateThreshold) ? 1.0f : gateReduction;
                if (framePeak > gateThreshold) {
                    gateHoldCounter = gateHoldSamples;
                } else if (gateHoldCounter > 0) {
                    target = 1.0f;
                    gateHoldCounter -= 1;
                }

                // Attack: time to open (gain recovery). Release: time to close (gain reduction).
                float coef = (target > gateEnvelope) ? gateAttackCoef : gateReleaseCoef;
                gateEnvelope = coef * gateEnvelope + (1.0f - coef) * target;
                
                if (m_gainViewMode == GainViewMode::Gate) pixelGainSolo = gateEnvelope;
                for (int c = 0; c < 2; ++c) frameVals[c] *= gateEnvelope;
                framePeak *= gateEnvelope;
            }

            // 3. Gain Rider
            float actualRiderGain = m_gainRiderEnabled ? riderGain : 1.0f;
            if (m_gainViewMode == GainViewMode::GainRider) pixelGainSolo = actualRiderGain;
            for (int c = 0; c < 2; ++c) frameVals[c] *= actualRiderGain;
            framePeak *= actualRiderGain;

            // 4. Compressor
            if (effects && m_compressorEnabled && m_postCompAmount > 0.01f) {
                float coef = (framePeak > compEnvelope) ? compAttackCoef : compReleaseCoef;
                compEnvelope = coef * compEnvelope + (1.0f - coef) * framePeak;

                float compGain = 1.0f;
                if (compEnvelope > compThreshold) {
                    compGain = std::pow(compEnvelope + 1e-9f, slope) * gainMultiplier;
                }
                if (m_compInverted) compGain = 1.0f / (compGain + 1e-6f);
                if (m_gainViewMode == GainViewMode::Compressor) pixelGainSolo = compGain;
                for (int c = 0; c < 2; ++c) frameVals[c] *= compGain;
                framePeak *= compGain;
            }

            // 5. Limiter (Calculated on un-boosted signal)
            float limiterGain = 1.0f;
            if (effects) {
                for (int c = 0; c < 2; ++c) {
                    float s = frameVals[c];
                    if (m_limiterEnabled) {
                        if (std::abs(s) > limThreshold) {
                            float reduction = limThreshold / (std::abs(s) + 1e-9f);
                            limiterGain = std::min(limiterGain, reduction);
                        }
                    }
                }
                if (m_limiterEnabled && m_gainViewMode == GainViewMode::Limiter) pixelGainSolo = limiterGain;
                
                // Apply limiter gain and then manual gain
                float currentLimGain = m_limiterEnabled ? limiterGain : 1.0f;
                for (int c = 0; c < 2; ++c) {
                    frameVals[c] *= currentLimGain * manualGainMultiplier;
                }
                framePeak = 0.0f;
                for (int c = 0; c < 2; ++c) framePeak = std::max(framePeak, std::abs(frameVals[c]));
            }
            // Track Net Gain for Combined Mode
            if (m_gainViewMode == GainViewMode::Combined) {
                float netGain = 1.0f;
                if (effects) {
                    netGain *= m_noiseGateEnabled ? gateEnvelope : 1.0f;
                    netGain *= m_gainRiderEnabled ? riderGain : 1.0f;
                    
                    float compGain = 1.0f;
                    if (m_compressorEnabled && m_postCompAmount > 0.01f) {
                        if (compEnvelope > compThreshold) {
                            compGain = std::pow(compEnvelope + 1e-9f, slope) * gainMultiplier;
                        }
                        if (m_compInverted) compGain = 1.0f / (compGain + 1e-6f);
                    }
                    netGain *= compGain;
                    netGain *= limiterGain;
                    netGain *= manualGainMultiplier;
                }
                pixelGainSolo = netGain;
            }

            for (int c = 0; c < 2; ++c) {
                if (frameVals[c] < minP[c]) minP[c] = frameVals[c];
                if (frameVals[c] > maxP[c]) maxP[c] = std::max(maxP[c], frameVals[c]);
                procPeakPixel = std::max(procPeakPixel, std::abs(frameVals[c]));
            }
        };

        // Sub-sampling logic: Constant density regardless of zoom
        // Sub-sampling logic: Constant density regardless of zoom
        const size_t NUM_POINTS = ConfigAdvanced::Visualization::PointsPerPixel;
        const size_t BLOCK_SIZE = ConfigAdvanced::Visualization::SamplesPerBlock;
        
        size_t samplesInPixel = endIdx - startIdx;
        bool useSubSampling = (samplesInPixel > NUM_POINTS * BLOCK_SIZE * m_channels);

        if (!useSubSampling) {
            // High zoom-in: process every sample contiguously
            for (size_t i = startIdx; i < endIdx && i < totalSamples; i += m_channels) {
                processSampleAt(i);
            }
        } else {
            // High zoom-out: process NUM_POINTS blocks of BLOCK_SIZE samples
            for (size_t j = 0; j < NUM_POINTS; ++j) {
                size_t offset = (j * samplesInPixel / NUM_POINTS);
                offset -= offset % m_channels;
                size_t baseIdx = startIdx + offset;

                for (size_t k = 0; k < BLOCK_SIZE * m_channels; k += m_channels) {
                    size_t i = baseIdx + k;
                    if (i < totalSamples) processSampleAt(i);
                    else break;
                }
            }
        }
        
        if (startIdx >= endSample) { 
            for (int c = 0; c < 2; ++c) { minP[c] = 0.0f; maxP[c] = 0.0f; }
        }
        
        for (int c = 0; c < 2; ++c) {
            m_cachedMinPeaks[c][x] = minP[c];
            m_cachedMaxPeaks[c][x] = maxP[c];
        }

        m_cachedGains[x] = pixelGainSolo;
    }
    
    renderToBitmap();
}

void WaveformWidget::OnSize(wxSizeEvent& event) {
    updateCache();
    Refresh();
    event.Skip();
}

void WaveformWidget::OnPaint(wxPaintEvent& WXUNUSED(event)) {
    wxBufferedPaintDC dc(this);

    if (!m_waveformBitmap.IsOk()) {
        dc.SetBackground(wxBrush(wxColour(30, 30, 30)));
        dc.Clear();
        return;
    }

    dc.DrawBitmap(m_waveformBitmap, 0, 0);

    wxSize sz = GetClientSize();
    int fullW = sz.GetWidth();
    int fullH = sz.GetHeight();
    int rulerH = ConfigAdvanced::Visualization::TimeRulerHeight;
    int scaleW = ConfigAdvanced::Visualization::DbScaleWidth;
    int w = fullW - scaleW;
    int h = fullH - rulerH;

    if (!m_pcmData || m_pcmData->empty() || w <= 0 || h <= 0) return;

    float viewDuration = m_duration / m_zoomLevel;
    float startTime = m_viewportOffset;
    float endTime = startTime + viewDuration;

    // 4. Playhead
    if (m_playheadTime >= startTime && m_playheadTime <= endTime) {
        int playheadX = scaleW + static_cast<int>((m_playheadTime - startTime) / viewDuration * w);
        dc.SetPen(wxPen(*wxRED, 1));
        dc.DrawLine(playheadX, rulerH, playheadX, fullH);
    }
    
    // 5. Crosshair
    if (m_mouseInside && m_mousePos.x >= scaleW && m_mousePos.x <= fullW) {
        dc.SetPen(wxPen(wxColour(200, 200, 200, 150), 1));
        
        // Vertical line
        dc.DrawLine(m_mousePos.x, rulerH, m_mousePos.x, fullH);
        
        // Horizontal line
        dc.DrawLine(scaleW, m_mousePos.y, fullW, m_mousePos.y);
        
        // Text labels
        float mouseTime = m_viewportOffset + (static_cast<float>(m_mousePos.x - scaleW) / w) * viewDuration;
        mouseTime = std::clamp(mouseTime, 0.0f, m_duration);
        
        int mins = static_cast<int>(mouseTime) / 60;
        int secs = static_cast<int>(mouseTime) % 60;
        int ms = static_cast<int>((mouseTime - std::floor(mouseTime)) * 1000);

        int targetCh = (m_channels == 1 || m_mousePos.y < rulerH + h / 2) ? 0 : 1;
        int chH = (m_channels == 1) ? h : h / 2;
        int chMidY = rulerH + (targetCh * chH) + (chH / 2);
        float chAmp = (static_cast<float>(chH) / 2.0f) * 0.95f;
        
        float dy = static_cast<float>(chMidY - m_mousePos.y);
        float linear = dy / (chAmp + 1e-6f);
        float mouseDb = 20.0f * std::log10(std::abs(linear) + 1e-9f);
        
        wxString timeStr = wxString::Format("%d:%02d.%03d", mins, secs, ms);
        wxString dbStr = (mouseDb < -99.0f) ? "-inf dB" : wxString::Format("%.2f dB", mouseDb);
        wxString label = timeStr + "  " + dbStr;
        
        dc.SetFont(wxFont(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
        wxSize labelSz = dc.GetTextExtent(label);
        
        int labelX = m_mousePos.x + 5;
        int labelY = m_mousePos.y - labelSz.y - 5;
        
        if (labelX + labelSz.x > fullW) labelX = m_mousePos.x - labelSz.x - 5;
        if (labelY < rulerH) labelY = m_mousePos.y + 5;
        
        dc.SetBrush(wxBrush(wxColour(0, 0, 0, 180)));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRectangle(labelX - 2, labelY - 2, labelSz.x + 4, labelSz.y + 4);
        
        dc.SetTextForeground(*wxWHITE);
        dc.DrawText(label, labelX, labelY);
    }
}

void WaveformWidget::renderToBitmap() {
    wxSize sz = GetClientSize();
    int fullW = sz.GetWidth();
    int fullH = sz.GetHeight();

    if (fullW <= 0 || fullH <= 0) return;

    m_waveformBitmap = wxBitmap(fullW, fullH);
    wxMemoryDC memDC(m_waveformBitmap);
    
    // Background
    memDC.SetBackground(wxBrush(wxColour(30, 30, 30)));
    memDC.Clear();

    int rulerH = ConfigAdvanced::Visualization::TimeRulerHeight;
    int scaleW = ConfigAdvanced::Visualization::DbScaleWidth;
    int w = fullW - scaleW;
    int h = fullH - rulerH;

    if (!m_pcmData || m_pcmData->empty() || w <= 0 || h <= 0) {
        memDC.SetTextForeground(*wxWHITE);
        wxString msg = "No Audio Loaded";
        wxSize textSz = memDC.GetTextExtent(msg);
        memDC.DrawText(msg, (fullW - textSz.x) / 2, (fullH - textSz.y) / 2);
        return;
    }

    // Draw Time Ruler Background
    memDC.SetBrush(wxBrush(wxColour(45, 45, 45)));
    memDC.SetPen(*wxTRANSPARENT_PEN);
    memDC.DrawRectangle(0, 0, fullW, rulerH);
    
    // Draw Scale Background
    memDC.DrawRectangle(0, 0, scaleW, fullH);

    memDC.SetFont(wxFont(ConfigAdvanced::Visualization::RulerFontSize, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    memDC.SetTextForeground(wxColour(180, 180, 180));

    // 1. Draw Time Ruler (Top)
    float startTime = m_viewportOffset;
    float viewDuration = m_duration / m_zoomLevel;
    float endTime = startTime + viewDuration;
    
    float interval = 1.0f;
    if (viewDuration > 300) interval = 60.0f;
    else if (viewDuration > 60) interval = 10.0f;
    else if (viewDuration > 10) interval = 5.0f;
    else if (viewDuration > 2) interval = 1.0f;
    else if (viewDuration > 0.5) interval = 0.1f;
    else interval = 0.05f;

    float firstTick = std::floor(startTime / interval) * interval;
    for (float t = firstTick; t <= endTime; t += interval) {
        if (t < startTime) continue;
        int x = scaleW + static_cast<int>((t - startTime) / viewDuration * w);
        
        memDC.SetPen(wxPen(wxColour(100, 100, 100)));
        memDC.DrawLine(x, rulerH - 8, x, rulerH);
        
        int mins = static_cast<int>(t) / 60;
        int secs = static_cast<int>(t) % 60;
        int fms = static_cast<int>((t - std::floor(t)) * 100);
        
        wxString timeStr = (interval >= 1.0f) ? wxString::Format("%d:%02d", mins, secs) : wxString::Format("%d:%02d.%0d", mins, secs, fms/10);
        wxSize szT = memDC.GetTextExtent(timeStr);
        memDC.DrawText(timeStr, x - szT.GetWidth() / 2, 2);
    }

    // Use wxGraphicsContext for the waveform and trace (smoother!)
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(memDC));
    if (!gc) return;

    // 2. Draw dB Scale & Waveforms
    std::vector<float> dbMarkers = {0.0f, -1.0f, -3.0f, -6.0f, -12.0f, -24.0f, -48.0f, -100.0f};
    for (int c = 0; c < m_channels && c < 2; ++c) {
        int chH = (m_channels == 1) ? h : h / 2;
        int chMidY = rulerH + (c * chH) + (chH / 2);
        float chAmp = (static_cast<float>(chH) / 2.0f) * 0.95f;

        // Draw dB Scale for this channel
        memDC.SetFont(wxFont(ConfigAdvanced::Visualization::RulerFontSize, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
        memDC.SetTextForeground(wxColour(180, 180, 180));
        
        int lastY_pos = chMidY + 1000;
        for (float db : dbMarkers) {
            float linear = std::pow(10.0f, db / 20.0f);
            int y_pos = chMidY - static_cast<int>(linear * chAmp);
            int y_neg = chMidY + static_cast<int>(linear * chAmp);

            bool showLabel = (db == 0.0f || std::abs(y_pos - lastY_pos) > 12);
            memDC.SetPen(wxPen(wxColour(60, 60, 60, 100), 1, wxPENSTYLE_DOT));
            memDC.DrawLine(scaleW, y_pos, fullW, y_pos);
            if (db < -0.01f) memDC.DrawLine(scaleW, y_neg, fullW, y_neg);

            if (showLabel) {
                wxString label = (db == 0.0f) ? "0dB" : ((db <= -99.0f) ? "-inf" : wxString::Format("%.0f", db));
                wxSize szL = memDC.GetTextExtent(label);
                memDC.DrawText(label, scaleW - szL.GetWidth() - 5, y_pos - szL.GetHeight() / 2);
                if (db < -0.1f) memDC.DrawText(label, scaleW - szL.GetWidth() - 5, y_neg - szL.GetHeight() / 2);
                lastY_pos = y_pos;
            }
        }

        // Draw Waveform for this channel
        if (m_cachedMaxPeaks[c].size() == static_cast<size_t>(w)) {
            gc->SetPen(wxPen(wxColour(99, 102, 241)));
            gc->SetBrush(wxBrush(wxColour(99, 102, 241, 180)));
            
            wxGraphicsPath path = gc->CreatePath();
            bool first = true;
            for (int x = 0; x < w; ++x) {
                float maxVal = m_cachedMaxPeaks[c][x];
                float y = chMidY + std::clamp(static_cast<float>(maxVal * chAmp), -chAmp, chAmp);
                if (first) { path.MoveToPoint(scaleW + x, y); first = false; }
                else path.AddLineToPoint(scaleW + x, y);
            }
            for (int x = w - 1; x >= 0; --x) {
                float minVal = m_cachedMinPeaks[c][x];
                float y = chMidY + std::clamp(static_cast<float>(minVal * chAmp), -chAmp, chAmp);
                path.AddLineToPoint(scaleW + x, y);
            }
            path.CloseSubpath();
            gc->DrawPath(path);
        }
        
        // Channel Label (L / R)
        if (m_channels > 1) {
            memDC.SetTextForeground(wxColour(150, 150, 150));
            memDC.DrawText(c == 0 ? "L" : "R", scaleW + 5, rulerH + (c * chH) + 2);
        }

        // Draw Gain Trace for this channel
        if (!m_cachedGains.empty()) {
            gc->SetPen(wxPen(wxColour(255, 235, 59, 200), 2));
            wxGraphicsPath gPath = gc->CreatePath();
            bool gFirst = true;

            for (int x = 0; x < w; ++x) {
                float gain = m_cachedGains[x];
                float gainDb = 20.0f * std::log10(gain + 1e-6f);
                float y = chMidY - (gainDb / 24.0f) * chAmp;
                y = std::clamp(y, (float)(rulerH + (c * chH)), (float)(rulerH + (c + 1) * chH - 1));
                
                if (gFirst) { gPath.MoveToPoint(scaleW + x, y); gFirst = false; }
                else gPath.AddLineToPoint(scaleW + x, y);
            }
            gc->StrokePath(gPath);
        }
    }
}

void WaveformWidget::OnMouseLeftDown(wxMouseEvent& event) {
    if (m_duration <= 0) return;
    int fullW = GetClientSize().GetWidth();
    int scaleW = ConfigAdvanced::Visualization::DbScaleWidth;
    int w = fullW - scaleW;
    if (w <= 0) return;

    if (event.GetX() < scaleW) return; // Ignore click on scale area

    float viewDuration = m_duration / m_zoomLevel;
    float seekTime = m_viewportOffset + (static_cast<float>(event.GetX() - scaleW) / w) * viewDuration;
    seekTime = (seekTime < 0.0f) ? 0.0f : (seekTime > m_duration ? m_duration : seekTime);

    if (onSeekTo) onSeekTo(seekTime);
}

void WaveformWidget::OnMouseMove(wxMouseEvent& event) {
    m_mousePos = event.GetPosition();
    m_mouseInside = true;
    Refresh();
}

void WaveformWidget::OnMouseEnter(wxMouseEvent& event) {
    m_mouseInside = true;
    Refresh();
}

void WaveformWidget::OnMouseLeave(wxMouseEvent& event) {
    m_mouseInside = false;
    Refresh();
}

void WaveformWidget::setZoomLevel(float zoom) {
    if (std::abs(m_zoomLevel - zoom) > 0.01f) {
        m_zoomLevel = std::max(1.0f, zoom);
        updateCache();
        Refresh();
    }
}

void WaveformWidget::setViewportOffset(float offset) {
    if (std::abs(m_viewportOffset - offset) > 0.001f) {
        float maxOffset = std::max(0.0f, m_duration - (m_duration / m_zoomLevel));
        m_viewportOffset = std::clamp(offset, 0.0f, maxOffset);
        updateCache();
        Refresh();
    }
}
