// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Config/GeneralConfigPane.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/event.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/radiobox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <Core/Slippi/SlippiNetplay.h>

#include "Core/Analytics.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/PowerPC/PowerPC.h"
#include "DolphinWX/WxEventUtils.h"

GeneralConfigPane::GeneralConfigPane(wxWindow* parent, wxWindowID id) : wxPanel(parent, id)
{
	m_cpu_cores = {
			{PowerPC::CORE_INTERPRETER, _("Interpreter (slowest)")},
			{PowerPC::CORE_CACHEDINTERPRETER, _("Cached Interpreter (slower)")},
#ifdef _M_X86_64
			{PowerPC::CORE_JIT64, _("JIT Recompiler (recommended)")},
			{PowerPC::CORE_JITIL64, _("JITIL Recompiler (slow, experimental)")},
#elif defined(_M_ARM_64)
			{PowerPC::CORE_JITARM64, _("JIT Arm64 (experimental)")},
#endif
	};

	InitializeGUI();
	LoadGUIValues();
	BindEvents();
}

void GeneralConfigPane::InitializeGUI()
{
	m_throttler_array_string.Add(_("Unlimited"));
	for (int i = 10; i <= 200; i += 10)  // from 10% to 200%
	{
		if (i == 100)
			m_throttler_array_string.Add(wxString::Format(_("%i%% (Normal Speed)"), i));
		else
			m_throttler_array_string.Add(wxString::Format(_("%i%%"), i));
	}

	for (const CPUCore& cpu_core : m_cpu_cores)
		m_cpu_engine_array_string.Add(cpu_core.name);

	m_dual_core_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable Dual Core (speedup)"));
	m_cheats_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable Cheats"));
	m_boot_default_iso_checkbox = new wxCheckBox(this, wxID_ANY, _("Start Default ISO on Launch"));
	m_force_ntscj_checkbox = new wxCheckBox(this, wxID_ANY, _("Force Console as NTSC-J"));
#ifdef IS_PLAYBACK
	m_throttler_choice =
		new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_throttler_array_string);
#endif
	m_cpu_engine_radiobox =
		new wxRadioBox(this, wxID_ANY, _("CPU Emulator Engine"), wxDefaultPosition, wxDefaultSize,
			m_cpu_engine_array_string, 0, wxRA_SPECIFY_ROWS);

	m_dual_core_checkbox->SetToolTip(
		_("Splits the CPU and GPU threads so they can be run on separate cores.\nProvides major "
			"speed improvements on most modern PCs, but can cause occasional crashes/glitches."));
	m_cheats_checkbox->SetToolTip(_("Enables the use of Action Replay and Gecko cheats."));
	m_boot_default_iso_checkbox->SetToolTip(_("Boots the Default ISO when Dolphin launches. Right click a game in games list to set it as the default ISO."));
	m_force_ntscj_checkbox->SetToolTip(
		_("Forces NTSC-J mode for using the Japanese ROM font.\nIf left unchecked, Dolphin defaults "
			"to NTSC-U and automatically enables this setting when playing Japanese games."));
#ifdef IS_PLAYBACK
	m_throttler_choice->SetToolTip(_("Limits the emulation speed to the specified percentage.\nNote "
		"that raising or lowering the emulation speed will also raise "
		"or lower the audio pitch to prevent audio from stuttering."));
#endif
	const int space5 = FromDIP(5);

	wxBoxSizer* const throttler_sizer = new wxBoxSizer(wxHORIZONTAL);
	throttler_sizer->AddSpacer(space5);
#ifdef IS_PLAYBACK
	throttler_sizer->Add(new wxStaticText(this, wxID_ANY, _("Speed Limit:")), 0,
		wxALIGN_CENTER_VERTICAL | wxBOTTOM, space5);
	throttler_sizer->AddSpacer(space5);
	throttler_sizer->Add(m_throttler_choice, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, space5);
	throttler_sizer->AddSpacer(space5);
#endif
	wxStaticBoxSizer* const basic_settings_sizer =
		new wxStaticBoxSizer(wxVERTICAL, this, _("Basic Settings"));
	basic_settings_sizer->AddSpacer(space5);
	basic_settings_sizer->Add(m_dual_core_checkbox, 0, wxLEFT | wxRIGHT, space5);
	basic_settings_sizer->AddSpacer(space5);
	basic_settings_sizer->Add(m_cheats_checkbox, 0, wxLEFT | wxRIGHT, space5);
	basic_settings_sizer->AddSpacer(space5);
	basic_settings_sizer->Add(m_boot_default_iso_checkbox, 0, wxLEFT | wxRIGHT, space5);
	basic_settings_sizer->AddSpacer(space5);
	basic_settings_sizer->Add(throttler_sizer);

	wxStaticBoxSizer* const advanced_settings_sizer =
		new wxStaticBoxSizer(wxVERTICAL, this, _("Advanced Settings"));
	advanced_settings_sizer->AddSpacer(space5);
	advanced_settings_sizer->Add(m_cpu_engine_radiobox, 0, wxLEFT | wxRIGHT, space5);
	advanced_settings_sizer->AddSpacer(space5);
	advanced_settings_sizer->Add(m_force_ntscj_checkbox, 0, wxLEFT | wxRIGHT, space5);
	advanced_settings_sizer->AddSpacer(space5);

	wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);
	main_sizer->AddSpacer(space5);
	main_sizer->Add(basic_settings_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	main_sizer->AddSpacer(space5);
	main_sizer->Add(advanced_settings_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	main_sizer->AddSpacer(space5);

	SetSizer(main_sizer);
}

void GeneralConfigPane::LoadGUIValues()
{
	const SConfig& startup_params = SConfig::GetInstance();

	m_dual_core_checkbox->SetValue(startup_params.bCPUThread);
	m_cheats_checkbox->SetValue(startup_params.bEnableCheats);
	m_boot_default_iso_checkbox->SetValue(startup_params.bBootDefaultISO);
	m_force_ntscj_checkbox->SetValue(startup_params.bForceNTSCJ);
#ifdef IS_PLAYBACK
	u32 selection = std::lround(startup_params.m_EmulationSpeed * 10.0f);
	if (selection < m_throttler_array_string.size())
		m_throttler_choice->SetSelection(selection);
#endif
	for (size_t i = 0; i < m_cpu_cores.size(); ++i)
	{
		if (m_cpu_cores[i].CPUid == startup_params.iCPUCore)
			m_cpu_engine_radiobox->SetSelection(i);
	}
}

void GeneralConfigPane::BindEvents()
{
	m_dual_core_checkbox->Bind(wxEVT_CHECKBOX, &GeneralConfigPane::OnDualCoreCheckBoxChanged, this);
	m_dual_core_checkbox->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

	m_cheats_checkbox->Bind(wxEVT_CHECKBOX, &GeneralConfigPane::OnCheatCheckBoxChanged, this);
	m_cheats_checkbox->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);

	m_boot_default_iso_checkbox->Bind(wxEVT_CHECKBOX, &GeneralConfigPane::OnBootDefaultCheckBoxChanged, this);

	m_force_ntscj_checkbox->Bind(wxEVT_CHECKBOX, &GeneralConfigPane::OnForceNTSCJCheckBoxChanged,
		this);
	m_force_ntscj_checkbox->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);
#ifdef IS_PLAYBACK
	m_throttler_choice->Bind(wxEVT_CHOICE, &GeneralConfigPane::OnThrottlerChoiceChanged, this);
#endif
	m_cpu_engine_radiobox->Bind(wxEVT_RADIOBOX, &GeneralConfigPane::OnCPUEngineRadioBoxChanged, this);
	m_cpu_engine_radiobox->Bind(wxEVT_UPDATE_UI, &WxEventUtils::OnEnableIfCoreNotRunning);
}

void GeneralConfigPane::OnDualCoreCheckBoxChanged(wxCommandEvent& event)
{
	if (Core::IsRunning())
		return;

	SConfig::GetInstance().bCPUThread = m_dual_core_checkbox->IsChecked();
}

void GeneralConfigPane::OnCheatCheckBoxChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().bEnableCheats = m_cheats_checkbox->IsChecked();
}

void GeneralConfigPane::OnBootDefaultCheckBoxChanged(wxCommandEvent &event)
{
	SConfig::GetInstance().bBootDefaultISO = m_boot_default_iso_checkbox->IsChecked();
}

void GeneralConfigPane::OnForceNTSCJCheckBoxChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().bForceNTSCJ = m_force_ntscj_checkbox->IsChecked();
}

void GeneralConfigPane::OnThrottlerChoiceChanged(wxCommandEvent& event)
{
    if(IsOnline()) return;
	if (m_throttler_choice->GetSelection() != wxNOT_FOUND)
		SConfig::GetInstance().m_EmulationSpeed = m_throttler_choice->GetSelection() * 0.1f;
}

void GeneralConfigPane::OnCPUEngineRadioBoxChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().iCPUCore = m_cpu_cores.at(event.GetSelection()).CPUid;
}
