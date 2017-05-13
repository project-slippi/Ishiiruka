// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
#pragma once

#include <vector>
#include <string>
#include <map>

#include "Core/ConfigManager.h"
#include "VideoCommon/VideoConfig.h"
#include "Core/Core.h"

#include <wx/wx.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/combobox.h>
#include <wx/checkbox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/spinctrl.h>

#include "Common/MsgHandler.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/PostProcessingConfigDiag.h"
template <typename W>
class BoolSetting : public W
{
public:
	BoolSetting(wxWindow* parent, const wxString& label, const wxString& tooltip, bool &setting, bool reverse = false, long style = 0);

	void UpdateValue(wxCommandEvent& ev)
	{
		m_setting = (ev.GetInt() != 0) ^ m_reverse;
		ev.Skip();
	}
private:
	bool &m_setting;
	const bool m_reverse;
};

typedef BoolSetting<wxCheckBox> SettingCheckBox;
typedef BoolSetting<wxRadioButton> SettingRadioButton;

template <typename T>
class IntegerSetting : public wxSpinCtrl
{
public:
	IntegerSetting(wxWindow* parent, const wxString& label, T& setting, int minVal, int maxVal, long style = 0);

	void UpdateValue(wxCommandEvent& ev)
	{
		m_setting = ev.GetInt();
		ev.Skip();
	}
private:
	T& m_setting;
};

typedef IntegerSetting<u32> U32Setting;

class SettingChoice : public wxChoice
{
public:
	SettingChoice(wxWindow* parent, int &setting, const wxString& tooltip, int num = 0, const wxString choices[] = NULL, long style = 0);
	void UpdateValue(wxCommandEvent& ev);
private:
	int &m_setting;
};

class VideoConfigDiag : public wxDialog
{
public:
	VideoConfigDiag(wxWindow* parent, const std::string &title);

protected:
	void Event_Backend(wxCommandEvent &ev);

	void Event_Adapter(wxCommandEvent &ev);

	void Event_DisplayResolution(wxCommandEvent &ev);

	void Event_ProgressiveScan(wxCommandEvent &ev);

	void Event_Stc(wxCommandEvent &ev);
	void Event_Bbox(wxCommandEvent &ev);

	// Post-processing shader list manipulation
	void Event_PPShaderList(wxCommandEvent& ev);
	void Event_PPShaderListMoveUp(wxCommandEvent& ev);
	void Event_PPShaderListMoveDown(wxCommandEvent& ev);
	void Event_PPShaderListOptions(wxCommandEvent& ev);
	void Event_PPShaderListRemove(wxCommandEvent& ev);
	void Event_PPShaderAdd(wxCommandEvent& ev);
	void Event_ScalingShader(wxCommandEvent& ev);
	void Event_ConfigureScalingShader(wxCommandEvent &ev);
	void Event_StereoShader(wxCommandEvent& ev);

	void Event_StereoDepth(wxCommandEvent &ev);
	void Event_TessellationDistance(wxCommandEvent &ev);
	void Event_TessellationMax(wxCommandEvent &ev);
	void Event_TessellationRounding(wxCommandEvent &ev);
	void Event_TessellationDisplacement(wxCommandEvent &ev);
	void Event_StereoConvergence(wxCommandEvent &ev);
	void Event_StereoMode(wxCommandEvent &ev);
	void Event_ScalingFactor(wxCommandEvent &ev);
	void Event_ClickClose(wxCommandEvent&);
	void Event_Close(wxCloseEvent&);

	void Event_SpecularIntensity(wxCommandEvent &ev);
	void Event_RimIntensity(wxCommandEvent &ev);
	void Event_RimPower(wxCommandEvent &ev);
	void Event_RimBase(wxCommandEvent &ev);

	void Event_BumpDetailFrequency(wxCommandEvent &ev);
	void Event_BumpDetailBlend(wxCommandEvent &ev);
	void Event_BumpStrength(wxCommandEvent &ev);
	void Event_BumpThreshold(wxCommandEvent &ev);

	// Enables/disables UI elements depending on current config
	void OnUpdateUI(wxUpdateUIEvent& ev);

	// Creates controls and connects their enter/leave window events to Evt_Enter/LeaveControl
	SettingCheckBox* CreateCheckBox(wxWindow* parent, const wxString& label, const wxString& description, bool &setting, bool reverse = false, long style = 0);
	SettingChoice* CreateChoice(wxWindow* parent, int& setting, const wxString& description, int num = 0, const wxString choices[] = NULL, long style = 0);
	SettingRadioButton* CreateRadioButton(wxWindow* parent, const wxString& label, const wxString& description, bool &setting, bool reverse = false, long style = 0);

	// Same as above but only connects enter/leave window events
	wxControl* RegisterControl(wxControl* const control, const wxString& description);

	void Evt_EnterControl(wxMouseEvent& ev);
	void Evt_LeaveControl(wxMouseEvent& ev);
	void CreateDescriptionArea(wxPanel* const page, wxBoxSizer* const sizer);
	void PopulatePostProcessingShaders();
	void UpdatePostProcessingShadersConfig();
	void UpdatePostProcessingShaderListButtons();
	void PopulateScalingShaders();
	void PopulateStereoShaders();
	void PopulateAAList();
	void OnAAChanged(wxCommandEvent& ev);
	wxChoice* choice_backend;
	wxChoice* choice_adapter;
	wxChoice* choice_display_resolution;

	wxStaticText* label_backend;
	wxStaticText* label_adapter;

	wxStaticText* text_aamode;
	wxStaticText* text_bboxmode;
	wxChoice* choice_aamode;
	wxSlider* conv_slider;

	wxStaticText* label_display_resolution;

	SettingCheckBox* pixel_lighting;
	SettingCheckBox* phong_lighting;
	SettingCheckBox* sim_bump;
	wxStaticText* label_TextureScale;
	SettingCheckBox* borderless_fullscreen;
	SettingCheckBox* render_to_main_checkbox;

	SettingCheckBox* Fast_efb_cache;
	SettingCheckBox* emulate_efb_format_changes;
	SettingCheckBox* Async_Shader_compilation;
	SettingCheckBox* GPU_Texture_decoding;
	SettingCheckBox* Compute_Shader_encoding;
	SettingCheckBox* Forced_LogicOp;
	SettingCheckBox* Predictive_FIFO;
	SettingCheckBox* Wait_For_Shaders;
	SettingRadioButton* virtual_xfb;
	SettingRadioButton* real_xfb;

	SettingCheckBox* hires_texturemaps;
	SettingCheckBox* cache_hires_textures;
	SettingCheckBox* shaderprecompile;

	wxButton* button_config_scalingshader;

	wxCheckBox* progressive_scan_checkbox;
	wxCheckBox* vertex_rounding_checkbox;

	wxListBox* listbox_selected_ppshaders;
	wxButton* button_move_ppshader_up;
	wxButton* button_move_ppshader_down;
	wxButton* button_config_ppshader;
	wxButton* button_remove_ppshader;
	wxChoice* choice_ppshader;
	wxButton* button_add_ppshader;;
	wxChoice* choice_pptrigger;
	wxChoice* choice_scalingshader;
	wxChoice* choice_stereoshader;
	wxStaticBoxSizer* group_phong;
	wxStaticBoxSizer* group_Tessellation;

	wxSlider* bump_strenght_slider;
	wxSlider* bump_threshold_slider;
	wxSlider* bump_blend_slider;
	wxSlider* bump_frequency_slider;

	SettingCheckBox* validation_layer;
	SettingCheckBox* backend_multithreading;

	std::map<wxWindow*, wxString> ctrl_descs; // maps setting controls to their descriptions
	std::map<wxWindow*, wxStaticText*> desc_texts; // maps dialog tabs (which are the parents of the setting controls) to their description text objects

	VideoConfig &vconfig;
};