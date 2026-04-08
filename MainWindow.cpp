#include "MainWindow.h"
#include <wx/filedlg.h>
#include <wx/stattext.h>
#include <wx/msgdlg.h>
#include "config.h"
#include "configAdvanced.h"
#include <wx/dnd.h>
#include <wx/fileconf.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>

class FileDropTarget : public wxFileDropTarget {
public:
    FileDropTarget(MainWindow* window) : m_window(window) {}
    virtual bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames) override {
        if (filenames.GetCount() > 0) {
            m_window->LoadAudioFile(filenames[0]);
            return true;
        }
        return false;
    }
private:
    MainWindow* m_window;
};

MainWindow::MainWindow()
    : wxFrame(nullptr, wxID_ANY, "Loudness Leveler", wxDefaultPosition, wxSize(1100, 600)),
      m_currentTarget(Config::GainRider::DefaultTargetDb), 
      m_currentComp(0.0f), m_currentGate(Config::NoiseGate::ThresholdOffDb),
      m_currentGateReduction(Config::NoiseGate::DefaultReductionDb),
      m_currentGainRiderRange(Config::GainRider::DefaultUpperRangeDb),
      m_currentLimiterGain(Config::Compressor::DefaultGainDb),
      m_lcFreq(Config::Filter::LowCutFreq),
      m_hcFreq(Config::Filter::HighCutFreq),
      m_mcFreq(Config::Filter::MidCutFreq),
      m_mcGain(Config::Filter::MidCutGainDb),
      m_gateAttack(Config::NoiseGate::AttackMs),
      m_gateHold(Config::NoiseGate::HoldMs),
      m_gateRelease(Config::NoiseGate::ReleaseMs),
      m_compAttack(Config::Compressor::AttackMs),
      m_compRelease(Config::Compressor::ReleaseMs),
      m_compMaxRatio(Config::Compressor::MaxRatio),
      m_compMaxThreshold(Config::Compressor::MaxThresholdDb),
      m_limiterAttack(Config::Limiter::AttackMs),
      m_limiterRelease(Config::Limiter::ReleaseMs),
      m_limiterLookahead(Config::Limiter::LookaheadMs),
      m_gainRiderAttack(Config::GainRider::AttackMs),
      m_gainRiderRelease(Config::GainRider::ReleaseMs),
      m_gainRiderLookahead(Config::GainRider::LookaheadMs),
      m_gainRiderWindow(Config::GainRider::AnalysisWindowMs),
      m_gainRiderSlew(Config::GainRider::SlewRate)
{
    loadConfigFromIni();
    setupUi();
    syncAdvancedModeUI();
 
    CreateStatusBar();
    SetStatusText("Ready");

    m_processor.onDecodingStarted = [this]() {
        SetStatusText("Decoding audio...");
        m_progressBar->SetValue(0);
        m_progressBar->Show();
        m_progressBar->GetParent()->Layout();
    };

    m_processor.onDecodingProgress = [this](float percent) {
        m_progressBar->SetValue(static_cast<int>(percent * 100));
        wxYield(); // keep UI responsive during heavy load
    };

    m_processor.onDecodingFinished = [this]() {
        SetStatusText("Ready");
        m_progressBar->Hide();
        m_progressBar->GetParent()->Layout();
        setupAudioContext();
        m_playBtn->Enable(true);
        m_exportBtn->Enable(true);
        
        RefreshGainRider();
        
        m_waveformWidget->setAudioData(&m_processor.getPcmData(), 
                                      &m_processor.getGainEnvelope(),
                                      m_processor.getDuration(), 
                                      m_processor.getSampleRate(), 
                                      m_processor.getChannels());

        float duration = m_processor.getDuration();
        float minZoom = 1.0f; // Full audio
        float defaultZoom = std::max(1.0f, duration / Config::UI::ZoomDefaultWindowSec);

        m_zoomSlider->SetRange(0, 1000);
        m_zoomSlider->SetValue(ZoomToSliderValue(defaultZoom));

        wxScrollEvent evt;
        evt.SetPosition(m_zoomSlider->GetValue());
        OnZoomScroll(evt);
    };

    m_processor.onSegmentsUpdated = [this]() {
        if (m_playerDevice) {
            m_playerDevice->updateGainEnvelope(m_processor.getGainEnvelope());
        }
        m_waveformWidget->Refresh();
    };

    m_processor.onDecodingError = [this](const std::string& err) {
        m_progressBar->Hide();
        m_progressBar->GetParent()->Layout();
        wxMessageBox(err, "Error", wxOK | wxICON_ERROR);
        SetStatusText("Error loading file");
    };

    m_playbackTimer.SetOwner(this);
    m_analysisAnimationTimer.SetOwner(this);
    Bind(wxEVT_TIMER, &MainWindow::OnPlaybackTimer, this, m_playbackTimer.GetId());
    Bind(wxEVT_TIMER, &MainWindow::OnLUFSAnimationTimer, this, m_analysisAnimationTimer.GetId());
    Bind(wxEVT_CHAR_HOOK, &MainWindow::OnKeyDown, this);
}

MainWindow::~MainWindow() {
    stopPlayback();
    m_analysisRequestId++; // Cancel any running analysis
    if (m_analysisThread.joinable()) {
        m_analysisThread.join();
    }
}

void MainWindow::loadConfigFromIni(const wxString& path) {
    wxString configFile = path;
    if (configFile.IsEmpty()) {
        wxString iniPath = wxStandardPaths::Get().GetExecutablePath();
        wxFileName fn(iniPath);
        fn.SetFullName("config.ini");
        configFile = fn.GetFullPath();
    }

    if (!wxFileExists(configFile)) return;

    wxFileConfig config("", "", configFile, "", wxCONFIG_USE_LOCAL_FILE);

    // Filter overrides
    m_lcFreq = config.ReadDouble("/Filter/LowCutFreq", Config::Filter::LowCutFreq);
    m_hcFreq = config.ReadDouble("/Filter/HighCutFreq", Config::Filter::HighCutFreq);
    m_mcFreq = config.ReadDouble("/Filter/MidCutFreq", Config::Filter::MidCutFreq);
    m_mcGain = config.ReadDouble("/Filter/MidCutGainDb", Config::Filter::MidCutGainDb);
    m_mcQ = config.ReadDouble("/Filter/MidCutQ", Config::Filter::MidCutQ);
    m_mcCompensation = config.ReadDouble("/Filter/MidCutCompensationDb", Config::Filter::MidCutCompensationDb);
    m_defaultFilterQ = config.ReadDouble("/Filter/DefaultQ", Config::Filter::DefaultQ);
    
    m_processor.setFilterSettings(m_lcFreq, m_hcFreq, m_mcFreq, m_mcGain);
    m_processor.setMidCutQ(m_mcQ);
    m_processor.setMidCutCompensationDb(m_mcCompensation);
    m_processor.setDefaultFilterQ(m_defaultFilterQ);

    m_lowCutEnabled = config.ReadBool("/Filter/LowCutEnabled", Config::Filter::LowCutEnabled);
    m_highCutEnabled = config.ReadBool("/Filter/HighCutEnabled", Config::Filter::HighCutEnabled);
    m_midCutEnabled = config.ReadBool("/Filter/MidCutEnabled", false);
    m_phaseRotationEnabled = config.ReadBool("/Filter/PhaseRotationEnabled", false);
    m_phaseRotationAmount = config.ReadDouble("/Filter/PhaseAmount", Config::Filter::PhaseAmount);
    m_processor.setPhaseRotationAmount(m_phaseRotationAmount);
    m_processor.setLowCutEnabled(m_lowCutEnabled);
    m_processor.setHighCutEnabled(m_highCutEnabled);
    m_processor.setMidCutEnabled(m_midCutEnabled);

    // Gain Rider defaults
    m_currentTarget = config.ReadDouble("/GainRider/TargetDb", Config::GainRider::DefaultTargetDb);
    m_currentGainRiderRange = config.ReadDouble("/GainRider/RangeDb", Config::GainRider::DefaultUpperRangeDb);
    m_gainRiderAttack = config.ReadDouble("/GainRider/AttackMs", Config::GainRider::AttackMs);
    m_gainRiderRelease = config.ReadDouble("/GainRider/ReleaseMs", Config::GainRider::ReleaseMs);
    m_gainRiderLookahead = config.ReadDouble("/GainRider/LookaheadMs", Config::GainRider::LookaheadMs);
    m_gainRiderWindow = config.ReadDouble("/GainRider/AnalysisWindowMs", Config::GainRider::AnalysisWindowMs);
    m_gainRiderSlew = config.ReadDouble("/GainRider/SlewRate", Config::GainRider::SlewRate);
    
    m_gainRiderEnabled = config.ReadBool("/GainRider/Enabled", Config::GainRider::DefaultEnabled);
    
    m_processor.setGainRiderTarget(m_currentTarget);
    m_processor.setGainRiderRange(m_currentGainRiderRange, -m_currentGainRiderRange);
    m_processor.setGainRiderAttack(m_gainRiderAttack);
    m_processor.setGainRiderRelease(m_gainRiderRelease);
    m_processor.setGainRiderLookahead(m_gainRiderLookahead);
    m_processor.setGainRiderWindow(m_gainRiderWindow);
    m_processor.setGainRiderSlewRate(m_gainRiderSlew);
    m_processor.setGainRiderEnabled(m_gainRiderEnabled);

    // Compressor defaults
    m_currentLimiterGain = config.ReadDouble("/Limiter/GainDb", Config::Compressor::DefaultGainDb);
    m_compAttack = config.ReadDouble("/Compressor/AttackMs", Config::Compressor::AttackMs);
    m_compRelease = config.ReadDouble("/Compressor/ReleaseMs", Config::Compressor::ReleaseMs);
    m_compMaxRatio = config.ReadDouble("/Compressor/MaxRatio", Config::Compressor::MaxRatio);
    m_compMaxThreshold = config.ReadDouble("/Compressor/MaxThresholdDb", Config::Compressor::MaxThresholdDb);
    
    m_compressorEnabled = config.ReadBool("/Compressor/Enabled", Config::Compressor::DefaultEnabled);
    
    m_processor.setLimiterGain(m_currentLimiterGain);
    m_processor.setCompAttack(m_compAttack);
    m_processor.setCompRelease(m_compRelease);
    m_processor.setCompMaxRatio(m_compMaxRatio);
    m_processor.setCompMaxThreshold(m_compMaxThreshold);
    m_processor.setCompressorEnabled(m_compressorEnabled);
    
    // Gate defaults
    m_currentGate = config.ReadDouble("/NoiseGate/ThresholdDb", Config::NoiseGate::ThresholdOffDb);
    m_currentGateReduction = config.ReadDouble("/NoiseGate/ReductionDb", Config::NoiseGate::DefaultReductionDb);
    m_gateAttack = config.ReadDouble("/NoiseGate/AttackMs", Config::NoiseGate::AttackMs);
    m_gateHold = config.ReadDouble("/NoiseGate/HoldMs", Config::NoiseGate::HoldMs);
    m_gateRelease = config.ReadDouble("/NoiseGate/ReleaseMs", Config::NoiseGate::ReleaseMs);
    
    m_gateThresholdMinActiveDb = config.ReadDouble("/NoiseGate/ThresholdMinActiveDb", Config::NoiseGate::ThresholdMinActiveDb);
    
    m_noiseGateEnabled = config.ReadBool("/NoiseGate/Enabled", Config::NoiseGate::DefaultEnabled);
    
    m_processor.setNoiseGateThreshold(m_currentGate);
    m_processor.setNoiseGateReduction(m_currentGateReduction);
    m_processor.setNoiseGateAttack(m_gateAttack);
    m_processor.setNoiseGateHold(m_gateHold);
    m_processor.setNoiseGateRelease(m_gateRelease);
    m_processor.setGateThresholdMinActiveDb(m_gateThresholdMinActiveDb);
    m_processor.setNoiseGateEnabled(m_noiseGateEnabled);

    // Limiter defaults
    m_currentLimiterThreshold = config.ReadDouble("/Limiter/ThresholdDb", Config::Limiter::ThresholdDb);
    m_limiterAttack = config.ReadDouble("/Limiter/AttackMs", Config::Limiter::AttackMs);
    m_limiterRelease = config.ReadDouble("/Limiter/ReleaseMs", Config::Limiter::ReleaseMs);
    m_limiterLookahead = config.ReadDouble("/Limiter/LookaheadMs", Config::Limiter::LookaheadMs);
    
    m_limiterEnabled = config.ReadBool("/Limiter/Enabled", Config::Limiter::DefaultEnabled);
    
    m_processor.setLimiterThreshold(m_currentLimiterThreshold);
    m_processor.setLimiterAttack(m_limiterAttack);
    m_processor.setLimiterRelease(m_limiterRelease);
    m_processor.setLimiterLookahead(m_limiterLookahead);
    m_processor.setLimiterEnabled(m_limiterEnabled);

    m_advancedMode = config.ReadBool("/UI/AdvancedMode", Config::UI::AdvancedMode);

    syncUiFromState();
}

void MainWindow::saveConfigToIni(const wxString& path) {
    wxString configFile = path;
    if (configFile.IsEmpty()) {
        wxString iniPath = wxStandardPaths::Get().GetExecutablePath();
        wxFileName fn(iniPath);
        fn.SetFullName("config.ini");
        configFile = fn.GetFullPath();
    }

    wxFileConfig config("", "", configFile, "", wxCONFIG_USE_LOCAL_FILE);

    config.Write("/Filter/LowCutFreq", m_lcFreq);
    config.Write("/Filter/HighCutFreq", m_hcFreq);
    config.Write("/Filter/MidCutFreq", m_mcFreq);
    config.Write("/Filter/MidCutGainDb", m_mcGain);
    config.Write("/Filter/MidCutQ", m_mcQ);
    config.Write("/Filter/MidCutCompensationDb", m_mcCompensation);
    config.Write("/Filter/DefaultQ", m_defaultFilterQ);
    config.Write("/Filter/LowCutEnabled", m_lowCutEnabled);
    config.Write("/Filter/HighCutEnabled", m_highCutEnabled);
    config.Write("/Filter/MidCutEnabled", m_midCutEnabled);
    config.Write("/Filter/PhaseRotationEnabled", m_phaseRotationEnabled);
    config.Write("/Filter/PhaseAmount", m_phaseRotationAmount);
    
    config.Write("/GainRider/TargetDb", m_currentTarget);
    config.Write("/GainRider/Enabled", m_gainRiderEnabled);
    config.Write("/GainRider/RangeDb", m_currentGainRiderRange);
    config.Write("/GainRider/AttackMs", m_gainRiderAttack);
    config.Write("/GainRider/ReleaseMs", m_gainRiderRelease);
    config.Write("/GainRider/LookaheadMs", m_gainRiderLookahead);
    config.Write("/GainRider/AnalysisWindowMs", m_gainRiderWindow);
    config.Write("/GainRider/SlewRate", m_gainRiderSlew);

    config.Write("/Compressor/AttackMs", m_compAttack);
    config.Write("/Compressor/ReleaseMs", m_compRelease);
    config.Write("/Compressor/MaxRatio", m_compMaxRatio);
    config.Write("/Compressor/MaxThresholdDb", m_compMaxThreshold);
    config.Write("/Compressor/Enabled", m_compressorEnabled);
    
    config.Write("/NoiseGate/ThresholdDb", m_currentGate);
    config.Write("/NoiseGate/ReductionDb", m_currentGateReduction);
    config.Write("/NoiseGate/AttackMs", m_gateAttack);
    config.Write("/NoiseGate/HoldMs", m_gateHold);
    config.Write("/NoiseGate/ReleaseMs", m_gateRelease);
    config.Write("/NoiseGate/ThresholdMinActiveDb", m_gateThresholdMinActiveDb);
    config.Write("/NoiseGate/Enabled", m_noiseGateEnabled);

    config.Write("/Limiter/ThresholdDb", m_currentLimiterThreshold);
    config.Write("/Limiter/GainDb", m_currentLimiterGain);
    config.Write("/Limiter/AttackMs", m_limiterAttack);
    config.Write("/Limiter/ReleaseMs", m_limiterRelease);
    config.Write("/Limiter/LookaheadMs", m_limiterLookahead);
    config.Write("/Limiter/Enabled", m_limiterEnabled);

    config.Write("/UI/AdvancedMode", m_advancedMode);

    config.Flush();
}

void MainWindow::setupUi() {
    // --- Menu Bar ---
    wxMenuBar* menuBar = new wxMenuBar();

    wxMenu* fileMenu = new wxMenu();
    fileMenu->Append(ID_LOAD_AUDIO, "&Load Audio...\tCtrl+O");
    fileMenu->Append(ID_EXPORT_AUDIO, "&Export Audio...\tCtrl+E");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT, "E&xit\tAlt+X");
    menuBar->Append(fileMenu, "&File");

    wxMenu* settingsMenu = new wxMenu();
    settingsMenu->Append(ID_LOAD_SETTINGS, "&Load Settings...\tCtrl+Shift+L");
    settingsMenu->Append(ID_SAVE_SETTINGS, "&Save Settings...\tCtrl+Shift+S");
    settingsMenu->AppendSeparator();
    settingsMenu->Append(ID_RESTORE_DEFAULTS, "&Restore Default Settings\tCtrl+R");
    settingsMenu->AppendSeparator();
    settingsMenu->AppendCheckItem(ID_TOGGLE_ADVANCED, "Show &Advanced Options\tCtrl+Shift+A");
    menuBar->Append(settingsMenu, "&Settings");

    SetMenuBar(menuBar);

    // Bind Menu Events
    Bind(wxEVT_MENU, &MainWindow::OnLoadAudio, this, ID_LOAD_AUDIO);
    Bind(wxEVT_MENU, &MainWindow::OnExportAudio, this, ID_EXPORT_AUDIO);
    Bind(wxEVT_MENU, &MainWindow::OnExit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &MainWindow::OnLoadSettings, this, ID_LOAD_SETTINGS);
    Bind(wxEVT_MENU, &MainWindow::OnSaveSettings, this, ID_SAVE_SETTINGS);
    Bind(wxEVT_MENU, &MainWindow::OnRestoreDefaults, this, ID_RESTORE_DEFAULTS);
    Bind(wxEVT_MENU, &MainWindow::OnToggleAdvanced, this, ID_TOGGLE_ADVANCED);

    wxPanel* panel = new wxPanel(this);
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Top Area: Load and Export
    wxBoxSizer* topSizer = new wxBoxSizer(wxHORIZONTAL);
    
    wxButton* loadBtn = new wxButton(panel, wxID_ANY, ConfigAdvanced::Labels::LoadAudio);
    loadBtn->Bind(wxEVT_BUTTON, &MainWindow::OnLoadAudio, this);
    topSizer->Add(loadBtn, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    
    m_effectsActiveCheckbox = new wxCheckBox(panel, wxID_ANY, ConfigAdvanced::Labels::EffectsActive);
    m_effectsActiveCheckbox->SetValue(true);
    m_effectsActiveCheckbox->Bind(wxEVT_CHECKBOX, &MainWindow::OnEffectsActiveToggled, this);
    topSizer->Add(m_effectsActiveCheckbox, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    topSizer->AddStretchSpacer();

    topSizer->Add(new wxStaticText(panel, wxID_ANY, ConfigAdvanced::Labels::Zoom), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    m_zoomSlider = new wxSlider(panel, wxID_ANY, ConfigAdvanced::Ranges::ZoomSliderDefault, ConfigAdvanced::Ranges::ZoomSliderMin, ConfigAdvanced::Ranges::ZoomSliderMax, wxDefaultPosition, wxSize(ConfigAdvanced::Sizes::ZoomSliderWidth, -1));
    m_zoomSlider->Bind(wxEVT_SCROLL_CHANGED, &MainWindow::OnZoomScroll, this);
    m_zoomSlider->Bind(wxEVT_SCROLL_THUMBTRACK, &MainWindow::OnZoomScroll, this);
    topSizer->Add(m_zoomSlider, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    m_exportBtn = new wxButton(panel, wxID_ANY, ConfigAdvanced::Labels::ExportWav);
    m_exportBtn->Enable(false);
    m_exportBtn->Bind(wxEVT_BUTTON, &MainWindow::OnExportAudio, this);
    topSizer->Add(m_exportBtn, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    mainSizer->Add(topSizer, 0, wxEXPAND | wxALL, 5);

    // Middle Area: Waveform
    m_waveformWidget = new WaveformWidget(panel);
    m_waveformWidget->onSeekTo = [this](float t) { OnSeekRequested(t); };
    mainSizer->Add(m_waveformWidget, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 5);

    // Scrollbar
    m_viewportScroll = new wxScrollBar(panel, wxID_ANY);
    m_viewportScroll->SetScrollbar(0, 100, 100, 100);
    m_viewportScroll->Bind(wxEVT_SCROLL_CHANGED, &MainWindow::OnViewportScroll, this);
    m_viewportScroll->Bind(wxEVT_SCROLL_THUMBTRACK, &MainWindow::OnViewportScroll, this);
    mainSizer->Add(m_viewportScroll, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Gain View Selection Radio Buttons
    wxBoxSizer* gainViewSizer = new wxBoxSizer(wxHORIZONTAL);
    gainViewSizer->Add(new wxStaticText(panel, wxID_ANY, ConfigAdvanced::Labels::Trace), 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);
    
    m_gainCombinedRadio = new wxRadioButton(panel, ID_GAIN_VIEW_COMBINED, ConfigAdvanced::Labels::TraceCombined, wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    m_gainGateRadio = new wxRadioButton(panel, ID_GAIN_VIEW_GATE, ConfigAdvanced::Labels::TraceGate);
    m_gainRiderRadio = new wxRadioButton(panel, ID_GAIN_VIEW_RIDER, ConfigAdvanced::Labels::TraceRider);
    m_gainCompRadio = new wxRadioButton(panel, ID_GAIN_VIEW_COMP, ConfigAdvanced::Labels::TraceComp);
    m_gainLimiterRadio = new wxRadioButton(panel, ID_GAIN_VIEW_LIMITER, ConfigAdvanced::Labels::TraceLimiter);

    m_gainCombinedRadio->Bind(wxEVT_RADIOBUTTON, &MainWindow::OnGainViewModeChanged, this);
    m_gainGateRadio->Bind(wxEVT_RADIOBUTTON, &MainWindow::OnGainViewModeChanged, this);
    m_gainRiderRadio->Bind(wxEVT_RADIOBUTTON, &MainWindow::OnGainViewModeChanged, this);
    m_gainCompRadio->Bind(wxEVT_RADIOBUTTON, &MainWindow::OnGainViewModeChanged, this);
    m_gainLimiterRadio->Bind(wxEVT_RADIOBUTTON, &MainWindow::OnGainViewModeChanged, this);

    m_gainCombinedRadio->SetValue(true);

    gainViewSizer->Add(m_gainCombinedRadio, 0, wxRIGHT, 10);
    gainViewSizer->Add(m_gainGateRadio, 0, wxRIGHT, 10);
    gainViewSizer->Add(m_gainRiderRadio, 0, wxRIGHT, 10);
    gainViewSizer->Add(m_gainCompRadio, 0, wxRIGHT, 10);
    gainViewSizer->Add(m_gainLimiterRadio, 0, wxRIGHT, 10);

    mainSizer->Add(gainViewSizer, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxALIGN_CENTER_HORIZONTAL, 5);

    // Bottom Area
    wxBoxSizer* bottomAreaSizer = new wxBoxSizer(wxHORIZONTAL);

    // Play button - large
    m_playBtn = new wxButton(panel, wxID_ANY, ConfigAdvanced::Labels::Play, wxDefaultPosition, wxSize(ConfigAdvanced::Sizes::PlayButtonWidth, ConfigAdvanced::Sizes::PlayButtonHeight));
    m_playBtn->Enable(false);
    m_playBtn->Bind(wxEVT_BUTTON, &MainWindow::OnPlayPause, this);
    bottomAreaSizer->Add(m_playBtn, 0, wxALL | wxALIGN_CENTER_VERTICAL, 10);

    // --- Control Groups (Scrollable) ---
    m_scrolledSettings = new wxScrolledWindow(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL);
    m_scrolledSettings->SetScrollRate(ConfigAdvanced::Sizes::ScrollRateX, ConfigAdvanced::Sizes::ScrollRateY);
    wxBoxSizer* controlsSizer = new wxBoxSizer(wxHORIZONTAL);

    // 1. Equalizer Group (Now Filter Group)
    wxStaticBoxSizer* eqGroupSizer = new wxStaticBoxSizer(wxVERTICAL, m_scrolledSettings, ConfigAdvanced::Labels::Equalizer);
    m_lowCutCheckbox = new wxCheckBox(m_scrolledSettings, wxID_ANY, ConfigAdvanced::Labels::LowCut);
    m_lowCutCheckbox->SetMinSize(wxSize(ConfigAdvanced::Sizes::CheckboxMinWidth, -1));
    m_lowCutCheckbox->SetValue(m_lowCutEnabled);
    m_lowCutCheckbox->Bind(wxEVT_CHECKBOX, &MainWindow::OnLowCutToggled, this);
    eqGroupSizer->Add(m_lowCutCheckbox, 0, wxALL, 2);

    m_midCutCheckbox = new wxCheckBox(m_scrolledSettings, wxID_ANY, ConfigAdvanced::Labels::MidCut);
    m_midCutCheckbox->SetMinSize(wxSize(ConfigAdvanced::Sizes::CheckboxMinWidth, -1));
    m_midCutCheckbox->SetValue(m_midCutEnabled);
    m_midCutCheckbox->Bind(wxEVT_CHECKBOX, &MainWindow::OnMidCutToggled, this);
    eqGroupSizer->Add(m_midCutCheckbox, 0, wxALL, 2);

    m_highCutCheckbox = new wxCheckBox(m_scrolledSettings, wxID_ANY, ConfigAdvanced::Labels::HighCut);
    m_highCutCheckbox->SetMinSize(wxSize(ConfigAdvanced::Sizes::CheckboxMinWidth, -1));
    m_highCutCheckbox->SetValue(m_highCutEnabled);
    m_highCutCheckbox->Bind(wxEVT_CHECKBOX, &MainWindow::OnHighCutToggled, this);
    eqGroupSizer->Add(m_highCutCheckbox, 0, wxALL, 2);

    m_phaseRotationCheckbox = new wxCheckBox(m_scrolledSettings, wxID_ANY, ConfigAdvanced::Labels::PhaseRotate);
    m_phaseRotationCheckbox->SetMinSize(wxSize(ConfigAdvanced::Sizes::CheckboxMinWidth, -1));
    m_phaseRotationCheckbox->SetValue(m_phaseRotationEnabled);
    m_phaseRotationCheckbox->Bind(wxEVT_CHECKBOX, &MainWindow::OnPhaseRotationToggled, this);
    eqGroupSizer->Add(m_phaseRotationCheckbox, 0, wxALL, 2);

    m_phaseAmountGroup = new ControlGroup(m_scrolledSettings, eqGroupSizer, ConfigAdvanced::Labels::PhaseAmount, ConfigAdvanced::Ranges::PhaseMin, ConfigAdvanced::Ranges::PhaseMax, m_phaseRotationAmount, 1.0f, 0, 1.0f);
    m_phaseAmountGroup->OnValueChange = [this](float v) {
        m_phaseRotationAmount = v;
        m_processor.setPhaseRotationAmount(v);
        if (m_playerDevice) m_playerDevice->setPhaseRotationAmount(v);
        if (m_waveformWidget) m_waveformWidget->setPhaseRotationAmount(v);
        saveConfigToIni();
        TriggerLUFSUpdate();
    };

    // Advanced Filter Tuning (Inside Equalizer)
    m_lcFreqGroup = new ControlGroup(m_scrolledSettings, eqGroupSizer, ConfigAdvanced::Labels::LcFreq, ConfigAdvanced::Ranges::LcFreqMin, ConfigAdvanced::Ranges::LcFreqMax, m_lcFreq, 1.0f, 0, 1.0f);
    m_lcFreqGroup->OnValueChange = [this](float v) {
        m_lcFreq = v;
        m_processor.setFilterSettings(m_lcFreq, m_hcFreq, m_mcFreq, m_mcGain);
        if (m_playerDevice) m_playerDevice->setFilterSettings(m_lcFreq, m_hcFreq, m_mcFreq, m_mcGain);
        if (m_waveformWidget) m_waveformWidget->setLcFreq(v); 
        saveConfigToIni();
        TriggerLUFSUpdate();
    };
    m_lcFreqGroup->Show(false);

    m_hcFreqGroup = new ControlGroup(m_scrolledSettings, eqGroupSizer, ConfigAdvanced::Labels::HcFreq, ConfigAdvanced::Ranges::HcFreqMin, ConfigAdvanced::Ranges::HcFreqMax, m_hcFreq, 100.0f, 0, 0.1f);
    m_hcFreqGroup->OnValueChange = [this](float v) {
        m_hcFreq = v;
        m_processor.setFilterSettings(m_lcFreq, m_hcFreq, m_mcFreq, m_mcGain);
        if (m_playerDevice) m_playerDevice->setFilterSettings(m_lcFreq, m_hcFreq, m_mcFreq, m_mcGain);
        if (m_waveformWidget) m_waveformWidget->setHcFreq(v);
        saveConfigToIni();
        TriggerLUFSUpdate();
    };
    m_hcFreqGroup->Show(false);

    m_mcFreqGroup = new ControlGroup(m_scrolledSettings, eqGroupSizer, ConfigAdvanced::Labels::McFreq, ConfigAdvanced::Ranges::McFreqMin, ConfigAdvanced::Ranges::McFreqMax, m_mcFreq, 10.0f, 0, 1.0f);
    m_mcFreqGroup->OnValueChange = [this](float v) {
        m_mcFreq = v;
        m_processor.setFilterSettings(m_lcFreq, m_hcFreq, m_mcFreq, m_mcGain);
        if (m_playerDevice) m_playerDevice->setFilterSettings(m_lcFreq, m_hcFreq, m_mcFreq, m_mcGain);
        if (m_waveformWidget) m_waveformWidget->setMcFreq(v);
        saveConfigToIni();
        TriggerLUFSUpdate();
    };
    m_mcFreqGroup->Show(false);

    m_mcGainGroup = new ControlGroup(m_scrolledSettings, eqGroupSizer, ConfigAdvanced::Labels::McGain, ConfigAdvanced::Ranges::McGainMin, ConfigAdvanced::Ranges::McGainMax, m_mcGain, 0.5f, 1, 10.0f);
    m_mcGainGroup->OnValueChange = [this](float v) {
        m_mcGain = v;
        m_processor.setFilterSettings(m_lcFreq, m_hcFreq, m_mcFreq, m_mcGain);
        if (m_playerDevice) m_playerDevice->setFilterSettings(m_lcFreq, m_hcFreq, m_mcFreq, m_mcGain);
        if (m_waveformWidget) m_waveformWidget->setMcGain(v);
        saveConfigToIni();
        TriggerLUFSUpdate();
    };
    m_mcGainGroup->Show(false);

    controlsSizer->Add(eqGroupSizer, 0, wxEXPAND | wxALL, 5);

    // 2. Gate Group
    wxStaticBoxSizer* gateGroupSizer = new wxStaticBoxSizer(wxVERTICAL, m_scrolledSettings, ConfigAdvanced::Labels::GateSuffix);
    m_noiseGateActiveCheckbox = new wxCheckBox(m_scrolledSettings, wxID_ANY, "Active");
    m_noiseGateActiveCheckbox->SetMinSize(wxSize(ConfigAdvanced::Sizes::CheckboxMinWidth, -1));
    m_noiseGateActiveCheckbox->SetValue(m_noiseGateEnabled);
    m_noiseGateActiveCheckbox->Bind(wxEVT_CHECKBOX, &MainWindow::OnNoiseGateActiveToggled, this);
    gateGroupSizer->Add(m_noiseGateActiveCheckbox, 0, wxALL, 2);

    m_gateThreshGroup = new ControlGroup(m_scrolledSettings, gateGroupSizer, ConfigAdvanced::Labels::Threshold, ConfigAdvanced::Ranges::GateThresholdMin, ConfigAdvanced::Ranges::GateThresholdMax, m_currentGate, 1.0f, 1, 10.0f);
    m_gateThreshGroup->OnValueChange = [this](float v) {
        m_currentGate = v;
        m_processor.setNoiseGateThreshold(v);
        if (m_playerDevice) m_playerDevice->setNoiseGateThreshold(v);
        if (m_waveformWidget) m_waveformWidget->setNoiseGateThreshold(v);
        saveConfigToIni();
        TriggerLUFSUpdate();
    };

    m_gateReductionGroup = new ControlGroup(m_scrolledSettings, gateGroupSizer, ConfigAdvanced::Labels::Reduction, ConfigAdvanced::Ranges::GateReductionMin, ConfigAdvanced::Ranges::GateReductionMax, m_currentGateReduction, 1.0f, 1, 10.0f);
    m_gateReductionGroup->OnValueChange = [this](float v) {
        m_currentGateReduction = v;
        m_processor.setNoiseGateReduction(v);
        if (m_playerDevice) m_playerDevice->setNoiseGateReduction(v);
        if (m_waveformWidget) m_waveformWidget->setNoiseGateReduction(v);
        saveConfigToIni();
        TriggerLUFSUpdate();
    };

    m_gateAttackGroup = new ControlGroup(m_scrolledSettings, gateGroupSizer, ConfigAdvanced::Labels::Attack, ConfigAdvanced::Ranges::GateAttackMin, ConfigAdvanced::Ranges::GateAttackMax, m_gateAttack, 0.1f, 1, 10.0f);
    m_gateAttackGroup->OnValueChange = [this](float v) {
        m_gateAttack = v;
        m_processor.setNoiseGateAttack(v);
        if (m_playerDevice) m_playerDevice->setNoiseGateAttack(v);
        if (m_waveformWidget) m_waveformWidget->setGateAttack(v);
        saveConfigToIni();
        TriggerLUFSUpdate();
    };
    m_gateAttackGroup->Show(false);

    m_gateHoldGroup = new ControlGroup(m_scrolledSettings, gateGroupSizer, ConfigAdvanced::Labels::Hold, ConfigAdvanced::Ranges::GateHoldMin, ConfigAdvanced::Ranges::GateHoldMax, m_gateHold, 1.0f, 1, 10.0f);
    m_gateHoldGroup->OnValueChange = [this](float v) {
        m_gateHold = v;
        m_processor.setNoiseGateHold(v);
        if (m_playerDevice) m_playerDevice->setNoiseGateHold(v);
        if (m_waveformWidget) m_waveformWidget->setGateHold(v);
        saveConfigToIni();
        TriggerLUFSUpdate();
    };
    m_gateHoldGroup->Show(false);

    m_gateReleaseGroup = new ControlGroup(m_scrolledSettings, gateGroupSizer, ConfigAdvanced::Labels::Release, ConfigAdvanced::Ranges::GateReleaseMin, ConfigAdvanced::Ranges::GateReleaseMax, m_gateRelease, 1.0f, 1, 10.0f);
    m_gateReleaseGroup->OnValueChange = [this](float v) {
        m_gateRelease = v;
        m_processor.setNoiseGateRelease(v);
        if (m_playerDevice) m_playerDevice->setNoiseGateRelease(v);
        if (m_waveformWidget) m_waveformWidget->setGateRelease(v);
        saveConfigToIni();
        TriggerLUFSUpdate();
    };
    m_gateReleaseGroup->Show(false);

    controlsSizer->Add(gateGroupSizer, 0, wxEXPAND | wxALL, 5);

    // 3. Gain Rider Group
    wxStaticBoxSizer* riderGroupSizer = new wxStaticBoxSizer(wxVERTICAL, m_scrolledSettings, "Gain Rider");
    m_gainRiderActiveCheckbox = new wxCheckBox(m_scrolledSettings, wxID_ANY, "Active");
    m_gainRiderActiveCheckbox->SetMinSize(wxSize(230, -1));
    m_gainRiderActiveCheckbox->SetValue(m_gainRiderEnabled);
    m_gainRiderActiveCheckbox->Bind(wxEVT_CHECKBOX, &MainWindow::OnGainRiderActiveToggled, this);
    riderGroupSizer->Add(m_gainRiderActiveCheckbox, 0, wxALL, 2);
    
    m_targetGroup = new ControlGroup(m_scrolledSettings, riderGroupSizer, "Target", ConfigAdvanced::Ranges::RiderTargetMin, ConfigAdvanced::Ranges::RiderTargetMax, m_currentTarget, 1.0f, 1, 10.0f);
    m_targetGroup->OnValueChange = [this](float v) {
        m_currentTarget = v;
        m_processor.setGainRiderTarget(v);
        RefreshGainRider();
        saveConfigToIni();
    };

    m_rangeGroup = new ControlGroup(m_scrolledSettings, riderGroupSizer, "Range", ConfigAdvanced::Ranges::RiderRangeMin, ConfigAdvanced::Ranges::RiderRangeMax, m_currentGainRiderRange, 0.5f, 1, 10.0f);
    m_rangeGroup->OnValueChange = [this](float v) {
        m_currentGainRiderRange = v;
        m_processor.setGainRiderRange(v, -v);
        RefreshGainRider();
        saveConfigToIni();
    };

    m_sensitivityGroup = new ControlGroup(m_scrolledSettings, riderGroupSizer, "Sensitivity", ConfigAdvanced::Ranges::RiderSensitivityMin, ConfigAdvanced::Ranges::RiderSensitivityMax, Config::GainRider::DefaultSensitivity, 1.0f, 0, 1.0f);
    m_sensitivityGroup->OnValueChange = [this](float v) {
        int s = (int)v;
        m_processor.setSensitivity(s);
        saveConfigToIni();
        RefreshGainRider();
    };

    m_gainRiderAttackGroup = new ControlGroup(m_scrolledSettings, riderGroupSizer, "Attack", ConfigAdvanced::Ranges::RiderAttackMin, ConfigAdvanced::Ranges::RiderAttackMax, m_gainRiderAttack, 1.0f, 0, 1.0f);
    m_gainRiderAttackGroup->OnValueChange = [this](float v) {
        m_gainRiderAttack = v;
        m_processor.setGainRiderAttack(v);
        if (m_waveformWidget) m_waveformWidget->setGainRiderAttack(v);
        RefreshGainRider();
        saveConfigToIni();
    };
    m_gainRiderAttackGroup->Show(false);

    m_gainRiderReleaseGroup = new ControlGroup(m_scrolledSettings, riderGroupSizer, "Release", ConfigAdvanced::Ranges::RiderReleaseMin, ConfigAdvanced::Ranges::RiderReleaseMax, m_gainRiderRelease, 1.0f, 0, 1.0f);
    m_gainRiderReleaseGroup->OnValueChange = [this](float v) {
        m_gainRiderRelease = v;
        m_processor.setGainRiderRelease(v);
        if (m_waveformWidget) m_waveformWidget->setGainRiderRelease(v);
        RefreshGainRider();
        saveConfigToIni();
    };
    m_gainRiderReleaseGroup->Show(false);

    m_gainRiderLookaheadGroup = new ControlGroup(m_scrolledSettings, riderGroupSizer, ConfigAdvanced::Labels::Lookahead, ConfigAdvanced::Ranges::RiderLookaheadMin, ConfigAdvanced::Ranges::RiderLookaheadMax, m_gainRiderLookahead, 1.0f, 0, 1.0f);
    m_gainRiderLookaheadGroup->OnValueChange = [this](float v) {
        m_gainRiderLookahead = v;
        m_processor.setGainRiderLookahead(v);
        if (m_waveformWidget) m_waveformWidget->setGainRiderLookahead(v);
        RefreshGainRider();
        saveConfigToIni();
    };
    m_gainRiderLookaheadGroup->Show(false);

    m_gainRiderWindowGroup = new ControlGroup(m_scrolledSettings, riderGroupSizer, ConfigAdvanced::Labels::Window, ConfigAdvanced::Ranges::RiderWindowMin, ConfigAdvanced::Ranges::RiderWindowMax, m_gainRiderWindow, 1.0f, 0, 1.0f);
    m_gainRiderWindowGroup->OnValueChange = [this](float v) {
        m_gainRiderWindow = v;
        m_processor.setGainRiderWindow(v);
        if (m_waveformWidget) m_waveformWidget->setGainRiderWindow(v);
        RefreshGainRider();
        saveConfigToIni();
    };
    m_gainRiderWindowGroup->Show(false);

    m_gainRiderInvertCheckbox = new wxCheckBox(m_scrolledSettings, wxID_ANY, "Invert Effect");
    m_gainRiderInvertCheckbox->Bind(wxEVT_CHECKBOX, &MainWindow::OnGainRiderInvertToggled, this);
    riderGroupSizer->Add(m_gainRiderInvertCheckbox, 0, wxEXPAND | wxALL, 2);

    controlsSizer->Add(riderGroupSizer, 0, wxEXPAND | wxALL, 5);

    // 4. Compressor Group
    wxStaticBoxSizer* compGroupSizer = new wxStaticBoxSizer(wxVERTICAL, m_scrolledSettings, ConfigAdvanced::Labels::Compressor);
    m_compressorActiveCheckbox = new wxCheckBox(m_scrolledSettings, wxID_ANY, "Active");
    m_compressorActiveCheckbox->SetMinSize(wxSize(ConfigAdvanced::Sizes::CheckboxMinWidth, -1));
    m_compressorActiveCheckbox->SetValue(m_compressorEnabled);
    m_compressorActiveCheckbox->Bind(wxEVT_CHECKBOX, &MainWindow::OnCompressorActiveToggled, this);
    compGroupSizer->Add(m_compressorActiveCheckbox, 0, wxALL, 2);

    m_compAmountGroup = new ControlGroup(m_scrolledSettings, compGroupSizer, "Amount", ConfigAdvanced::Ranges::CompThresholdMin, 1.0f, m_currentComp, 0.05f, 2, 100.0f); // Amount is actually threshold for this specific simplified slider
    m_compAmountGroup->OnValueChange = [this](float v) {
        m_currentComp = v;
        m_processor.setPostCompAmount(v);
        if (m_playerDevice) m_playerDevice->setPostCompAmount(v);
        if (m_waveformWidget) m_waveformWidget->setPostCompAmount(v);
        saveConfigToIni();
        TriggerLUFSUpdate();
    };

    m_compAttackGroup = new ControlGroup(m_scrolledSettings, compGroupSizer, ConfigAdvanced::Labels::Attack, ConfigAdvanced::Ranges::CompAttackMin, ConfigAdvanced::Ranges::CompAttackMax, m_compAttack, 0.1f, 1, 10.0f);
    m_compAttackGroup->OnValueChange = [this](float v) {
        m_compAttack = v;
        m_processor.setCompAttack(v);
        if (m_playerDevice) m_playerDevice->setCompAttack(v);
        if (m_waveformWidget) m_waveformWidget->setCompAttack(v);
        saveConfigToIni();
        TriggerLUFSUpdate();
    };
    m_compAttackGroup->Show(false);

    m_compReleaseGroup = new ControlGroup(m_scrolledSettings, compGroupSizer, ConfigAdvanced::Labels::Release, ConfigAdvanced::Ranges::CompReleaseMin, ConfigAdvanced::Ranges::CompReleaseMax, m_compRelease, 1.0f, 0, 1.0f);
    m_compReleaseGroup->OnValueChange = [this](float v) {
        m_compRelease = v;
        m_processor.setCompRelease(v);
        if (m_playerDevice) m_playerDevice->setCompRelease(v);
        if (m_waveformWidget) m_waveformWidget->setCompRelease(v);
        saveConfigToIni();
        TriggerLUFSUpdate();
    };
    m_compReleaseGroup->Show(false);

    m_compRatioGroup = new ControlGroup(m_scrolledSettings, compGroupSizer, ConfigAdvanced::Labels::Ratio, ConfigAdvanced::Ranges::CompRatioMin, ConfigAdvanced::Ranges::CompRatioMax, m_compMaxRatio, 1.0f, 1, 10.0f);
    m_compRatioGroup->OnValueChange = [this](float v) {
        m_compMaxRatio = v;
        m_processor.setCompMaxRatio(v);
        if (m_playerDevice) m_playerDevice->setCompMaxRatio(v);
        if (m_waveformWidget) m_waveformWidget->setCompMaxRatio(v);
        saveConfigToIni();
        TriggerLUFSUpdate();
    };
    m_compRatioGroup->Show(false);

    m_compThresholdGroup = new ControlGroup(m_scrolledSettings, compGroupSizer, ConfigAdvanced::Labels::Threshold, ConfigAdvanced::Ranges::CompThresholdMin, ConfigAdvanced::Ranges::CompThresholdMax, m_compMaxThreshold, 1.0f, 1, 10.0f);
    m_compThresholdGroup->OnValueChange = [this](float v) {
        m_compMaxThreshold = v;
        m_processor.setCompMaxThreshold(v);
        if (m_playerDevice) m_playerDevice->setCompMaxThreshold(v);
        if (m_waveformWidget) m_waveformWidget->setCompMaxThreshold(v);
        saveConfigToIni();
        TriggerLUFSUpdate();
    };
    m_compThresholdGroup->Show(false);

    m_compInvertCheckbox = new wxCheckBox(m_scrolledSettings, wxID_ANY, "Invert Effect");
    m_compInvertCheckbox->Bind(wxEVT_CHECKBOX, &MainWindow::OnCompInvertToggled, this);
    compGroupSizer->Add(m_compInvertCheckbox, 0, wxEXPAND | wxALL, 2);

    controlsSizer->Add(compGroupSizer, 0, wxEXPAND | wxALL, 5);
    
    // Sync all params to waveform widget initially
    m_waveformWidget->setGateAttack(m_gateAttack);
    m_waveformWidget->setGateHold(m_gateHold);
    m_waveformWidget->setGateRelease(m_gateRelease);
    m_waveformWidget->setNoiseGateThreshold(m_currentGate);
    m_waveformWidget->setPhaseRotationAmount(m_phaseRotationAmount);
    m_waveformWidget->setNoiseGateReduction(m_currentGateReduction);
    m_waveformWidget->setCompAttack(m_compAttack);
    m_waveformWidget->setCompRelease(m_compRelease);
    m_waveformWidget->setCompMaxRatio(m_compMaxRatio);
    m_waveformWidget->setCompMaxThreshold(m_compMaxThreshold);
    m_waveformWidget->setPostCompAmount(m_currentComp);
    m_waveformWidget->setLimiterAttack(m_limiterAttack);
    m_waveformWidget->setLimiterRelease(m_limiterRelease);
    m_waveformWidget->setLimiterGain(m_currentLimiterGain);
    m_waveformWidget->setLimiterThreshold(m_currentLimiterThreshold);
    m_waveformWidget->setGainRiderAttack(m_gainRiderAttack);
    m_waveformWidget->setGainRiderRelease(m_gainRiderRelease);
    m_waveformWidget->setGainRiderWindow(m_gainRiderWindow);
    m_waveformWidget->setGainRiderSlewRate(m_gainRiderSlew);
    m_waveformWidget->setPhaseRotationAmount(m_phaseRotationAmount);
    m_waveformWidget->setPhaseRotationEnabled(m_phaseRotationEnabled);
    m_waveformWidget->setGainRiderLookahead(m_gainRiderLookahead);
    m_waveformWidget->setLimiterLookahead(m_limiterLookahead);
    m_waveformWidget->setLcFreq(m_lcFreq);
    m_waveformWidget->setHcFreq(m_hcFreq);
    m_waveformWidget->setMcFreq(m_mcFreq);
    m_waveformWidget->setMcGain(m_mcGain);
    m_waveformWidget->setMcQ(m_mcQ);
    m_waveformWidget->setMcCompensation(m_mcCompensation);
    m_waveformWidget->setDefaultFilterQ(m_defaultFilterQ);
    m_waveformWidget->setEffectsActive(m_processor.isEffectsEnabled());
    m_waveformWidget->setGainRiderInverted(m_processor.isGainRiderInverted());
    m_waveformWidget->setCompInverted(m_processor.isCompInverted());
    
    // Initial sync for Hilbert
    m_processor.setPhaseRotationAmount(m_phaseRotationAmount);

    // 5. Limiter Group
    wxStaticBoxSizer* limiterGroupSizer = new wxStaticBoxSizer(wxVERTICAL, m_scrolledSettings, ConfigAdvanced::Labels::Limiter);
    m_limiterActiveCheckbox = new wxCheckBox(m_scrolledSettings, wxID_ANY, "Active");
    m_limiterActiveCheckbox->SetMinSize(wxSize(ConfigAdvanced::Sizes::CheckboxMinWidth, -1));
    m_limiterActiveCheckbox->SetValue(m_limiterEnabled);
    m_limiterActiveCheckbox->Bind(wxEVT_CHECKBOX, &MainWindow::OnLimiterActiveToggled, this);
    limiterGroupSizer->Add(m_limiterActiveCheckbox, 0, wxALL, 2);
    
    m_limiterCeilingGroup = new ControlGroup(m_scrolledSettings, limiterGroupSizer, ConfigAdvanced::Labels::Ceiling, ConfigAdvanced::Ranges::LimiterThresholdMin, ConfigAdvanced::Ranges::LimiterThresholdMax, m_currentLimiterThreshold, 0.1f, 1, 10.0f);
    m_limiterCeilingGroup->OnValueChange = [this](float v) {
        m_currentLimiterThreshold = v;
        m_processor.setLimiterThreshold(v);
        if (m_playerDevice) m_playerDevice->setLimiterThreshold(v);
        if (m_waveformWidget) m_waveformWidget->setLimiterThreshold(v);
        saveConfigToIni();
        TriggerLUFSUpdate();
    };

    m_limiterGainGroup = new ControlGroup(m_scrolledSettings, limiterGroupSizer, "Gain", ConfigAdvanced::Ranges::LimiterGainMin, ConfigAdvanced::Ranges::LimiterGainMax, m_currentLimiterGain, 0.5f, 1, 10.0f);
    m_limiterGainGroup->OnValueChange = [this](float v) {
        m_currentLimiterGain = v;
        m_processor.setLimiterGain(v);
        if (m_playerDevice) m_playerDevice->setLimiterGain(v);
        if (m_waveformWidget) m_waveformWidget->setLimiterGain(v);
        saveConfigToIni();
        TriggerLUFSUpdate();
    };

    m_limiterAttackGroup = new ControlGroup(m_scrolledSettings, limiterGroupSizer, "Attack", ConfigAdvanced::Ranges::LimiterAttackMin, ConfigAdvanced::Ranges::LimiterAttackMax, m_limiterAttack, 0.01f, 2, 100.0f);
    m_limiterAttackGroup->OnValueChange = [this](float v) {
        m_limiterAttack = v;
        m_processor.setLimiterAttack(v);
        if (m_playerDevice) m_playerDevice->setLimiterAttack(v);
        if (m_waveformWidget) m_waveformWidget->setLimiterAttack(v);
        saveConfigToIni();
        TriggerLUFSUpdate();
    };
    m_limiterAttackGroup->Show(false);

    m_limiterReleaseGroup = new ControlGroup(m_scrolledSettings, limiterGroupSizer, "Release", ConfigAdvanced::Ranges::LimiterReleaseMin, ConfigAdvanced::Ranges::LimiterReleaseMax, m_limiterRelease, 1.0f, 0, 1.0f);
    m_limiterReleaseGroup->OnValueChange = [this](float v) {
        m_limiterRelease = v;
        m_processor.setLimiterRelease(v);
        if (m_playerDevice) m_playerDevice->setLimiterRelease(v);
        if (m_waveformWidget) m_waveformWidget->setLimiterRelease(v);
        saveConfigToIni();
        TriggerLUFSUpdate();
    };
    m_limiterReleaseGroup->Show(false);

    m_limiterLookaheadGroup = new ControlGroup(m_scrolledSettings, limiterGroupSizer, "Lookahead", 0.1f, 50.0f, m_limiterLookahead, 0.1f, 1, 10.0f);
    m_limiterLookaheadGroup->OnValueChange = [this](float v) {
        m_limiterLookahead = v;
        m_processor.setLimiterLookahead(v);
        if (m_playerDevice) m_playerDevice->setLimiterLookahead(v);
        if (m_waveformWidget) m_waveformWidget->setLimiterLookahead(v);
        saveConfigToIni();
        TriggerLUFSUpdate();
    };
    m_limiterLookaheadGroup->Show(false);

    controlsSizer->Add(limiterGroupSizer, 0, wxEXPAND | wxALL, 5);


    m_scrolledSettings->SetSizer(controlsSizer);
    m_scrolledSettings->FitInside(); 
    bottomAreaSizer->Add(m_scrolledSettings, 1, wxEXPAND);
    mainSizer->Add(bottomAreaSizer, 0, wxALL | wxEXPAND, 5);

    // Progress Bar (hidden by default)
    m_progressBar = new wxGauge(panel, wxID_ANY, 100);
    m_progressBar->Hide();
    mainSizer->Add(m_progressBar, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    panel->SetSizer(mainSizer);
    SetDropTarget(new FileDropTarget(this));
    
    syncAdvancedModeUI();
}

void MainWindow::maDataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    AudioPlayerDevice* player = (AudioPlayerDevice*)pDevice->pUserData;
    if (player) {
        player->dataCallback(pOutput, pInput, frameCount);
    }
}

void MainWindow::setupAudioContext() {
    stopPlayback();
    if (m_deviceInitialized) {
        ma_device_uninit(&m_device);
        m_deviceInitialized = false;
    }

    m_playerDevice = std::make_unique<AudioPlayerDevice>(&m_processor.getPcmData(),
                                                        m_processor.getSampleRate(), 
                                                        m_processor.getChannels());
    m_playerDevice->updateGainEnvelope(m_processor.getGainEnvelope());
    m_playerDevice->setLimiterThreshold(m_currentLimiterThreshold);
    m_playerDevice->setLowCutEnabled(m_processor.isLowCutEnabled());
    m_playerDevice->setHighCutEnabled(m_processor.isHighCutEnabled());
    m_playerDevice->setMidCutEnabled(m_processor.isMidCutEnabled());
    m_playerDevice->setPhaseRotationEnabled(m_processor.isPhaseRotationEnabled());
    m_playerDevice->setPhaseRotationAmount(m_phaseRotationAmount);
    m_playerDevice->setCompInverted(m_compInvertCheckbox->GetValue());
    m_waveformWidget->setGainRiderInverted(m_gainRiderInvertCheckbox->GetValue());
    m_waveformWidget->setCompInverted(m_compInvertCheckbox->GetValue());
    m_waveformWidget->setLimiterThreshold(m_currentLimiterThreshold);
    m_waveformWidget->setPhaseRotationEnabled(m_phaseRotationEnabled);
    m_waveformWidget->setPhaseRotationAmount(m_phaseRotationAmount);
    
    // We can't update continuous envelope easily to the player device 
    // unless we use a shared pointer or similar. 
    // For now, let's just make sure the processor has it.
    m_processor.calculateGainRiderEnvelope();

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = m_processor.getChannels();
    config.sampleRate        = m_processor.getSampleRate();
    config.dataCallback      = maDataCallback;
    config.pUserData         = m_playerDevice.get();

    if (ma_device_init(NULL, &config, &m_device) != MA_SUCCESS) {
        wxMessageBox("Failed to initialize audio device.", "Error", wxOK | wxICON_ERROR);
    } else {
        m_deviceInitialized = true;
    }
}

void MainWindow::stopPlayback() {
    m_playbackTimer.Stop();
    if (m_deviceInitialized) {
        ma_device_stop(&m_device);
    }
}

void MainWindow::OnLoadAudio(wxCommandEvent& WXUNUSED(event)) {
    wxFileDialog openFileDialog(this, "Open Audio File", "", "", "Audio Files (*.wav;*.mp3;*.m4a;*.aac;*.ogg;*.flac)|*.wav;*.mp3;*.m4a;*.aac;*.ogg;*.flac", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (openFileDialog.ShowModal() == wxID_CANCEL) return;

    LoadAudioFile(openFileDialog.GetPath());
}

void MainWindow::LoadAudioFile(const wxString& path) {
    // 1. STOP ALL THREADS/CALLBACKS BEFORE DATA MUTATION
    stopPlayback();
    m_analysisRequestId++; 
    if (m_analysisThread.joinable()) {
        m_analysisThread.join();
    }
    // resetSettings(); // REMOVED: Preserve settings across file loads
    m_playBtn->SetLabel("Play");
    m_playBtn->Enable(false);
    m_exportBtn->Enable(false);
    m_waveformWidget->setAudioData(nullptr, nullptr, 0, 44100, 2);
    m_processor.decodeAudio(std::string(path.mb_str()));
    
    // Update window title with filename and channel count
    wxFileName fn(path);
    wxString chanInfo;
    int channels = m_processor.getChannels();
    if (channels == 1) chanInfo = " (Mono)";
    else if (channels == 2) chanInfo = " (Stereo)";
    else chanInfo = wxString::Format(" (%d Channels)", channels);

    SetTitle(ConfigAdvanced::Labels::AppTitlePrefix + fn.GetFullName() + chanInfo);

    m_waveformWidget->setAudioData(&m_processor.getPcmData(), &m_processor.getGainEnvelope(), m_processor.getDuration(), m_processor.getSampleRate(), m_processor.getChannels());

    syncUiFromState(); // Ensure all layers (processor, waveform, player) are synced with CURRENT UI
    TriggerLUFSUpdate();
}

void MainWindow::TriggerLUFSUpdate() {
    m_analysisRequestId++;
    int currentId = m_analysisRequestId.load();
    
    if (m_analysisThread.joinable()) {
        m_analysisThread.join();
    }
    
    {
        std::lock_guard<std::mutex> lock(m_resultsMutex);
        m_latestResults.valid = false;
    }
    
    m_animationState = 0;
    m_analysisAnimationTimer.Start(300);
    SetStatusText("Ready. Calculating LUFS...");

    m_analysisThread = std::thread(&MainWindow::RunAnalysisTask, this, currentId);
}

void MainWindow::RunAnalysisTask(int requestId) {
    if (m_processor.getPcmData().empty()) return;

    lufs::LoudnessAnalyzer analyzer(m_processor.getChannels(), m_processor.getSampleRate());
    DSPState dspState;
    
    size_t totalFrames = m_processor.getPcmData().size() / m_processor.getChannels();
    size_t framesDone = 0;
    const size_t CHUNK_SIZE = 4096;
    std::vector<float> buffer(CHUNK_SIZE * m_processor.getChannels());

    while (framesDone < totalFrames) {
        if (m_analysisRequestId.load() != requestId) return; // Cancel if new request came in

        size_t toProcess = std::min(CHUNK_SIZE, totalFrames - framesDone);
        m_processor.processBlock(&m_processor.getPcmData()[framesDone * m_processor.getChannels()], buffer.data(), toProcess, dspState);
        analyzer.process(buffer.data(), toProcess);
        framesDone += toProcess;
    }

    if (m_analysisRequestId.load() != requestId) return;

    LUFSResults results;
    results.lufs = analyzer.getIntegratedLoudness();
    results.lra = analyzer.getLoudnessRange();
    results.tp = analyzer.getTruePeak();
    results.valid = true;


    {
        std::lock_guard<std::mutex> lock(m_resultsMutex);
        m_latestResults = results;
    }
}

void MainWindow::OnLUFSAnimationTimer(wxTimerEvent& event) {
    std::lock_guard<std::mutex> lock(m_resultsMutex);
    if (m_latestResults.valid) {
        m_analysisAnimationTimer.Stop();
        wxString rawSuffix = m_processor.isEffectsEnabled() ? "" : " (RAW)";
        wxString status = wxString::Format("Ready. LUFS: %.1f LRA: %.1f TP: %.1f%s", 
                                          m_latestResults.lufs, m_latestResults.lra, m_latestResults.tp, rawSuffix);
        SetStatusText(status);
        return;
    }

    m_animationState = (m_animationState + 1) % 4;
    wxString dots = "";
    for(int i=0; i<m_animationState; ++i) dots += ".";
    SetStatusText("Ready. Calculating LUFS" + dots);
}

void MainWindow::RefreshGainRider() {
    m_processor.calculateGainRiderEnvelope();
    if (m_playerDevice) {
        m_playerDevice->updateGainEnvelope(m_processor.getGainEnvelope());
    }
    if (m_waveformWidget) {
        m_waveformWidget->updateCache();
        m_waveformWidget->Refresh();
    }
    TriggerLUFSUpdate();
}

void MainWindow::resetSettings() {
    m_currentTarget = Config::GainRider::DefaultTargetDb;
    m_currentComp = 0.0f;
    m_currentGate = Config::NoiseGate::ThresholdOffDb;
    m_currentGateReduction = Config::NoiseGate::DefaultReductionDb;
    m_currentGainRiderRange = Config::GainRider::DefaultUpperRangeDb;
    m_currentLimiterGain = Config::Compressor::DefaultGainDb;
    m_currentLimiterThreshold = Config::Limiter::ThresholdDb;

    m_lcFreq = Config::Filter::LowCutFreq;
    m_hcFreq = Config::Filter::HighCutFreq;
    m_mcFreq = Config::Filter::MidCutFreq;
    m_mcGain = Config::Filter::MidCutGainDb;
    m_phaseRotationAmount = Config::Filter::PhaseAmount;
    m_phaseRotationEnabled = false;

    m_gateAttack = Config::NoiseGate::AttackMs;
    m_gateHold = Config::NoiseGate::HoldMs;
    m_gateRelease = Config::NoiseGate::ReleaseMs;

    m_compAttack = Config::Compressor::AttackMs;
    m_compRelease = Config::Compressor::ReleaseMs;
    m_compMaxRatio = Config::Compressor::MaxRatio;
    m_compMaxThreshold = Config::Compressor::MaxThresholdDb;

    m_limiterAttack = Config::Limiter::AttackMs;
    m_limiterRelease = Config::Limiter::ReleaseMs;
    m_limiterLookahead = Config::Limiter::LookaheadMs;

    m_gainRiderAttack = Config::GainRider::AttackMs;
    m_gainRiderRelease = Config::GainRider::ReleaseMs;
    m_gainRiderLookahead = Config::GainRider::LookaheadMs;
    m_gainRiderWindow = Config::GainRider::AnalysisWindowMs;

    // Update internal tracking variables to match defaults
    m_effectsEnabled = true;
    m_lowCutEnabled = Config::Filter::LowCutEnabled;
    m_midCutEnabled = false;
    m_highCutEnabled = Config::Filter::HighCutEnabled;
    m_noiseGateEnabled = Config::NoiseGate::DefaultEnabled;
    m_gainRiderEnabled = Config::GainRider::DefaultEnabled;
    m_compressorEnabled = Config::Compressor::DefaultEnabled;
    m_limiterEnabled = Config::Limiter::DefaultEnabled;
    
    m_processor.setGainRiderInverted(false);
    m_processor.setCompInverted(false);
    m_processor.setEffectsEnabled(true);
    m_processor.setLowCutEnabled(m_lowCutEnabled);
    m_processor.setMidCutEnabled(false);
    m_processor.setHighCutEnabled(m_highCutEnabled);
    m_processor.setNoiseGateEnabled(m_noiseGateEnabled);
    m_processor.setGainRiderEnabled(m_gainRiderEnabled);
    m_processor.setCompressorEnabled(m_compressorEnabled);
    m_processor.setLimiterEnabled(m_limiterEnabled);

    syncUiFromState();
}

void MainWindow::OnPhaseRotationToggled(wxCommandEvent& event) {
    bool checked = event.IsChecked();
    m_phaseRotationEnabled = checked;
    m_processor.setPhaseRotationEnabled(checked);
    m_waveformWidget->setPhaseRotationEnabled(checked);
    if (m_playerDevice) m_playerDevice->setPhaseRotationEnabled(checked);
    saveConfigToIni();
    syncStageVisibility();
    TriggerLUFSUpdate();
}

void MainWindow::OnGainViewModeChanged(wxCommandEvent& event) {
    int id = event.GetId();
    WaveformWidget::GainViewMode mode = WaveformWidget::GainViewMode::Combined;
    
    if (id == ID_GAIN_VIEW_GATE) mode = WaveformWidget::GainViewMode::Gate;
    else if (id == ID_GAIN_VIEW_RIDER) mode = WaveformWidget::GainViewMode::GainRider;
    else if (id == ID_GAIN_VIEW_COMP) mode = WaveformWidget::GainViewMode::Compressor;
    else if (id == ID_GAIN_VIEW_LIMITER) mode = WaveformWidget::GainViewMode::Limiter;
    
    m_waveformWidget->setGainViewMode(mode);
}

void MainWindow::OnPlayPause(wxCommandEvent& WXUNUSED(event)) {
    if (!m_deviceInitialized) return;

    if (ma_device_get_state(&m_device) == ma_device_state_started) {
        ma_device_stop(&m_device);
        m_playbackTimer.Stop();
        m_playBtn->SetLabel(ConfigAdvanced::Labels::Play);
    } else {
        if (ma_device_start(&m_device) == MA_SUCCESS) {
            m_playbackTimer.Start(30);
            m_playBtn->SetLabel(ConfigAdvanced::Labels::Pause);
        }
    }
}

void MainWindow::OnPlaybackTimer(wxTimerEvent& WXUNUSED(event)) {
    if (!m_playerDevice) return;
    long long sampleIdx = m_playerDevice->currentSampleIndex();
    float timeSecs = static_cast<float>(sampleIdx) / (m_processor.getSampleRate() * m_processor.getChannels());
    m_waveformWidget->setPlayhead(timeSecs);
    
    
    // Auto-stop at end
    if (timeSecs >= m_processor.getDuration()) {
        stopPlayback();
        m_playBtn->SetLabel(ConfigAdvanced::Labels::Play);
        m_playerDevice->seekToSample(0);
        m_waveformWidget->setPlayhead(0);
    } else {
        // Auto-scroll logic if playhead goes out of view
        float offset = m_waveformWidget->getViewportOffset();
        float zoom = m_waveformWidget->getZoomLevel();
        float viewDuration = m_processor.getDuration() / zoom;
        
        if (timeSecs > offset + viewDuration || timeSecs < offset) {
            float newOffset = std::clamp(timeSecs - (viewDuration / 2.0f), 0.0f, m_processor.getDuration() - viewDuration);
            m_waveformWidget->setViewportOffset(newOffset);
            m_viewportScroll->SetThumbPosition(static_cast<int>(newOffset * 1000));
        }
    }
}

void MainWindow::OnSeekRequested(float timeSeconds) {
    if (!m_playerDevice) return;
    long long targetSample = static_cast<long long>(timeSeconds * m_processor.getSampleRate() * m_processor.getChannels());
    
    bool wasPlaying = (m_deviceInitialized && ma_device_get_state(&m_device) == ma_device_state_started);
    if (wasPlaying) ma_device_stop(&m_device);

    m_playerDevice->seekToSample(targetSample);
    m_waveformWidget->setPlayhead(timeSeconds);

    if (wasPlaying) {
        ma_device_start(&m_device);
    }
}


float MainWindow::SliderValueToZoom(int val) {
    if (m_processor.getDuration() <= 0) return 1.0f;
    float duration = m_processor.getDuration();
    float minZoom = 1.0f;
    float maxZoom = std::max(1.01f, duration / Config::UI::ZoomMinWindowSec);
    
    // Logarithmic interpolation: zoom = min * (max/min) ^ (val / slider_range)
    float t = static_cast<float>(val) / 1000.0f;
    return minZoom * std::pow(maxZoom / minZoom, t);
}

int MainWindow::ZoomToSliderValue(float zoom) {
    if (m_processor.getDuration() <= 0) return 0;
    float duration = m_processor.getDuration();
    float minZoom = 1.0f;
    float maxZoom = std::max(1.01f, duration / Config::UI::ZoomMinWindowSec);
    
    if (zoom <= minZoom) return 0;
    if (zoom >= maxZoom) return 1000;
    
    // t = log(zoom/min) / log(max/min)
    float t = std::log(zoom / minZoom) / std::log(maxZoom / minZoom);
    return static_cast<int>(t * 1000.0f);
}

void MainWindow::updateViewportScroll() {
    float range = m_processor.getDuration();
    float zoom = m_waveformWidget->getZoomLevel();
    if (range > 0 && zoom > 0) {
        float thumbSize = range / zoom;
        m_viewportScroll->SetScrollbar(static_cast<int>(m_waveformWidget->getViewportOffset() * 1000), static_cast<int>(thumbSize * 1000), static_cast<int>(range * 1000), static_cast<int>(thumbSize * 1000));
    }
}

void MainWindow::OnZoomScroll(wxScrollEvent& event) {
    float zoom = SliderValueToZoom(event.GetPosition());
    m_waveformWidget->setZoomLevel(zoom);
    updateViewportScroll();
}

void MainWindow::OnViewportScroll(wxScrollEvent& event) {
    float offset = event.GetPosition() / 1000.0f;
    m_waveformWidget->setViewportOffset(offset);
}


void MainWindow::OnLowCutToggled(wxCommandEvent& event) {
    m_lowCutEnabled = event.IsChecked();
    m_processor.setLowCutEnabled(m_lowCutEnabled);
    m_waveformWidget->setLowCutEnabled(m_lowCutEnabled);
    if (m_playerDevice) m_playerDevice->setLowCutEnabled(m_lowCutEnabled);
    syncStageVisibility();
    saveConfigToIni();
    TriggerLUFSUpdate();
}

void MainWindow::OnHighCutToggled(wxCommandEvent& event) {
    m_highCutEnabled = event.IsChecked();
    m_processor.setHighCutEnabled(m_highCutEnabled);
    m_waveformWidget->setHighCutEnabled(m_highCutEnabled);
    if (m_playerDevice) m_playerDevice->setHighCutEnabled(m_highCutEnabled);
    syncStageVisibility();
    saveConfigToIni();
    TriggerLUFSUpdate();
}

void MainWindow::OnMidCutToggled(wxCommandEvent& event) {
    m_midCutEnabled = event.IsChecked();
    m_processor.setMidCutEnabled(m_midCutEnabled);
    m_waveformWidget->setMidCutEnabled(m_midCutEnabled);
    if (m_playerDevice) m_playerDevice->setMidCutEnabled(m_midCutEnabled);
    syncStageVisibility();
    TriggerLUFSUpdate();
}

void MainWindow::OnGainRiderInvertToggled(wxCommandEvent& event) {
    bool checked = event.IsChecked();
    m_processor.setGainRiderInverted(checked);
    m_waveformWidget->setGainRiderInverted(checked);
    if (m_playerDevice) m_playerDevice->setGainRiderInverted(checked);
    TriggerLUFSUpdate();
}

void MainWindow::OnCompInvertToggled(wxCommandEvent& event) {
    bool checked = event.IsChecked();
    m_processor.setCompInverted(checked);
    m_waveformWidget->setCompInverted(checked);
    if (m_playerDevice) m_playerDevice->setCompInverted(checked);
    TriggerLUFSUpdate();
}

void MainWindow::OnExportAudio(wxCommandEvent& WXUNUSED(event)) {
    wxFileDialog saveFileDialog(this, "Export WAV", "", "", "WAV Files (*.wav)|*.wav", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (saveFileDialog.ShowModal() == wxID_CANCEL) return;

    SetStatusText("Exporting...");
    m_progressBar->SetValue(0);
    m_progressBar->Show();
    m_progressBar->GetParent()->Layout();
    
    m_processor.onExportProgress = [this](float percent) {
        m_progressBar->SetValue(static_cast<int>(percent * 100));
        wxYield();
    };

    bool ok = m_processor.exportWav(std::string(saveFileDialog.GetPath().mb_str()));
    
    m_progressBar->Hide();
    m_progressBar->GetParent()->Layout();
    
    if (ok) {
        SetStatusText("Export Successful");
    } else {
        wxMessageBox("Failed to export audio file.", "Export Error", wxOK | wxICON_ERROR);
        SetStatusText("Export Failed");
    }
}

void MainWindow::OnKeyDown(wxKeyEvent& event) {
    if (event.GetKeyCode() == WXK_SPACE) {
        if (m_playBtn->IsEnabled()) {
            wxCommandEvent dummy;
            OnPlayPause(dummy);
        }
    } else {
        event.Skip();
    }
}

void MainWindow::OnEffectsActiveToggled(wxCommandEvent& event) {
    m_effectsEnabled = event.IsChecked();
    m_processor.setEffectsEnabled(m_effectsEnabled);
    syncUiFromState();
    saveConfigToIni();
}


void MainWindow::OnLoadSettings(wxCommandEvent& WXUNUSED(event)) {
    wxFileDialog openFileDialog(this, "Load Settings", "", "", "INI files (*.ini)|*.ini", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (openFileDialog.ShowModal() == wxID_CANCEL) return;
    loadConfigFromIni(openFileDialog.GetPath());
    TriggerLUFSUpdate();
    Refresh();
}

void MainWindow::OnSaveSettings(wxCommandEvent& WXUNUSED(event)) {
    wxFileDialog saveFileDialog(this, "Save Settings", "", "config.ini", "INI files (*.ini)|*.ini", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (saveFileDialog.ShowModal() == wxID_CANCEL) return;
    saveConfigToIni(saveFileDialog.GetPath());
}

void MainWindow::OnRestoreDefaults(wxCommandEvent& WXUNUSED(event)) {
    if (wxMessageBox("Restore all settings to defaults?", "Confirm", wxYES_NO | wxICON_QUESTION) == wxYES) {
        resetSettings();
        saveConfigToIni(); // Save to default location
        TriggerLUFSUpdate();
        Refresh();
    }
}

void MainWindow::OnExit(wxCommandEvent& WXUNUSED(event)) {
    Close(true);
}

void MainWindow::updateEffectsUIState(bool enabled) {
    m_lowCutCheckbox->Enable(enabled);
    m_highCutCheckbox->Enable(enabled);
    m_midCutCheckbox->Enable(enabled);
    m_phaseRotationCheckbox->Enable(enabled);
    m_gateThreshGroup->Enable(enabled);
    m_gateReductionGroup->Enable(enabled);
    m_targetGroup->Enable(enabled);
    m_rangeGroup->Enable(enabled);
    m_compAmountGroup->Enable(enabled);
    m_limiterGainGroup->Enable(enabled);
    m_limiterCeilingGroup->Enable(enabled);
    m_gainRiderInvertCheckbox->Enable(enabled);
    m_compInvertCheckbox->Enable(enabled);
    m_sensitivityGroup->Enable(enabled);

    // Advanced Sliders
    m_gateAttackGroup->Enable(enabled);
    m_gateHoldGroup->Enable(enabled);
    m_gateReleaseGroup->Enable(enabled);
    m_compAttackGroup->Enable(enabled);
    m_compReleaseGroup->Enable(enabled);
    m_compRatioGroup->Enable(enabled);
    m_compThresholdGroup->Enable(enabled);
    m_limiterAttackGroup->Enable(enabled);
    m_limiterReleaseGroup->Enable(enabled);
    m_phaseAmountGroup->Enable(enabled);
    m_limiterLookaheadGroup->Enable(enabled);
    m_gainRiderAttackGroup->Enable(enabled);
    m_gainRiderReleaseGroup->Enable(enabled);
    m_gainRiderLookaheadGroup->Enable(enabled);
    m_gainRiderWindowGroup->Enable(enabled);
    m_lcFreqGroup->Enable(enabled);
    m_hcFreqGroup->Enable(enabled);
    m_mcFreqGroup->Enable(enabled);
    m_mcGainGroup->Enable(enabled);
}

void MainWindow::OnToggleAdvanced(wxCommandEvent& event) {
    m_advancedMode = event.IsChecked();
    syncAdvancedModeUI();
}

void MainWindow::syncAdvancedModeUI() {
    // Sync Menu Item
    wxMenuBar* menuBar = GetMenuBar();
    if (menuBar) {
        menuBar->Check(ID_TOGGLE_ADVANCED, m_advancedMode);
    }
    
    syncStageVisibility();
}

void MainWindow::OnNoiseGateActiveToggled(wxCommandEvent& event) {
    m_noiseGateEnabled = event.IsChecked();
    m_processor.setNoiseGateEnabled(m_noiseGateEnabled);
    if (m_playerDevice) m_playerDevice->setNoiseGateEnabled(m_noiseGateEnabled);
    if (m_waveformWidget) m_waveformWidget->setNoiseGateEnabled(m_noiseGateEnabled);
    syncStageVisibility();
    saveConfigToIni();
    TriggerLUFSUpdate();
}

void MainWindow::OnGainRiderActiveToggled(wxCommandEvent& event) {
    m_gainRiderEnabled = event.IsChecked();
    m_processor.setGainRiderEnabled(m_gainRiderEnabled);
    if (m_playerDevice) m_playerDevice->setGainRiderEnabled(m_gainRiderEnabled);
    if (m_waveformWidget) m_waveformWidget->setGainRiderEnabled(m_gainRiderEnabled);
    syncStageVisibility();
    saveConfigToIni();
    RefreshGainRider();
}

void MainWindow::OnCompressorActiveToggled(wxCommandEvent& event) {
    m_compressorEnabled = event.IsChecked();
    m_processor.setCompressorEnabled(m_compressorEnabled);
    if (m_playerDevice) m_playerDevice->setCompressorEnabled(m_compressorEnabled);
    if (m_waveformWidget) m_waveformWidget->setCompressorEnabled(m_compressorEnabled);
    syncStageVisibility();
    saveConfigToIni();
    TriggerLUFSUpdate();
}

void MainWindow::OnLimiterActiveToggled(wxCommandEvent& event) {
    m_limiterEnabled = event.IsChecked();
    m_processor.setLimiterEnabled(m_limiterEnabled);
    if (m_playerDevice) m_playerDevice->setLimiterEnabled(m_limiterEnabled);
    if (m_waveformWidget) m_waveformWidget->setLimiterEnabled(m_limiterEnabled);
    syncStageVisibility();
    saveConfigToIni();
    TriggerLUFSUpdate();
}

void MainWindow::syncStageVisibility() {
    // 1. Equalizer / Filters
    if (m_lcFreqGroup) m_lcFreqGroup->Show(m_advancedMode && m_lowCutEnabled);
    if (m_hcFreqGroup) m_hcFreqGroup->Show(m_advancedMode && m_highCutEnabled);
    if (m_mcFreqGroup) m_mcFreqGroup->Show(m_advancedMode && m_midCutEnabled);
    if (m_mcGainGroup) m_mcGainGroup->Show(m_advancedMode && m_midCutEnabled);
    
    bool phaseOn = m_phaseRotationEnabled;
    if (m_phaseAmountGroup) m_phaseAmountGroup->Show(phaseOn);

    // 2. Gate
    bool gateActive = m_noiseGateEnabled;
    if (m_gateThreshGroup) m_gateThreshGroup->Show(gateActive);
    if (m_gateReductionGroup) m_gateReductionGroup->Show(gateActive);
    if (m_gateAttackGroup) m_gateAttackGroup->Show(m_advancedMode && gateActive);
    if (m_gateHoldGroup) m_gateHoldGroup->Show(m_advancedMode && gateActive);
    if (m_gateReleaseGroup) m_gateReleaseGroup->Show(m_advancedMode && gateActive);

    // 3. Gain Rider
    bool riderActive = m_gainRiderEnabled;
    if (m_targetGroup) m_targetGroup->Show(riderActive);
    if (m_rangeGroup) m_rangeGroup->Show(riderActive);
    if (m_sensitivityGroup) m_sensitivityGroup->Show(riderActive);
    if (m_gainRiderAttackGroup) m_gainRiderAttackGroup->Show(m_advancedMode && riderActive);
    if (m_gainRiderReleaseGroup) m_gainRiderReleaseGroup->Show(m_advancedMode && riderActive);
    if (m_gainRiderLookaheadGroup) m_gainRiderLookaheadGroup->Show(m_advancedMode && riderActive);
    if (m_gainRiderWindowGroup) m_gainRiderWindowGroup->Show(m_advancedMode && riderActive);
    if (m_gainRiderInvertCheckbox) m_gainRiderInvertCheckbox->Show(riderActive);

    // 4. Compressor
    bool compActive = m_compressorEnabled;
    if (m_compAmountGroup) m_compAmountGroup->Show(compActive);
    if (m_compAttackGroup) m_compAttackGroup->Show(m_advancedMode && compActive);
    if (m_compReleaseGroup) m_compReleaseGroup->Show(m_advancedMode && compActive);
    if (m_compRatioGroup) m_compRatioGroup->Show(m_advancedMode && compActive);
    if (m_compThresholdGroup) m_compThresholdGroup->Show(m_advancedMode && compActive);
    if (m_compInvertCheckbox) m_compInvertCheckbox->Show(compActive);

    // 5. Limiter
    bool limActive = m_limiterEnabled;
    if (m_limiterCeilingGroup) m_limiterCeilingGroup->Show(limActive);
    if (m_limiterAttackGroup) m_limiterAttackGroup->Show(m_advancedMode && limActive);
    if (m_limiterReleaseGroup) m_limiterReleaseGroup->Show(m_advancedMode && limActive);
    if (m_limiterLookaheadGroup) m_limiterLookaheadGroup->Show(m_advancedMode && limActive);

    // Refresh layout of scrollable container
    if (m_scrolledSettings) {
        if (m_scrolledSettings->GetSizer()) {
            m_scrolledSettings->GetSizer()->Layout();
        }
        m_scrolledSettings->FitInside();
    }
    
    Layout();
}

void MainWindow::syncUiFromState() {
    // 1. Update Checkboxes
    if (m_effectsActiveCheckbox) m_effectsActiveCheckbox->SetValue(m_effectsEnabled);
    if (m_lowCutCheckbox) m_lowCutCheckbox->SetValue(m_lowCutEnabled);
    if (m_highCutCheckbox) m_highCutCheckbox->SetValue(m_highCutEnabled);
    if (m_midCutCheckbox) m_midCutCheckbox->SetValue(m_midCutEnabled);
    if (m_phaseRotationCheckbox) m_phaseRotationCheckbox->SetValue(m_phaseRotationEnabled);
    if (m_noiseGateActiveCheckbox) m_noiseGateActiveCheckbox->SetValue(m_noiseGateEnabled);
    if (m_gainRiderActiveCheckbox) m_gainRiderActiveCheckbox->SetValue(m_gainRiderEnabled);
    if (m_compressorActiveCheckbox) m_compressorActiveCheckbox->SetValue(m_compressorEnabled);
    if (m_limiterActiveCheckbox) m_limiterActiveCheckbox->SetValue(m_limiterEnabled);

    // 2. Update ControlGroups (Sliders/Spinboxes)
    if (m_targetGroup) m_targetGroup->SetValue(m_currentTarget);
    if (m_rangeGroup) m_rangeGroup->SetValue(m_currentGainRiderRange);
    if (m_compAmountGroup) m_compAmountGroup->SetValue(m_currentComp);
    if (m_gateThreshGroup) m_gateThreshGroup->SetValue(m_currentGate);
    if (m_gateReductionGroup) m_gateReductionGroup->SetValue(m_currentGateReduction);
    if (m_limiterGainGroup) m_limiterGainGroup->SetValue(m_currentLimiterGain);
    if (m_limiterCeilingGroup) m_limiterCeilingGroup->SetValue(m_currentLimiterThreshold);
    if (m_phaseAmountGroup) m_phaseAmountGroup->SetValue(m_phaseRotationAmount);

    // Advanced Sliders
    if (m_gateAttackGroup) m_gateAttackGroup->SetValue(m_gateAttack);
    if (m_gateHoldGroup) m_gateHoldGroup->SetValue(m_gateHold);
    if (m_gateReleaseGroup) m_gateReleaseGroup->SetValue(m_gateRelease);
    if (m_compAttackGroup) m_compAttackGroup->SetValue(m_compAttack);
    if (m_compReleaseGroup) m_compReleaseGroup->SetValue(m_compRelease);
    if (m_compRatioGroup) m_compRatioGroup->SetValue(m_compMaxRatio);
    if (m_compThresholdGroup) m_compThresholdGroup->SetValue(m_compMaxThreshold);
    if (m_limiterAttackGroup) m_limiterAttackGroup->SetValue(m_limiterAttack);
    if (m_limiterReleaseGroup) m_limiterReleaseGroup->SetValue(m_limiterRelease);
    if (m_limiterLookaheadGroup) m_limiterLookaheadGroup->SetValue(m_limiterLookahead);
    if (m_gainRiderAttackGroup) m_gainRiderAttackGroup->SetValue(m_gainRiderAttack);
    if (m_gainRiderReleaseGroup) m_gainRiderReleaseGroup->SetValue(m_gainRiderRelease);
    if (m_gainRiderLookaheadGroup) m_gainRiderLookaheadGroup->SetValue(m_gainRiderLookahead);
    if (m_gainRiderWindowGroup) m_gainRiderWindowGroup->SetValue(m_gainRiderWindow);

    // 3. Propagate to Waveform & Player Device
    if (m_waveformWidget) {
        m_waveformWidget->setEffectsActive(m_effectsEnabled);
        m_waveformWidget->setLowCutEnabled(m_lowCutEnabled);
        m_waveformWidget->setHighCutEnabled(m_highCutEnabled);
        m_waveformWidget->setMidCutEnabled(m_midCutEnabled);
        m_waveformWidget->setPhaseRotationEnabled(m_phaseRotationEnabled);
        m_waveformWidget->setNoiseGateEnabled(m_noiseGateEnabled);
        m_waveformWidget->setGainRiderEnabled(m_gainRiderEnabled);
        m_waveformWidget->setCompressorEnabled(m_compressorEnabled);
        m_waveformWidget->setLimiterEnabled(m_limiterEnabled);
        m_waveformWidget->setPhaseRotationAmount(m_phaseRotationAmount);
        m_waveformWidget->updateCache();
    }

    if (m_playerDevice) {
        m_playerDevice->setEffectsEnabled(m_effectsEnabled);
        m_playerDevice->setLowCutEnabled(m_lowCutEnabled);
        m_playerDevice->setHighCutEnabled(m_highCutEnabled);
        m_playerDevice->setMidCutEnabled(m_midCutEnabled);
        m_playerDevice->setPhaseRotationEnabled(m_phaseRotationEnabled);
        m_playerDevice->setNoiseGateEnabled(m_noiseGateEnabled);
        m_playerDevice->setGainRiderEnabled(m_gainRiderEnabled);
        m_playerDevice->setCompressorEnabled(m_compressorEnabled);
        m_playerDevice->setLimiterEnabled(m_limiterEnabled);
        m_playerDevice->setPhaseRotationAmount(m_phaseRotationAmount);
    }

    syncStageVisibility();
    TriggerLUFSUpdate();
}
