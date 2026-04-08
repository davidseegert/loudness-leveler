#pragma once
 
 namespace Config {
 
 // Filter Settings
namespace Filter {
    constexpr float LowCutFreq = 80.0f;
    constexpr float HighCutFreq = 20000.0f;
    constexpr float MidCutFreq = 1000.0f;
    constexpr float MidCutGainDb = -5.0f;
    constexpr float MidCutQ = 1.5f;
    constexpr float MidCutCompensationDb = 1.5f;
    constexpr float DefaultQ = 0.707f;
    constexpr float PhaseAmount = 90.0f; // degrees
    constexpr bool LowCutEnabled = false;
    constexpr bool HighCutEnabled = false;
}

// Noise Gate Settings
namespace NoiseGate {
    constexpr float AttackMs = 2.0f;
    constexpr float HoldMs = 250.0f;
    constexpr float ReleaseMs = 325.0f;
    constexpr float ThresholdOffDb = -60.0f;
    constexpr float ThresholdMinActiveDb = -59.0f;
    constexpr float DefaultReductionDb = 12.5f;
    constexpr bool DefaultEnabled = false;
}

// Compressor Settings
namespace Compressor {
    constexpr float AttackMs = 2.0f;
    constexpr float ReleaseMs = 150.0f;
    constexpr float MaxRatio = 3.5f; 
    constexpr float MaxThresholdDb = -36.0f;
    constexpr float DefaultGainDb = 0.0f;
    constexpr bool DefaultEnabled = false;
}

// Limiter Settings
namespace Limiter {
    constexpr float ThresholdDb = -1.0f;
    constexpr float AttackMs = 0.01f;
    constexpr float ReleaseMs = 50.0f;
    constexpr float LookaheadMs = 5.0f;
    constexpr bool DefaultEnabled = false;
}

// Gain Rider Settings
namespace GainRider {
    constexpr float DefaultTargetDb = -16.0f;
    constexpr float DefaultUpperRangeDb = 6.0f;
    constexpr float DefaultLowerRangeDb = -6.0f;
    constexpr int DefaultSensitivity = 50;
    constexpr float AttackMs = 500.0f; 
    constexpr float ReleaseMs = 1000.0f;
    constexpr float LookaheadMs = 50.0f;
    constexpr float AnalysisWindowMs = 400.0f;
    constexpr float AnalysisStepMs = 1.0f; // Default 1ms buckets
    constexpr float SensitivityFloorBaseDb = -60.0f;
    constexpr float SensitivityFloorRangeDb = 40.0f;
    constexpr float SlewRate = 0.95f; // Inter-sample smoothing alpha
    constexpr bool DefaultEnabled = false;
}
 
 // UI Settings
 namespace UI {
     constexpr float ZoomMaxWindowSec = 3600.0f;
     constexpr float ZoomDefaultWindowSec = 30.0f;
     constexpr float ZoomMinWindowSec = 0.05f;
     constexpr float HideLabelsThresholdSec = 60.0f;
     constexpr bool AdvancedMode = false;
 }
 
 } // namespace Config
