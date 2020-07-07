// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Config/AdvancedConfigPane.h"

#include <cmath>

#include <wx/checkbox.h>
#include <wx/datectrl.h>
#include <wx/dateevt.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/time.h>
#include <wx/timectrl.h>

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "DolphinWX/DolphinSlider.h"
#include "DolphinWX/WxEventUtils.h"

AdvancedConfigPane::AdvancedConfigPane(wxWindow* parent, wxWindowID id) : wxPanel(parent, id)
{
	InitializeGUI();
	LoadGUIValues();
	BindEvents();
}

void AdvancedConfigPane::InitializeGUI()
{
	m_clock_override_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable CPU Clock Override"));
	m_clock_override_slider =
		new DolphinSlider(this, wxID_ANY, 100, 0, 150, wxDefaultPosition, FromDIP(wxSize(200, -1)));
	m_clock_override_text = new wxStaticText(this, wxID_ANY, "");

	m_qos_enabled = new wxCheckBox(this, wxID_ANY, _("Enable QoS (Quality of Service) bit on packets"));
	m_adapter_warning = new wxCheckBox(this, wxID_ANY, _("Neutralize inputs when adapter problems are detected"));
	
	m_custom_rtc_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable Custom RTC"));
	m_custom_rtc_date_picker = new wxDatePickerCtrl(this, wxID_ANY);
	m_custom_rtc_time_picker = new wxTimePickerCtrl(this, wxID_ANY);

	wxStaticText* const clock_override_description =
		new wxStaticText(this, wxID_ANY, _("Higher values can make variable-framerate games "
			"run at a higher framerate, at the expense of CPU. "
			"Lower values can make variable-framerate games "
			"run at a lower framerate, saving CPU.\n\n"
			"WARNING: Changing this from the default (100%) "
			"can and will break games and cause glitches. "
			"Do so at your own risk. Please do not report "
			"bugs that occur with a non-default clock. "));

	/* wxStaticText* const qos_description =
		new wxStaticText(this, wxID_ANY, _("This setting makes Dolphin tag outgoing packets with a QoS bit.\n\n"
			"This should make your router prioritize NetPlay packets over normal packets, "
			"which means you can download and use your Internet connection for other things "
			"while playing without getting extra packet drops/input lag."
			"\n\n"
			"Try turning this setting off if you experience problems with NetPlay."));	

	wxStaticText* const adapter_warning_description =
		new wxStaticText(this, wxID_ANY, _("This setting makes Dolphin warn and neutralize (centered sticks and no buttons pressed) inputs when an adapter problem is detected.\n\n"
			"This should only occur when your adapter returns something other than LIBUSB_SUCCESS.\n"
			"Before turning this off, try reinstalling drivers and switching USB ports."
			"\n\n"
			"Try turning this setting off if a false positive error is being detected (though there's a high chance that an actual problem is happening).")); */

	wxStaticText* const custom_rtc_description = new wxStaticText(
		this, wxID_ANY,
		_("This setting allows you to set a custom real time clock (RTC) separate "
			"from your current system time.\n\nIf you're unsure, leave this disabled."));

#ifdef __APPLE__
	clock_override_description->Wrap(550);
	// qos_description->Wrap(550);
	// adapter_warning_description->Wrap(550);
	custom_rtc_description->Wrap(550);
#else
	clock_override_description->Wrap(FromDIP(400));
	// qos_description->Wrap(FromDIP(400));	
	// adapter_warning_description->Wrap(FromDIP(400));
	custom_rtc_description->Wrap(FromDIP(400));
#endif

	const int space5 = FromDIP(5);

	wxBoxSizer* const clock_override_slider_sizer = new wxBoxSizer(wxHORIZONTAL);
	clock_override_slider_sizer->Add(m_clock_override_slider, 1);
	clock_override_slider_sizer->Add(m_clock_override_text, 1, wxLEFT, space5);

	wxStaticBoxSizer* const cpu_options_sizer =
		new wxStaticBoxSizer(wxVERTICAL, this, _("CPU Options"));
	cpu_options_sizer->AddSpacer(space5);
	cpu_options_sizer->Add(m_clock_override_checkbox, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	cpu_options_sizer->AddSpacer(space5);
	cpu_options_sizer->Add(clock_override_slider_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	cpu_options_sizer->AddSpacer(space5);
	cpu_options_sizer->Add(clock_override_description, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	cpu_options_sizer->AddSpacer(space5);

	wxStaticBoxSizer* const troubleshooting_sizer =
		new wxStaticBoxSizer(wxVERTICAL, this, _("Troubleshooting"));
	troubleshooting_sizer->AddSpacer(space5);
	troubleshooting_sizer->Add(m_qos_enabled, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	troubleshooting_sizer->AddSpacer(space5);
	troubleshooting_sizer->Add(m_adapter_warning, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	troubleshooting_sizer->AddSpacer(space5);

	wxFlexGridSizer* const custom_rtc_date_time_sizer =
		new wxFlexGridSizer(2, wxSize(space5, space5));
	custom_rtc_date_time_sizer->Add(m_custom_rtc_date_picker, 0, wxEXPAND);
	custom_rtc_date_time_sizer->Add(m_custom_rtc_time_picker, 0, wxEXPAND);

	wxStaticBoxSizer* const custom_rtc_sizer =
		new wxStaticBoxSizer(wxVERTICAL, this, _("Custom RTC Options"));
	custom_rtc_sizer->AddSpacer(space5);
	custom_rtc_sizer->Add(m_custom_rtc_checkbox, 0, wxLEFT | wxRIGHT, space5);
	custom_rtc_sizer->AddSpacer(space5);
	custom_rtc_sizer->Add(custom_rtc_date_time_sizer, 0, wxLEFT | wxRIGHT, space5);
	custom_rtc_sizer->AddSpacer(space5);
	custom_rtc_sizer->Add(custom_rtc_description, 0, wxLEFT | wxRIGHT, space5);
	custom_rtc_sizer->AddSpacer(space5);

	wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);
	main_sizer->AddSpacer(space5);
	main_sizer->Add(cpu_options_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	main_sizer->AddSpacer(space5);
	main_sizer->Add(troubleshooting_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	main_sizer->AddSpacer(space5);
	main_sizer->Add(custom_rtc_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	main_sizer->AddSpacer(space5);

	SetSizer(main_sizer);
}

void AdvancedConfigPane::LoadGUIValues()
{
	int ocFactor = (int)(std::log2f(SConfig::GetInstance().m_OCFactor) * 25.f + 100.f + 0.5f);
	bool oc_enabled = SConfig::GetInstance().m_OCEnable;
	m_clock_override_checkbox->SetValue(oc_enabled);
	m_clock_override_slider->SetValue(ocFactor);
	m_clock_override_slider->Enable(oc_enabled);
	UpdateCPUClock();
	LoadCustomRTC();

	m_qos_enabled->SetValue(SConfig::GetInstance().bQoSEnabled);
	m_adapter_warning->SetValue(SConfig::GetInstance().bAdapterWarning);
}

void AdvancedConfigPane::BindEvents()
{
	m_clock_override_checkbox->Bind(wxEVT_CHECKBOX,
		&AdvancedConfigPane::OnClockOverrideCheckBoxChanged, this);
	m_clock_override_checkbox->Bind(wxEVT_UPDATE_UI, &AdvancedConfigPane::OnUpdateCPUClockControls,
		this);

	m_clock_override_slider->Bind(wxEVT_SLIDER, &AdvancedConfigPane::OnClockOverrideSliderChanged,
		this);
	m_clock_override_slider->Bind(wxEVT_UPDATE_UI, &AdvancedConfigPane::OnUpdateCPUClockControls,
		this);

	m_qos_enabled->Bind(wxEVT_CHECKBOX,
		&AdvancedConfigPane::OnQoSCheckBoxChanged, this);	
	m_adapter_warning->Bind(wxEVT_CHECKBOX,
		&AdvancedConfigPane::OnAdapterWarningCheckBoxChanged, this);	

	m_custom_rtc_checkbox->Bind(wxEVT_CHECKBOX, &AdvancedConfigPane::OnCustomRTCCheckBoxChanged,
		this);
	m_custom_rtc_checkbox->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

	m_custom_rtc_date_picker->Bind(wxEVT_DATE_CHANGED, &AdvancedConfigPane::OnCustomRTCDateChanged,
		this);
	m_custom_rtc_date_picker->Bind(wxEVT_UPDATE_UI, &AdvancedConfigPane::OnUpdateRTCDateTimeEntries,
		this);

	m_custom_rtc_time_picker->Bind(wxEVT_TIME_CHANGED, &AdvancedConfigPane::OnCustomRTCTimeChanged,
		this);
	m_custom_rtc_time_picker->Bind(wxEVT_UPDATE_UI, &AdvancedConfigPane::OnUpdateRTCDateTimeEntries,
		this);
}

void AdvancedConfigPane::OnClockOverrideCheckBoxChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().m_OCEnable = m_clock_override_checkbox->IsChecked();
	m_clock_override_slider->Enable(SConfig::GetInstance().m_OCEnable);
	UpdateCPUClock();
}

void AdvancedConfigPane::OnClockOverrideSliderChanged(wxCommandEvent& event)
{
	// Vaguely exponential scaling?
	SConfig::GetInstance().m_OCFactor =
		std::exp2f((m_clock_override_slider->GetValue() - 100.f) / 25.f);
	UpdateCPUClock();
}

void AdvancedConfigPane::OnQoSCheckBoxChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().bQoSEnabled = m_qos_enabled->IsChecked();
}

void AdvancedConfigPane::OnAdapterWarningCheckBoxChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().bAdapterWarning = m_adapter_warning->IsChecked();
}

static u32 ToSeconds(wxDateTime date)
{
	return static_cast<u32>(date.GetValue().GetValue() / 1000);
}

void AdvancedConfigPane::OnCustomRTCCheckBoxChanged(wxCommandEvent& event)
{
	const bool checked = m_custom_rtc_checkbox->IsChecked();
	SConfig::GetInstance().bEnableCustomRTC = checked;
	m_custom_rtc_date_picker->Enable(checked);
	m_custom_rtc_time_picker->Enable(checked);
}

void AdvancedConfigPane::OnCustomRTCDateChanged(wxCommandEvent& event)
{
	m_temp_date = ToSeconds(m_custom_rtc_date_picker->GetValue());
	UpdateCustomRTC(m_temp_date, m_temp_time);
}

void AdvancedConfigPane::OnCustomRTCTimeChanged(wxCommandEvent& event)
{
	m_temp_time = ToSeconds(m_custom_rtc_time_picker->GetValue()) - m_temp_date;
	UpdateCustomRTC(m_temp_date, m_temp_time);
}

void AdvancedConfigPane::UpdateCPUClock()
{
	bool wii = SConfig::GetInstance().bWii;
	int percent = (int)(std::roundf(SConfig::GetInstance().m_OCFactor * 100.f));
	int clock = (int)(std::roundf(SConfig::GetInstance().m_OCFactor * (wii ? 729.f : 486.f)));

	m_clock_override_text->SetLabel(
		SConfig::GetInstance().m_OCEnable ? wxString::Format("%d %% (%d mhz)", percent, clock) : "");
}

void AdvancedConfigPane::LoadCustomRTC()
{
	wxDateTime custom_rtc(static_cast<time_t>(SConfig::GetInstance().m_customRTCValue));
	custom_rtc = custom_rtc.ToUTC();
	bool custom_rtc_enabled = SConfig::GetInstance().bEnableCustomRTC;
	m_custom_rtc_checkbox->SetValue(custom_rtc_enabled);
	if (custom_rtc.IsValid())
	{
		m_custom_rtc_date_picker->SetValue(custom_rtc);
		m_custom_rtc_time_picker->SetValue(custom_rtc);
	}
	m_temp_date = ToSeconds(m_custom_rtc_date_picker->GetValue());
	m_temp_time = ToSeconds(m_custom_rtc_time_picker->GetValue()) - m_temp_date;
	// Limit dates to a valid range (Jan 1/2000 to Dec 31/2099)
	m_custom_rtc_date_picker->SetRange(wxDateTime(1, wxDateTime::Jan, 2000),
		wxDateTime(31, wxDateTime::Dec, 2099));
}

void AdvancedConfigPane::UpdateCustomRTC(time_t date, time_t time)
{
	wxDateTime custom_rtc(date + time);
	SConfig::GetInstance().m_customRTCValue = ToSeconds(custom_rtc.FromUTC());
	m_custom_rtc_date_picker->SetValue(custom_rtc);
	m_custom_rtc_time_picker->SetValue(custom_rtc);
}

void AdvancedConfigPane::OnUpdateCPUClockControls(wxUpdateUIEvent& event)
{
	if (!Core::IsRunning())
	{
		event.Enable(true);
		return;
	}

	event.Enable(!Core::g_want_determinism);
}

void AdvancedConfigPane::OnUpdateRTCDateTimeEntries(wxUpdateUIEvent& event)
{
	event.Enable(!Core::IsRunning() && m_custom_rtc_checkbox->IsChecked());
}
