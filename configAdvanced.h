#pragma once

namespace ConfigAdvanced {

namespace Labels {
    // Main Area
    inline const char* LoadAudio = "Load Audio";
    inline const char* EffectsActive = "Effects Active";
    inline const char* Zoom = "Zoom:";
    inline const char* ExportWav = "Export WAV";
    inline const char* Trace = "Trace:";
    
    // Gain Trace Options
    inline const char* TraceCombined = "Combined";
    inline const char* TraceGate = "Gate";
    inline const char* TraceRider = "Gain Rider";
    inline const char* TraceComp = "Compressor";
    inline const char* TraceLimiter = "Limiter";

    // Playback
    inline const char* Play = "Play";
    inline const char* Pause = "Pause";

    // Equalizer / Filter
    inline const char* Equalizer = "Equalizer";
    inline const char* LowCut = "Low Cut 80Hz";
    inline const char* MidCut = "Mid Cut 1kHz -5dB";
    inline const char* HighCut = "High Cut 20kHz";
    inline const char* PhaseRotate = "Phase Rotate";
    inline const char* PhaseAmount = "Phase";
    inline const char* LcFreq = "LC Freq";
    inline const char* HcFreq = "HC Freq";
    inline const char* McFreq = "MC Freq";
    inline const char* McGain = "MC Gain";

    // Gate
    inline const char* GateSuffix = "Gate";
    inline const char* Threshold = "Threshold";
    inline const char* Reduction = "Reduction";
    inline const char* Attack = "Attack";
    inline const char* Hold = "Hold";
    inline const char* Release = "Release";

    // Gain Rider
    inline const char* RiderSuffix = "Gain Rider";
    inline const char* Target = "Target";
    inline const char* Range = "Range";
    inline const char* Sensitivity = "Sensitivity";
    inline const char* Window = "Window";
    inline const char* Lookahead = "Lookahead";

    // Compressor / Limiter
    inline const char* Compressor = "Compressor";
    inline const char* Ratio = "Ratio";
    inline const char* Limiter = "Limiter";
    inline const char* Ceiling = "Ceiling"; // Limiter threshold

    // Window Titles
    inline const char* AppTitlePrefix = "Loudness Leveler - ";
}

namespace Ranges {
    // Zoom
    constexpr int ZoomSliderMin = 10;
    constexpr int ZoomSliderMax = 100;
    constexpr int ZoomSliderDefault = 10;

    // Filters
    constexpr float PhaseMin = 0.0f;
    constexpr float PhaseMax = 360.0f;
    constexpr float LcFreqMin = 20.0f;
    constexpr float LcFreqMax = 1000.0f;
    constexpr float HcFreqMin = 1000.0f;
    constexpr float HcFreqMax = 22000.0f;
    constexpr float McFreqMin = 100.0f;
    constexpr float McFreqMax = 5000.0f;
    constexpr float McGainMin = -20.0f;
    constexpr float McGainMax = 20.0f;

    // Gate
    constexpr float GateThresholdMin = -60.0f;
    constexpr float GateThresholdMax = 0.0f;
    constexpr float GateReductionMin = 0.0f;
    constexpr float GateReductionMax = 40.0f;
    constexpr float GateAttackMin = 0.0f;
    constexpr float GateAttackMax = 100.0f;
    constexpr float GateHoldMin = 0.0f;
    constexpr float GateHoldMax = 1000.0f;
    constexpr float GateReleaseMin = 0.0f;
    constexpr float GateReleaseMax = 1000.0f;

    // Gain Rider
    constexpr float RiderTargetMin = -60.0f;
    constexpr float RiderTargetMax = 0.0f;
    constexpr float RiderRangeMin = 0.0f;
    constexpr float RiderRangeMax = 12.0f;
    constexpr float RiderSensitivityMin = 0.0f;
    constexpr float RiderSensitivityMax = 100.0f;
    constexpr float RiderAttackMin = 0.0f;
    constexpr float RiderAttackMax = 2000.0f;
    constexpr float RiderReleaseMin = 0.0f;
    constexpr float RiderReleaseMax = 5000.0f;
    constexpr float RiderWindowMin = 0.0f;
    constexpr float RiderWindowMax = 1000.0f;
    constexpr float RiderLookaheadMin = 0.0f;
    constexpr float RiderLookaheadMax = 100.0f;
    constexpr float RiderHighResStep = 1.0f;
    constexpr float RiderLowResStep = 10.0f;
    constexpr float RiderResThreshold = 50.0f; // Swap to HighRes if Window < 50ms

    // Compressor
    constexpr float CompAttackMin = 0.0f;
    constexpr float CompAttackMax = 200.0f;
    constexpr float CompReleaseMin = 0.0f;
    constexpr float CompReleaseMax = 1000.0f;
    constexpr float CompRatioMin = 1.0f;
    constexpr float CompRatioMax = 20.0f;
    constexpr float CompThresholdMin = -60.0f;
    constexpr float CompThresholdMax = 0.0f;

    // Limiter
    constexpr float LimiterAttackMin = 0.0f;
    constexpr float LimiterAttackMax = 10.0f;
    constexpr float LimiterReleaseMin = 0.0f;
    constexpr float LimiterReleaseMax = 500.0f;
    constexpr float LimiterLookaheadMin = 0.0f;
    constexpr float LimiterLookaheadMax = 20.0f;
    constexpr float LimiterThresholdMin = -12.0f;
    constexpr float LimiterThresholdMax = 0.0f;
    constexpr float LimiterGainMin = -20.0f;
    constexpr float LimiterGainMax = 20.0f;
}

namespace Sizes {
    constexpr int PlayButtonWidth = 80;
    constexpr int PlayButtonHeight = 120;
    constexpr int CheckboxMinWidth = 230;
    constexpr int ZoomSliderWidth = 150;
    constexpr int ScrollRateX = 10;
    constexpr int ScrollRateY = 0;
}

namespace Visualization {
    constexpr size_t SamplesPerBlock = 64;   // contiguous samples per block for filter settling
    constexpr size_t PointsPerPixel = 10;    // number of blocks sampled per pixel column
    
    constexpr int TimeRulerHeight = 25;
    constexpr int DbScaleWidth = 45;
    constexpr int RulerFontSize = 9;
}

} // namespace ConfigAdvanced
