#pragma once

#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <functional>

class ControlGroup : public wxEvtHandler {
public:
    ControlGroup(wxWindow* parent, wxSizer* targetSizer, const wxString& label, float minVal, float maxVal, float initialVal, float step = 0.1f, int precision = 1, float sliderScale = 10.0f);

    void SetValue(float val);
    float GetValue() const;
    void Enable(bool enabled);
    void Show(bool visible);

    std::function<void(float)> OnValueChange;

private:
    void OnSliderScroll(wxScrollEvent& event);
    void OnSpinChange(wxSpinDoubleEvent& event);

    wxWindow* m_parent;
    wxSizer* m_parentSizer;
    wxBoxSizer* m_rowSizer;
    wxStaticText* m_labelText;
    wxSlider* m_slider;
    wxSpinCtrlDouble* m_spin;
    float m_sliderScale;
};
