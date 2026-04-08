#include "ControlGroup.h"

ControlGroup::ControlGroup(wxWindow* parent, wxSizer* targetSizer, const wxString& label, float minVal, float maxVal, float initialVal, float step, int precision, float sliderScale)
    : m_parent(parent), m_parentSizer(targetSizer), m_sliderScale(sliderScale) 
{
    m_rowSizer = new wxBoxSizer(wxHORIZONTAL);

    m_labelText = new wxStaticText(m_parent, wxID_ANY, label + ":", wxDefaultPosition, wxSize(80, -1));
    m_rowSizer->Add(m_labelText, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    int sliderMin = static_cast<int>(minVal * m_sliderScale);
    int sliderMax = static_cast<int>(maxVal * m_sliderScale);
    int sliderVal = static_cast<int>(initialVal * m_sliderScale);

    m_slider = new wxSlider(m_parent, wxID_ANY, sliderVal, sliderMin, sliderMax, wxDefaultPosition, wxSize(80, -1));
    m_slider->Bind(wxEVT_SCROLL_THUMBTRACK, &ControlGroup::OnSliderScroll, this);
    m_slider->Bind(wxEVT_SCROLL_CHANGED, &ControlGroup::OnSliderScroll, this);
    m_rowSizer->Add(m_slider, 1, wxEXPAND | wxRIGHT, 5);

    m_spin = new wxSpinCtrlDouble(m_parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, minVal, maxVal, initialVal, step);
    m_spin->SetDigits(precision);
    m_spin->Bind(wxEVT_SPINCTRLDOUBLE, &ControlGroup::OnSpinChange, this);
    m_rowSizer->Add(m_spin, 0, wxALIGN_CENTER_VERTICAL);

    m_parentSizer->Add(m_rowSizer, 0, wxEXPAND | wxALL, 2);
}

void ControlGroup::SetValue(float val) {
    m_spin->SetValue(val);
    m_slider->SetValue(static_cast<int>(val * m_sliderScale));
}

float ControlGroup::GetValue() const {
    return static_cast<float>(m_spin->GetValue());
}

void ControlGroup::Enable(bool enabled) {
    m_labelText->Enable(enabled);
    m_slider->Enable(enabled);
    m_spin->Enable(enabled);
}

void ControlGroup::Show(bool visible) {
    m_parentSizer->Show(m_rowSizer, visible);
    m_labelText->Show(visible);
    m_slider->Show(visible);
    m_spin->Show(visible);
    m_parent->Layout();
}

void ControlGroup::OnSliderScroll(wxScrollEvent& event) {
    float val = static_cast<float>(event.GetPosition()) / m_sliderScale;
    m_spin->SetValue(val);
    if (OnValueChange) OnValueChange(val);
}

void ControlGroup::OnSpinChange(wxSpinDoubleEvent& event) {
    float val = static_cast<float>(event.GetValue());
    m_slider->SetValue(static_cast<int>(val * m_sliderScale));
    if (OnValueChange) OnValueChange(val);
}
