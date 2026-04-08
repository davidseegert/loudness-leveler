#pragma once

#include <wx/wx.h>
#include <vector>
#include <functional>
#include "AudioProcessor.h"

class WaveformWidget : public wxPanel {
public:
    WaveformWidget(wxWindow *parent);
    
    enum class GainViewMode { Combined, Gate, GainRider, Compressor, Limiter };
    void setGainViewMode(GainViewMode mode) { m_gainViewMode = mode; updateCache(); Refresh(); }
    GainViewMode getGainViewMode() const { return m_gainViewMode; }

    void setAudioData(const std::vector<float> *pcmData, const std::vector<float> *gainEnvelope, float duration, int sampleRate, int channels);
    void setPlayhead(float timeSeconds);
    void setLowCutEnabled(bool enabled);
    void setHighCutEnabled(bool enabled);
    void setMidCutEnabled(bool enabled);
    void setPostCompAmount(float amount);
    void setLimiterGain(float gainDb);
    void setNoiseGateThreshold(float db);
    void setNoiseGateReduction(float db);
    void setEffectsActive(bool active);
    void setNoiseGateEnabled(bool enabled);
    void setCompressorEnabled(bool enabled);
    void setLimiterEnabled(bool enabled);
    void setGainRiderEnabled(bool enabled);
    void setGainRiderInverted(bool inverted);
    void setCompInverted(bool inverted);
    void setLimiterThreshold(float db);
    void setPhaseRotationEnabled(bool enabled);
    void setPhaseRotationAmount(float deg);
    
    // Advanced Parameter Sync
    void setGateAttack(float ms);
    void setGateHold(float ms);
    void setGateRelease(float ms);
    void setCompAttack(float ms);
    void setCompRelease(float ms);
    void setCompMaxRatio(float ratio);
    void setCompMaxThreshold(float db);
    void setLimiterAttack(float ms);
    void setLimiterRelease(float ms);
    void setGainRiderAttack(float ms);
    void setGainRiderRelease(float ms);
    void setGainRiderWindow(float ms);
    void setGainRiderSlewRate(float alpha);
    void setGainRiderLookahead(float ms);
    
    void setLcFreq(float freq);
    void setHcFreq(float freq);
    void setMcFreq(float freq);
    void setMcGain(float db);
    void setMcQ(float q);
    void setMcCompensation(float db);
    void setDefaultFilterQ(float q);
    void setLimiterLookahead(float ms);

    void setZoomLevel(float zoom);
    void setViewportOffset(float offset);
    void updateCache();
    void setupFilters();

    float getZoomLevel() const { return m_zoomLevel; }
    float getViewportOffset() const { return m_viewportOffset; }

    // Callback for seek events (replacing Qt signal)
    std::function<void(float)> onSeekTo;

protected:
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnMouseLeftDown(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnMouseEnter(wxMouseEvent& event);
    void OnMouseLeave(wxMouseEvent& event);

private:
    const std::vector<float> *m_pcmData = nullptr;
    const std::vector<float> *m_gainEnvelope = nullptr; // Raw Gain Rider envelope from processor
    GainViewMode m_gainViewMode = GainViewMode::Combined;
    std::vector<float> m_cachedGains;
    float m_duration = 0;
    float m_playheadTime = 0;
    float m_zoomLevel = 1.0f;
    float m_viewportOffset = 0.0f; // in seconds
    bool m_effectsEnabled = true;
    bool m_lowCutEnabled = Config::Filter::LowCutEnabled;
    bool m_highCutEnabled = Config::Filter::HighCutEnabled;
    bool m_midCutEnabled = false;
    int m_sampleRate = 44100;
    int m_channels = 2;
    float m_postCompAmount = 0.0f;
    float m_limiterGainDb = 0.0f;
    bool m_gainRiderInverted = false;
    bool m_compInverted = false;
    float m_noiseGateThresholdDb = Config::NoiseGate::ThresholdOffDb;
    float m_limiterThresholdDb = -1.0f;
    float m_noiseGateReductionDb = Config::NoiseGate::DefaultReductionDb;
    float m_currentTarget = Config::GainRider::DefaultTargetDb;

    float m_lcFreq = Config::Filter::LowCutFreq;
    float m_hcFreq = Config::Filter::HighCutFreq;
    float m_mcFreq = Config::Filter::MidCutFreq;
    float m_mcGain = Config::Filter::MidCutGainDb;
    float m_mcQ = Config::Filter::MidCutQ;
    float m_mcCompensation = Config::Filter::MidCutCompensationDb;
    float m_defaultFilterQ = Config::Filter::DefaultQ;

    bool m_phaseRotationEnabled = false;
    float m_phaseRotationAmount = Config::Filter::PhaseAmount;

    // Advanced dynamic values for visual sync
    float m_gateAttack = Config::NoiseGate::AttackMs;
    float m_gateHold = Config::NoiseGate::HoldMs;
    float m_gateRelease = Config::NoiseGate::ReleaseMs;
    float m_compAttack = Config::Compressor::AttackMs;
    float m_compRelease = Config::Compressor::ReleaseMs;
    float m_compMaxRatio = Config::Compressor::MaxRatio;
    float m_compMaxThreshold = Config::Compressor::MaxThresholdDb;
    float m_limiterAttack = Config::Limiter::AttackMs;
    float m_limiterRelease = Config::Limiter::ReleaseMs;
    float m_limiterLookahead = Config::Limiter::LookaheadMs;
    float m_gainRiderAttack = Config::GainRider::AttackMs;
    float m_gainRiderRelease = Config::GainRider::ReleaseMs;
    float m_gainRiderLookahead = Config::GainRider::LookaheadMs;
    float m_gainRiderWindow = Config::GainRider::AnalysisWindowMs;
    float m_gainRiderSlewRate = Config::GainRider::SlewRate;
    bool m_noiseGateEnabled = Config::NoiseGate::DefaultEnabled;
    bool m_compressorEnabled = Config::Compressor::DefaultEnabled;
    bool m_limiterEnabled = Config::Limiter::DefaultEnabled;
    bool m_gainRiderEnabled = Config::GainRider::DefaultEnabled;

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
    struct AllPass1st {
        float z1 = 0;
        inline float process(float in, float a) {
            float out = -a * in + z1;
            z1 = in + a * out;
            return out;
        }
    };
    
    AllPass1st m_hilbertA[2][6];
    AllPass1st m_hilbertB[2][6];
    Biquad m_lcL, m_lcR, m_hcL, m_hcR, m_mcL, m_mcR;
    
    std::vector<float> m_cachedMinPeaks[2];
    std::vector<float> m_cachedMaxPeaks[2];

    wxPoint m_mousePos;
    bool m_mouseInside = false;
};
