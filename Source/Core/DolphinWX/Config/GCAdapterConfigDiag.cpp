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
  remap_array_string.Add(_("None"));

wxCheckBox* const gamecube_rumble = new wxCheckBox(this, wxID_ANY, _("Rumble"));
  wxChoice* const a_choice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, remap_array_string);
  wxChoice* const b_choice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, remap_array_string);
  wxChoice* const x_choice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, remap_array_string);
	wxChoice *const y_choice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, remap_array_string);
  wxChoice* const z_choice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, remap_array_string);
  wxChoice* const l_choice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, remap_array_string);
  wxChoice* const r_choice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, remap_array_string);
  wxChoice* const up_choice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, remap_array_string);
  wxChoice* const right_choice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, remap_array_string);
  wxChoice* const down_choice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, remap_array_string);
  wxChoice* const left_choice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, remap_array_string);
	gamecube_rumble->SetValue(SConfig::GetInstance().m_AdapterRumble[m_pad_id]);
	gamecube_rumble->Bind(wxEVT_CHECKBOX, &GCAdapterConfigDiag::OnAdapterRumble, this);

	a_choice->SetSelection(PadButtonToSelection(SConfig::GetInstance().m_AChoice[m_pad_id]));
	b_choice->SetSelection(PadButtonToSelection(SConfig::GetInstance().m_BChoice[m_pad_id]));
	x_choice->SetSelection(PadButtonToSelection(SConfig::GetInstance().m_XChoice[m_pad_id]));
	y_choice->SetSelection(PadButtonToSelection(SConfig::GetInstance().m_YChoice[m_pad_id]));
	z_choice->SetSelection(PadButtonToSelection(SConfig::GetInstance().m_ZChoice[m_pad_id]));
	l_choice->SetSelection(PadButtonToSelection(SConfig::GetInstance().m_LChoice[m_pad_id]));
	r_choice->SetSelection(PadButtonToSelection(SConfig::GetInstance().m_RChoice[m_pad_id]));
	up_choice->SetSelection(PadButtonToSelection(SConfig::GetInstance().m_UpChoice[m_pad_id]));
	right_choice->SetSelection(PadButtonToSelection(SConfig::GetInstance().m_RightChoice[m_pad_id]));
	down_choice->SetSelection(PadButtonToSelection(SConfig::GetInstance().m_DownChoice[m_pad_id]));
	left_choice->SetSelection(PadButtonToSelection(SConfig::GetInstance().m_LeftChoice[m_pad_id]));

	a_choice->Bind(wxEVT_CHOICE, &GCAdapterConfigDiag::OnAChoice, this);
	b_choice->Bind(wxEVT_CHOICE, &GCAdapterConfigDiag::OnBChoice, this);
	x_choice->Bind(wxEVT_CHOICE, &GCAdapterConfigDiag::OnXChoice, this);
	y_choice->Bind(wxEVT_CHOICE, &GCAdapterConfigDiag::OnYChoice, this);
	z_choice->Bind(wxEVT_CHOICE, &GCAdapterConfigDiag::OnZChoice, this);
	l_choice->Bind(wxEVT_CHOICE, &GCAdapterConfigDiag::OnLChoice, this);
	r_choice->Bind(wxEVT_CHOICE, &GCAdapterConfigDiag::OnRChoice, this);
	up_choice->Bind(wxEVT_CHOICE, &GCAdapterConfigDiag::OnUpChoice, this);
	right_choice->Bind(wxEVT_CHOICE, &GCAdapterConfigDiag::OnRightChoice, this);
	down_choice->Bind(wxEVT_CHOICE, &GCAdapterConfigDiag::OnDownChoice, this);
	left_choice->Bind(wxEVT_CHOICE, &GCAdapterConfigDiag::OnLeftChoice, this);

	m_adapter_status = new wxStaticText(this, wxID_ANY, _("Adapter Not Detected"));

	if (!GCAdapter::IsDetected())
	{
		if (!GCAdapter::IsDriverDetected())
		{
			m_adapter_status->SetLabelText(_("Driver Not Detected"));
			gamecube_rumble->Disable();
			a_choice->Disable();
			b_choice->Disable();
			x_choice->Disable();
			y_choice->Disable();
			z_choice->Disable();
			l_choice->Disable();
			r_choice->Disable();
			up_choice->Disable();
			right_choice->Disable();
			down_choice->Disable();
			left_choice->Disable();
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
	szr->Add(new wxStaticText(this, wxID_ANY, _("Button remapping: ")));
	szr->AddSpacer(space5);

	wxBoxSizer *const a_box = new wxBoxSizer(wxHORIZONTAL);
	a_box->Add(new wxStaticText(this, wxID_ANY, _("A: ")));
	a_box->AddSpacer(space5);
	a_box->Add(a_choice, 0, wxEXPAND);
	szr->Add(a_box);

	wxBoxSizer *const b_box = new wxBoxSizer(wxHORIZONTAL);
	b_box->Add(new wxStaticText(this, wxID_ANY, _("B: ")));
	b_box->AddSpacer(space5);
	b_box->Add(b_choice, 0, wxEXPAND);
	szr->Add(b_box);

	wxBoxSizer *const x_box = new wxBoxSizer(wxHORIZONTAL);
	x_box->Add(new wxStaticText(this, wxID_ANY, _("X: ")));
	x_box->AddSpacer(space5);
	x_box->Add(x_choice, 0, wxEXPAND);
	szr->Add(x_box);

	wxBoxSizer *const y_box = new wxBoxSizer(wxHORIZONTAL);
	y_box->Add(new wxStaticText(this, wxID_ANY, _("Y: ")));
	y_box->AddSpacer(space5);
	y_box->Add(y_choice, 0, wxEXPAND);
	szr->Add(y_box);

	wxBoxSizer *const z_box = new wxBoxSizer(wxHORIZONTAL);
	z_box->Add(new wxStaticText(this, wxID_ANY, _("Z: ")));
	z_box->AddSpacer(space5);
	z_box->Add(z_choice, 0, wxEXPAND);
	szr->Add(z_box);

	wxBoxSizer *const l_box = new wxBoxSizer(wxHORIZONTAL);
	l_box->Add(new wxStaticText(this, wxID_ANY, _("L: ")));
	l_box->AddSpacer(space5);
	l_box->Add(l_choice, 0, wxEXPAND);
	szr->Add(l_box);

	wxBoxSizer *const r_box = new wxBoxSizer(wxHORIZONTAL);
	r_box->Add(new wxStaticText(this, wxID_ANY, _("R: ")));
	r_box->AddSpacer(space5);
	r_box->Add(r_choice, 0, wxEXPAND);
	szr->Add(r_box);

	wxBoxSizer *const up_box = new wxBoxSizer(wxHORIZONTAL);
	up_box->Add(new wxStaticText(this, wxID_ANY, _("D-pad up: ")));
	up_box->AddSpacer(space5);
	up_box->Add(up_choice, 0, wxEXPAND);
	szr->Add(up_box);

	wxBoxSizer *const right_box = new wxBoxSizer(wxHORIZONTAL);
	right_box->Add(new wxStaticText(this, wxID_ANY, _("D-pad right: ")));
	right_box->AddSpacer(space5);
	right_box->Add(right_choice, 0, wxEXPAND);
	szr->Add(right_box);

	wxBoxSizer *const down_box = new wxBoxSizer(wxHORIZONTAL);
	down_box->Add(new wxStaticText(this, wxID_ANY, _("D-pad down: ")));
	down_box->AddSpacer(space5);
	down_box->Add(down_choice, 0, wxEXPAND);
	szr->Add(down_box);

	wxBoxSizer *const left_box = new wxBoxSizer(wxHORIZONTAL);
	left_box->Add(new wxStaticText(this, wxID_ANY, _("D-pad left: ")));
	left_box->AddSpacer(space5);
	left_box->Add(left_choice, 0, wxEXPAND);
	szr->Add(left_box);

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


int GCAdapterConfigDiag::PadButtonToSelection(PadButton button) {
	switch (button)
	{
	case PAD_BUTTON_A:
		return 0;
	case PAD_BUTTON_B:
		return 1;
	case PAD_BUTTON_X:
		return 2;
	case PAD_BUTTON_Y:
		return 3;
	case PAD_TRIGGER_Z:
		return 4;
	case PAD_TRIGGER_L:
		return 5;
	case PAD_TRIGGER_R:
		return 6;
	case PAD_BUTTON_UP:
		return 7;
	case PAD_BUTTON_RIGHT:
		return 8;
	case PAD_BUTTON_DOWN:
		return 9;
	case PAD_BUTTON_LEFT:
		return 10;
	case PAD_BUTTON_NONE:
		return 11;
	};
	return -1;
}

void GCAdapterConfigDiag::OnAChoice(wxCommandEvent &event)
{
	SConfig::GetInstance().m_AChoice[m_pad_id] = SelectionToPadButton[event.GetSelection()];
}

void GCAdapterConfigDiag::OnBChoice(wxCommandEvent &event)
{
	SConfig::GetInstance().m_BChoice[m_pad_id] = SelectionToPadButton[event.GetSelection()];
}

void GCAdapterConfigDiag::OnXChoice(wxCommandEvent &event)
{
	SConfig::GetInstance().m_XChoice[m_pad_id] = SelectionToPadButton[event.GetSelection()];
}

void GCAdapterConfigDiag::OnYChoice(wxCommandEvent &event)
{
	SConfig::GetInstance().m_YChoice[m_pad_id] = SelectionToPadButton[event.GetSelection()];
}

void GCAdapterConfigDiag::OnZChoice(wxCommandEvent &event)
{
	SConfig::GetInstance().m_ZChoice[m_pad_id] = SelectionToPadButton[event.GetSelection()];
}

void GCAdapterConfigDiag::OnLChoice(wxCommandEvent &event)
{
	SConfig::GetInstance().m_LChoice[m_pad_id] = SelectionToPadButton[event.GetSelection()];
}

void GCAdapterConfigDiag::OnRChoice(wxCommandEvent &event)
{
	SConfig::GetInstance().m_RChoice[m_pad_id] = SelectionToPadButton[event.GetSelection()];
}

void GCAdapterConfigDiag::OnUpChoice(wxCommandEvent &event)
{
	SConfig::GetInstance().m_UpChoice[m_pad_id] = SelectionToPadButton[event.GetSelection()];
}

void GCAdapterConfigDiag::OnRightChoice(wxCommandEvent &event)
{
	SConfig::GetInstance().m_RightChoice[m_pad_id] = SelectionToPadButton[event.GetSelection()];
}

void GCAdapterConfigDiag::OnDownChoice(wxCommandEvent &event)
{
	SConfig::GetInstance().m_DownChoice[m_pad_id] = SelectionToPadButton[event.GetSelection()];
}

void GCAdapterConfigDiag::OnLeftChoice(wxCommandEvent &event)
{
	SConfig::GetInstance().m_LeftChoice[m_pad_id] = SelectionToPadButton[event.GetSelection()];
}



void GCAdapterConfigDiag::OnUpdateRate(wxTimerEvent& ev) 
{
	if (GCAdapter::IsDetected())
		m_adapter_status->SetLabelText(wxString::Format("%s (poll rate: %.1f hz)", _("Adapter Detected"), 1000.0 / GCAdapter::ReadRate()));
}
