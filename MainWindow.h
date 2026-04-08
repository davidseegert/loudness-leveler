#include <wx/wx.h>
#include <wx/timer.h>
#include <wx/spinctrl.h>
#include <wx/gauge.h>
#include <wx/filename.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include "AudioProcessor.h"
#include "ControlGroup.h"
#include "LufsAnalyzer.h"
#include "AudioPlayerDevice.h"
#include "WaveformWidget.h"
#include "miniaudio.h"

class MainWindow : public wxFrame {
public:
    MainWindow();
    ~MainWindow();

    void LoadAudioFile(const wxString& path);
    void resetSettings();
    void TriggerLUFSUpdate();
    void RefreshGainRider();
    void syncStageVisibility();
    void syncUiFromState();

private:
    float SliderValueToZoom(int val);
    int ZoomToSliderValue(float zoom);
    
    void RunAnalysisTask(int requestId);
    void updateViewportScroll();
    void OnLUFSAnimationTimer(wxTimerEvent& event);
    void setupUi();
    void loadConfigFromIni(const wxString& path = "");
    void saveConfigToIni(const wxString& path = "");
    void setupAudioContext();
    void stopPlayback();

    void OnLoadAudio(wxCommandEvent& event);
    void OnPlayPause(wxCommandEvent& event);
    void OnExportAudio(wxCommandEvent& event);
    
    void OnGainRiderInvertToggled(wxCommandEvent& event);
    void OnCompInvertToggled(wxCommandEvent& event);
    void OnLowCutToggled(wxCommandEvent& event);
    void OnHighCutToggled(wxCommandEvent& event);
    void OnMidCutToggled(wxCommandEvent& event);
    void OnPhaseRotationToggled(wxCommandEvent& event);
    void OnPhaseAmountChanged(float val);
    void OnEffectsActiveToggled(wxCommandEvent& event);
    void OnNoiseGateActiveToggled(wxCommandEvent& event);
    void OnGainRiderActiveToggled(wxCommandEvent& event);
    void OnCompressorActiveToggled(wxCommandEvent& event);
    void OnLimiterActiveToggled(wxCommandEvent& event);
    
    void OnLoadSettings(wxCommandEvent& event);
    void OnSaveSettings(wxCommandEvent& event);
    void OnRestoreDefaults(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);
    void OnToggleAdvanced(wxCommandEvent& event);
    void syncAdvancedModeUI();
    void OnGainViewModeChanged(wxCommandEvent& event);
    
    void updateEffectsUIState(bool enabled);
    void OnSeekRequested(float timeSeconds);
    void OnZoomScroll(wxScrollEvent& event);
    void OnViewportScroll(wxScrollEvent& event);
    void OnPlaybackTimer(wxTimerEvent& event);
    void OnKeyDown(wxKeyEvent& event);

    // Audio members
    AudioProcessor m_processor;
    std::unique_ptr<AudioPlayerDevice> m_playerDevice;
    ma_device m_device;
    bool m_deviceInitialized = false;

    // UI members
    WaveformWidget *m_waveformWidget;
    wxButton *m_playBtn;
    wxButton *m_exportBtn;

    // Original Simple Controls
    ControlGroup *m_compAmountGroup;
    ControlGroup *m_targetGroup;
    ControlGroup *m_rangeGroup;
    ControlGroup *m_limiterGainGroup;
    ControlGroup *m_gateThreshGroup;
    ControlGroup *m_gateReductionGroup;
    ControlGroup *m_limiterCeilingGroup;
    ControlGroup *m_sensitivityGroup;

    // Advanced Controls
    ControlGroup *m_gateAttackGroup;
    ControlGroup *m_gateHoldGroup;
    ControlGroup *m_gateReleaseGroup;
    
    ControlGroup *m_compAttackGroup;
    ControlGroup *m_compReleaseGroup;
    ControlGroup *m_compRatioGroup;
    ControlGroup *m_compThresholdGroup;
    
    ControlGroup *m_limiterAttackGroup;
    ControlGroup *m_limiterReleaseGroup;
    ControlGroup *m_limiterLookaheadGroup;
    
    ControlGroup *m_gainRiderAttackGroup;
    ControlGroup *m_gainRiderReleaseGroup;
    ControlGroup *m_gainRiderLookaheadGroup;
    ControlGroup *m_gainRiderWindowGroup;

    ControlGroup *m_lcFreqGroup;
    ControlGroup *m_hcFreqGroup;
    ControlGroup *m_mcFreqGroup;
    ControlGroup *m_mcGainGroup;
    ControlGroup *m_phaseAmountGroup;

    wxCheckBox *m_lowCutCheckbox;
    wxCheckBox *m_highCutCheckbox;
    wxCheckBox *m_midCutCheckbox;
    wxCheckBox *m_phaseRotationCheckbox;
    
    wxRadioButton *m_gainCombinedRadio;
    wxRadioButton *m_gainGateRadio;
    wxRadioButton *m_gainRiderRadio;
    wxRadioButton *m_gainCompRadio;
    wxRadioButton *m_gainLimiterRadio;
    wxCheckBox *m_effectsActiveCheckbox;
    wxCheckBox *m_noiseGateActiveCheckbox;
    wxCheckBox *m_gainRiderActiveCheckbox;
    wxCheckBox *m_compressorActiveCheckbox;
    wxCheckBox *m_limiterActiveCheckbox;
    wxCheckBox *m_gainRiderInvertCheckbox;
    wxCheckBox *m_compInvertCheckbox;
    wxGauge *m_progressBar;
    
    wxSlider *m_zoomSlider;
    wxScrollBar *m_viewportScroll;
    wxScrolledWindow *m_scrolledSettings;

    wxTimer m_playbackTimer;
    float m_currentTarget;
    float m_currentGainRiderRange;
    float m_currentComp;
    float m_currentLimiterGain;
    float m_currentGate;
    float m_currentGateReduction;
    float m_currentLimiterThreshold;

    // Advanced dynamic values
    float m_lcFreq = Config::Filter::LowCutFreq;
    float m_hcFreq = Config::Filter::HighCutFreq;
    float m_mcFreq = Config::Filter::MidCutFreq;
    float m_mcGain = Config::Filter::MidCutGainDb;
    float m_phaseRotationAmount = Config::Filter::PhaseAmount;
    bool m_effectsEnabled = true;
    bool m_phaseRotationEnabled = false;
    bool m_lowCutEnabled = Config::Filter::LowCutEnabled;
    bool m_highCutEnabled = Config::Filter::HighCutEnabled;
    bool m_midCutEnabled = false;
    bool m_noiseGateEnabled = Config::NoiseGate::DefaultEnabled;
    bool m_gainRiderEnabled = Config::GainRider::DefaultEnabled;
    bool m_compressorEnabled = Config::Compressor::DefaultEnabled;
    bool m_limiterEnabled = Config::Limiter::DefaultEnabled;

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
    float m_gainRiderSlew = Config::GainRider::SlewRate;

    float m_mcQ = Config::Filter::MidCutQ;
    float m_mcCompensation = Config::Filter::MidCutCompensationDb;
    float m_defaultFilterQ = Config::Filter::DefaultQ;
    float m_gateThresholdMinActiveDb = Config::NoiseGate::ThresholdMinActiveDb;

    bool m_advancedMode = Config::UI::AdvancedMode;

    // LUFS Analysis
    std::thread m_analysisThread;
    std::atomic<int> m_analysisRequestId{0};
    wxTimer m_analysisAnimationTimer;
    int m_animationState = 0;
    
    struct LUFSResults {
        double lufs = -100.0;
        double lra = 0.0;
        double tp = -100.0;
        bool valid = false;
    };
    LUFSResults m_latestResults;
    std::mutex m_resultsMutex;

    static void maDataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);

    enum {
        ID_LOAD_AUDIO = wxID_HIGHEST + 1,
        ID_EXPORT_AUDIO,
        ID_LOAD_SETTINGS,
        ID_SAVE_SETTINGS,
        ID_RESTORE_DEFAULTS,
        ID_TOGGLE_ADVANCED,
        ID_GAIN_VIEW_COMBINED,
        ID_GAIN_VIEW_GATE,
        ID_GAIN_VIEW_RIDER,
        ID_GAIN_VIEW_COMP,
        ID_GAIN_VIEW_LIMITER,
        ID_ABOUT
    };
};
