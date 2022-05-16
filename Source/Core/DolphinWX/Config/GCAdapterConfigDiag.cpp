// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Config/GCAdapterConfigDiag.h"

#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "Common/CommonTypes.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "InputCommon/GCAdapter.h"

wxDEFINE_EVENT(wxEVT_ADAPTER_UPDATE, wxCommandEvent);

GCAdapterConfigDiag::GCAdapterConfigDiag(wxWindow* const parent, const wxString& name,
	const int tab_num)
	: wxDialog(parent, wxID_ANY, name), m_pad_id(tab_num)
{
	GCAdapter::ResetAdapterIfNecessary();
    wxArrayString remap_array_string;

    remap_array_string.Add(_("A"));
    remap_array_string.Add(_("B"));
    remap_array_string.Add(_("X"));
    remap_array_string.Add(_("Y"));
    remap_array_string.Add(_("Z"));
    remap_array_string.Add(_("L"));
    remap_array_string.Add(_("R"));
    remap_array_string.Add(_("D-pad up"));
    remap_array_string.Add(_("D-pad right"));
    remap_array_string.Add(_("D-pad down"));
    remap_array_string.Add(_("D-pad left"));

	wxCheckBox* const gamecube_rumble = new wxCheckBox(this, wxID_ANY, _("Rumble"));
    wxChoice* const a_choice = new wxChoice(this, wxID_ANY, wxDefaultSize, remap_array_string); 
    wxChoice* const b_choice = new wxChoice(this, wxID_ANY, wxDefaultSize, remap_array_string); 
    wxChoice* const x_choice = new wxChoice(this, wxID_ANY, wxDefaultSize, remap_array_string); 
    wxChoice* const y_choice = new wxChoice(this, wxID_ANY, wxDefaultSize, remap_array_string); 
    wxChoice* const z_choice = new wxChoice(this, wxID_ANY, wxDefaultSize, remap_array_string); 
    wxChoice* const l_choice = new wxChoice(this, wxID_ANY, wxDefaultSize, remap_array_string); 
    wxChoice* const r_choice = new wxChoice(this, wxID_ANY, wxDefaultSize, remap_array_string); 
    wxChoice* const up_choice = new wxChoice(this, wxID_ANY, wxDefaultSize, remap_array_string); 
    wxChoice* const right_choice = new wxChoice(this, wxID_ANY, wxDefaultSize, remap_array_string); 
    wxChoice* const down_choice = new wxChoice(this, wxID_ANY, wxDefaultSize, remap_array_string); 
    wxChoice* const left_choice = new wxChoice(this, wxID_ANY, wxDefaultSize, remap_array_string); 
	gamecube_rumble->SetValue(SConfig::GetInstance().m_AdapterRumble[m_pad_id]);
	gamecube_rumble->Bind(wxEVT_CHECKBOX, &GCAdapterConfigDiag::OnAdapterRumble, this);

	m_adapter_status = new wxStaticText(this, wxID_ANY, _("Adapter Not Detected"));

	if (!GCAdapter::IsDetected())
	{
		if (!GCAdapter::IsDriverDetected())
		{
			m_adapter_status->SetLabelText(_("Driver Not Detected"));
			gamecube_rumble->Disable();
		}
	}
	else
	{
		m_adapter_status->SetLabelText(wxString::Format("%s (poll rate: %.1f hz)", _("Adapter Detected"), 1000.0 / GCAdapter::ReadRate()));
	}
	GCAdapter::SetAdapterCallback(std::bind(&GCAdapterConfigDiag::ScheduleAdapterUpdate, this));

	const int space5 = FromDIP(5);

	wxBoxSizer* const szr = new wxBoxSizer(wxVERTICAL);
	szr->Add(m_adapter_status, 0, wxEXPAND);
	szr->AddSpacer(space5);
	szr->Add(gamecube_rumble, 0, wxEXPAND);
	szr->AddSpacer(space5);
	wxBoxSizer *const a_box = new wxBoxSizer(wxHORIZONTAL);
	a_box->Add(new wxStaticText(this, wxID_ANY, _("A: ")));
	a_box->AddSpacer(space5);
	a_box->Add(a_choice, 0, wxEXPAND);
	szr->Add(a_box);
	szr->AddSpacer(space5);
	szr->Add(CreateButtonSizer(wxCLOSE | wxNO_DEFAULT), 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	szr->AddSpacer(space5);

	wxBoxSizer* const padding_szr = new wxBoxSizer(wxVERTICAL);
	padding_szr->Add(szr, 0, wxALL, 12);

	SetSizerAndFit(padding_szr);
	Center();

	Bind(wxEVT_ADAPTER_UPDATE, &GCAdapterConfigDiag::OnUpdateAdapter, this);
	
	m_update_rate_timer.SetOwner(this);
	Bind(wxEVT_TIMER, &GCAdapterConfigDiag::OnUpdateRate, this, m_update_rate_timer.GetId());
	m_update_rate_timer.Start(1000, wxTIMER_CONTINUOUS);
}

GCAdapterConfigDiag::~GCAdapterConfigDiag()
{
	GCAdapter::SetAdapterCallback(nullptr);
}

void GCAdapterConfigDiag::ScheduleAdapterUpdate()
{
	wxQueueEvent(this, new wxCommandEvent(wxEVT_ADAPTER_UPDATE));
}

void GCAdapterConfigDiag::OnUpdateAdapter(wxCommandEvent& WXUNUSED(event))
{
	bool unpause = Core::PauseAndLock(true);
	if (GCAdapter::IsDetected())
		m_adapter_status->SetLabelText(wxString::Format("%s (poll rate: %.1f hz)", _("Adapter Detected"), 1000.0 / GCAdapter::ReadRate()));
	else
		m_adapter_status->SetLabelText(_("Adapter Not Detected"));
	Core::PauseAndLock(false, unpause);
}

void GCAdapterConfigDiag::OnAdapterRumble(wxCommandEvent& event)
{
	SConfig::GetInstance().m_AdapterRumble[m_pad_id] = event.IsChecked();
}

void GCAdapterConfigDiag::OnZXSwap(wxCommandEvent &event)
{
	SConfig::GetInstance().m_AdapterZXSwap[m_pad_id] = event.IsChecked();
}

void GCAdapterConfigDiag::OnUpdateRate(wxTimerEvent& ev) 
{
	if (GCAdapter::IsDetected())
		m_adapter_status->SetLabelText(wxString::Format("%s (poll rate: %.1f hz)", _("Adapter Detected"), 1000.0 / GCAdapter::ReadRate()));
}
