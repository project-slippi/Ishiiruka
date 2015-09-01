// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <map>
#include <string>
#include <utility>
#include <vector>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/control.h>
#include <wx/dialog.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/radiobut.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/stattext.h>

#include "Common/FileUtil.h"
#include "Common/SysConf.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/ConfigManager.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/VideoConfigDiag.h"
#include "DolphinWX/WxUtils.h"
#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#endif

// template instantiation
template class BoolSetting<wxCheckBox>;
template class BoolSetting<wxRadioButton>;

template <>
SettingCheckBox::BoolSetting(wxWindow* parent, const wxString& label, const wxString& tooltip, bool &setting, bool reverse, long style)
	: wxCheckBox(parent, wxID_ANY, label, wxDefaultPosition, wxDefaultSize, style)
	, m_setting(setting)
	, m_reverse(reverse)
{
	SetToolTip(tooltip);
	SetValue(m_setting ^ m_reverse);
	Bind(wxEVT_CHECKBOX, &SettingCheckBox::UpdateValue, this);
}

template <>
SettingRadioButton::BoolSetting(wxWindow* parent, const wxString& label, const wxString& tooltip, bool &setting, bool reverse, long style)
	: wxRadioButton(parent, wxID_ANY, label, wxDefaultPosition, wxDefaultSize, style)
	, m_setting(setting)
	, m_reverse(reverse)
{
	SetToolTip(tooltip);
	SetValue(m_setting ^ m_reverse);
	Bind(wxEVT_RADIOBUTTON, &SettingRadioButton::UpdateValue, this);
}

SettingChoice::SettingChoice(wxWindow* parent, int &setting, const wxString& tooltip, int num, const wxString choices[], long style)
	: wxChoice(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, num, choices)
	, m_setting(setting)
{
	SetToolTip(tooltip);
	Select(m_setting);
	Bind(wxEVT_CHOICE, &SettingChoice::UpdateValue, this);
}

void SettingChoice::UpdateValue(wxCommandEvent& ev)
{
	m_setting = ev.GetInt();
	ev.Skip();
}

void VideoConfigDiag::Event_ClickClose(wxCommandEvent&)
{
	Close();
}

void VideoConfigDiag::Event_Close(wxCloseEvent& ev)
{
	g_Config.Save((File::GetUserPath(D_CONFIG_IDX) + ininame + ".ini").c_str());

	EndModal(wxID_OK);
}

#if defined(_WIN32)
wxString backend_desc = _("Selects what graphics API to use internally.\nDirect3D 9 usually is the fastest one. OpenGL is more accurate though. Direct3D 11 is somewhere between the two.\nNote that the Direct3D backends are only available on Windows.\n\nIf unsure, use Direct3D 11.");
#else
wxString backend_desc = _("Selects what graphics API to use internally.\nDirect3D 9 usually is the fastest one. OpenGL is more accurate though. Direct3D 11 is somewhere between the two.\nNote that the Direct3D backends are only available on Windows.\n\nIf unsure, use OpenGL.");
#endif
static wxString adapter_desc = _("Select a hardware adapter to use.\n\nIf unsure, use the first one.");
static wxString display_res_desc = _("Selects the display resolution used in fullscreen mode.\nThis should always be bigger than or equal to the internal resolution. Performance impact is negligible.\n\nIf unsure, use your desktop resolution.\nIf still unsure, use the highest resolution which works for you.");
static wxString use_fullscreen_desc = _("Enable this if you want the whole screen to be used for rendering.\nIf this is disabled, a render window will be created instead.\n\nIf unsure, leave this unchecked.");
static wxString auto_window_size_desc = _("Automatically adjusts the window size to your internal resolution.\n\nIf unsure, leave this unchecked.");
static wxString keep_window_on_top_desc = _("Keep the game window on top of all other windows.\n\nIf unsure, leave this unchecked.");
static wxString hide_mouse_cursor_desc = _("Hides the mouse cursor if it's on top of the emulation window.\n\nIf unsure, leave this checked.");
static wxString render_to_main_win_desc = _("Enable this if you want to use the main Dolphin window for rendering rather than a separate render window.\n\nIf unsure, leave this unchecked.");
static wxString prog_scan_desc = _("Enables progressive scan if supported by the emulated software.\nMost games don't care about this.\n\nIf unsure, leave this unchecked.");
static wxString ar_desc = _("Select what aspect ratio to use when rendering:\nAuto: Use the native aspect ratio\nForce 16:9: Mimic an analog TV with a widescreen aspect ratio.\nForce 4:3: Mimic a standard 4:3 analog TV.\nStretch to Window: Stretch the picture to the window size.\n\nIf unsure, select Auto.");
static wxString ws_hack_desc = _("Force the game to output graphics for widescreen resolutions.\nCauses graphical glitches is some games.\n\nIf unsure, leave this unchecked.");
static wxString vsync_desc = _("Wait for vertical blanks in order to reduce tearing.\nDecreases performance if emulation speed is below 100%.\n\nIf unsure, leave this unchecked.");
static wxString af_desc = _("Enable anisotropic filtering.\nEnhances visual quality of textures that are at oblique viewing angles.\nMight cause issues in a small number of games.\n\nIf unsure, select 1x.");
static wxString aa_desc = _("Reduces the amount of aliasing caused by rasterizing 3D graphics.\nThis makes the rendered picture look less blocky.\nHeavily decreases emulation speed and sometimes causes issues.\n\nIf unsure, select None.");
static wxString scaled_efb_copy_desc = _("Greatly increases quality of textures generated using render to texture effects.\nRaising the internal resolution will improve the effect of this setting.\nSlightly decreases performance and possibly causes issues (although unlikely).\n\nIf unsure, leave this checked.");
static wxString pixel_lighting_desc = _("Calculate lighting of 3D graphics per-pixel rather than per vertex.\nDecreases emulation speed by some percent (depending on your GPU).\nThis usually is a safe enhancement, but might cause issues sometimes.\n\nIf unsure, leave this unchecked.");
static wxString hacked_buffer_upload_desc = _("Uses unsafe operations to speed up vertex streaming in OpenGL. There are no known problems on supported GPUs, but it will cause severe stability and graphical issues otherwise.\n\nIf unsure, leave this unchecked.");
static wxString fast_depth_calc_desc = _("Use a less accurate algorithm to calculate depth values.\nCauses issues in a few games but might give a decent speedup.\n\nIf unsure, leave this checked.");
static wxString force_filtering_desc = _("Force texture filtering even if the emulated game explicitly disabled it.\nImproves texture quality slightly but causes glitches in some games.\n\nIf unsure, leave this unchecked.");
static wxString Use_Scaling_filter_desc = _("Use filtering when efb scaled size is larger than the target resolution.");
static wxString borderless_fullscreen_desc = _("Implement fullscreen mode with a borderless window spanning the whole screen instead of using exclusive mode.\nAllows for faster transitions between fullscreen and windowed mode, but increases input latency, makes movement less smooth and slightly decreases performance.\nExclusive mode is required to support Nvidia 3D Vision in the Direct3D backend.\n\nIf unsure, leave this unchecked.");
static wxString internal_res_desc = _("Specifies the resolution used to render at. A high resolution greatly improves visual quality, but also greatly increases GPU load and can cause issues in certain games.\n\"Multiple of 640x528\" will result in a size slightly larger than \"Window Size\" but yield fewer issues. Generally speaking, the lower the internal resolution is, the better your performance will be. Auto (Window Size), 1.5x, and 2.5x may cause issues in some games.\n\nIf unsure, select Native.");
static wxString efb_access_desc = _("Ignore any requests of the CPU to read from or write to the EFB.\nImproves performance in some games, but might disable some gameplay-related features or graphical effects.\n\nIf unsure, leave this unchecked.");
static wxString efb_fast_access_desc = _("Use a fast efb caching method to speed up access. This method is inaccurate but will make games run faster and efb reads and writes will still work.");
static wxString efb_emulate_format_changes_desc = _("Ignore any changes to the EFB format.\nImproves performance in many games without any negative effect. Causes graphical defects in a small number of other games though.\n\nIf unsure, leave this checked.");
static wxString skip_efb_copy_to_ram_desc = wxTRANSLATE("Stores EFB Copies exclusively on the GPU, bypassing system memory. Causes graphical defects in a small number of games.\n\nEnabled = EFB Copies to Texture\nDisabled = EFB Copies to RAM (and Texture)\n\nIf unsure, leave this checked.");
static wxString stc_desc = _("The safer you adjust this, the less likely the emulator will be missing any texture updates from RAM.\n\nIf unsure, use the rightmost value.");
static wxString bbox_desc = _("Selects wish implementation is used to emulate Bounding Box. By Default GPU will be used if supported.");
static wxString wireframe_desc = _("Render the scene as a wireframe.\n\nIf unsure, leave this unchecked.");
static wxString disable_fog_desc = _("Makes distant objects more visible by removing fog, thus increasing the overall detail.\nDisabling fog will break some games which rely on proper fog emulation.\n\nIf unsure, leave this unchecked.");
static wxString disable_dstalpha_desc = _("Disables emulation of a hardware feature called destination alpha, which is used in many games for various graphical effects.\n\nIf unsure, leave this unchecked.");
static wxString show_fps_desc = _("Show the number of frames rendered per second as a measure of emulation speed.\n\nIf unsure, leave this unchecked.");
static wxString log_fps_to_file_desc = _("Log the number of frames rendered per second to User/Logs/fps.txt. Use this feature when you want to measure the performance of Dolphin.\n\nIf unsure, leave this unchecked.");
static wxString show_input_display_desc = _("Display the inputs read by the emulator.\n\nIf unsure, leave this unchecked.");
static wxString show_stats_desc = _("Show various statistics.\n\nIf unsure, leave this unchecked.");
static wxString texfmt_desc = _("Modify textures to show the format they're encoded in. Needs an emulation reset in most cases.\n\nIf unsure, leave this unchecked.");
static wxString efb_copy_regions_desc = _("[BROKEN]\nHighlight regions the EFB was copied from.\n\nIf unsure, leave this unchecked.");
static wxString xfb_desc = _("Disable any XFB emulation.\nSpeeds up emulation a lot but causes heavy glitches in many games which rely on them (especially homebrew applications).\n\nIf unsure, leave this checked.");
static wxString xfb_virtual_desc = _("Emulate XFBs using GPU texture objects.\nFixes many games which don't work without XFB emulation while not being as slow as real XFB emulation. However, it may still fail for a lot of other games (especially homebrew applications).\n\nIf unsure, leave this checked.");
static wxString xfb_real_desc = _("Emulate XFBs accurately.\nSlows down emulation a lot and prohibits high-resolution rendering but is necessary to emulate a number of games properly.\n\nIf unsure, check virtual XFB emulation instead.");
static wxString dump_textures_desc = _("Dump decoded game textures to User/Dump/Textures/<game_id>/\n\nIf unsure, leave this unchecked.");
static wxString dump_VertexTranslators_desc = _("Dump Vertex translator code to User/Dump/\n\nIf unsure, leave this unchecked.");
static wxString fullAsyncShaderCompilation_desc = _("Make shader compilation proccess fully asynchronous. This can cause glitches but will give a smooth game experience.");
static wxString waitforshadercompilation_desc = _("Wait for shader compilation in the cpu to avoid fifo problems. This option prevents loops in F-Zero, Metroid Prime fifo resets and others.");
static wxString predictiveFifo_desc = _("Generate a secondary fifo to predict resource usage and improve loading time.");
static wxString load_hires_textures_desc = _("Load custom textures from User/Load/Textures/<game_id>/\n\nIf unsure, leave this unchecked.");
static wxString cache_hires_textures_desc = _("Cache custom textures to system RAM on startup.\nThis can require exponentially more RAM but fixes possible stuttering.\n\nIf unsure, leave this unchecked.");
static wxString dump_efb_desc = _("Dump the contents of EFB copies to User/Dump/Textures/\n\nIf unsure, leave this unchecked.");
static wxString dump_frames_desc = _("Dump all rendered frames to an AVI file in User/Dump/Frames/\n\nIf unsure, leave this unchecked.");
#if !defined WIN32 && defined HAVE_LIBAV
static wxString use_ffv1_desc = _("Encode frame dumps using the FFV1 codec.\n\nIf unsure, leave this unchecked.");
#endif
static wxString free_look_desc = _("This feature allows you to change the game's camera.\nMove the mouse while holding the right mouse button to pan and while holding the middle button to move.\nHold SHIFT and press one of the WASD keys to move the camera by a certain step distance (SHIFT+0 to move faster and SHIFT+9 to move slower). Press SHIFT+R to reset the camera.\n\nIf unsure, leave this unchecked.");
static wxString crop_desc = _("Crop the picture from its native aspect ratio to 4:3 or 16:9.\n\nIf unsure, leave this unchecked.");
static wxString opencl_desc = _("[EXPERIMENTAL]\nAims to speed up emulation by offloading texture decoding to the GPU using the OpenCL framework.\nHowever, right now it's known to cause texture defects in various games. Also it's slower than regular CPU texture decoding in most cases.\n\nIf unsure, leave this unchecked.");
static wxString ppshader_desc = _("Apply a post-processing effect after finishing a frame.\n\nIf unsure, select (off).");
static wxString shader_errors_desc = _("Usually if shader compilation fails, an error message is displayed.\nHowever, one may skip the popups to allow interruption free gameplay by checking this option.\n\nIf unsure, leave this unchecked.");
static wxString stereo_3d_desc = _("Select the stereoscopic 3D  mode, stereoscopy allows you to get a better feeling of depth if you have the necessary hardware.\nSide-by-Side and Top-and-Bottom are used by most 3D TVs.\nAnaglyph is used for Red-Cyan colored glasses.\nHeavily decreases emulation speed and sometimes causes issues.\n\nIf unsure, select Off.");
static wxString stereo_separation_desc = _("Control the separation distance, this is the distance between the virtual cameras.\nA higher value creates a stronger feeling of depth while a lower value is more comfortable.");
static wxString stereo_convergence_desc = _("Control the convergence distance, this controls the apparant distance of virtual objects.\nA higher value creates stronger out-of-screen effects while a lower value is more comfortable.");
static wxString stereo_swap_desc = _("Swap the left and right eye, mostly useful if you want to view side-by-side cross-eyed.\n\nIf unsure, leave this unchecked.");
static const char *s_bbox_mode_text[] = { "Disabled", "CPU", "GPU" };
static wxString texture_scaling_desc = _("Apply the selected scaling algorithm to improve texture quality.");
static wxString scaling_factor_desc = _("Multiplier applied to the texture size.");
static wxString texture_deposterize_desc = _("Decrease some gradient's artifacts caused by scaling.");

// Search for available resolutions - TODO: Move to Common?
static  wxArrayString GetListOfResolutions()
{
	wxArrayString retlist;
	retlist.Add(_("Auto"));
#ifdef _WIN32
	DWORD iModeNum = 0;
	DEVMODE dmi;
	ZeroMemory(&dmi, sizeof(dmi));
	dmi.dmSize = sizeof(dmi);
	std::vector<std::string> resos;

	while (EnumDisplaySettings(nullptr, iModeNum++, &dmi) != 0)
	{
		char res[100];
		sprintf(res, "%dx%d", dmi.dmPelsWidth, dmi.dmPelsHeight);
		std::string strRes(res);
		// Only add unique resolutions
		if (std::find(resos.begin(), resos.end(), strRes) == resos.end())
		{
			resos.push_back(strRes);
			retlist.Add(StrToWxStr(res));
		}
		ZeroMemory(&dmi, sizeof(dmi));
	}
#elif defined(HAVE_XRANDR) && HAVE_XRANDR
	std::vector<std::string> resos;
	main_frame->m_XRRConfig->AddResolutions(resos);
	for (auto res : resos)
		retlist.Add(StrToWxStr(res));
#elif defined(__APPLE__)
	CFArrayRef modes = CGDisplayCopyAllDisplayModes(CGMainDisplayID(), nullptr);
	for (CFIndex i = 0; i < CFArrayGetCount(modes); i++)
	{
		std::stringstream res;
		CGDisplayModeRef mode;
		CFStringRef encoding;
		size_t w, h;
		bool is32;

		mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i);
		w = CGDisplayModeGetWidth(mode);
		h = CGDisplayModeGetHeight(mode);
		encoding = CGDisplayModeCopyPixelEncoding(mode);
		is32 = CFEqual(encoding, CFSTR(IO32BitDirectPixels));
		CFRelease(encoding);

		if (!is32)
			continue;
		if (CGDisplayModeGetIOFlags(mode) & kDisplayModeStretchedFlag)
			continue;

		res << w << "x" << h;

		retlist.Add(res.str());
	}
	CFRelease(modes);
#endif
	return retlist;
}

VideoConfigDiag::VideoConfigDiag(wxWindow* parent, const std::string &title, const std::string& _ininame)
	: wxDialog(parent, wxID_ANY,
		wxString::Format(_("Dolphin %s Graphics Configuration"), StrToWxStr(title)),
		wxDefaultPosition, wxDefaultSize)
	, vconfig(g_Config)
	, ininame(_ininame)
{
	vconfig.Load((File::GetUserPath(D_CONFIG_IDX) + ininame + ".ini").c_str());

	Bind(wxEVT_UPDATE_UI, &VideoConfigDiag::OnUpdateUI, this);

	wxNotebook* const notebook = new wxNotebook(this, wxID_ANY);

	// -- GENERAL --
	{
	wxPanel* const page_general = new wxPanel(notebook, -1, wxDefaultPosition);
	notebook->AddPage(page_general, _("General"));
	wxBoxSizer* const szr_general = new wxBoxSizer(wxVERTICAL);

	// - basic
	{
	wxFlexGridSizer* const szr_basic = new wxFlexGridSizer(2, 5, 5);

	// backend
	{
	label_backend = new wxStaticText(page_general, wxID_ANY, _("Backend:"));
	choice_backend = new wxChoice(page_general, wxID_ANY, wxDefaultPosition);
	RegisterControl(choice_backend, backend_desc);

	for (const VideoBackend* backend : g_available_video_backends)
	{
		choice_backend->AppendString(StrToWxStr(backend->GetDisplayName()));
	}

	choice_backend->SetStringSelection(StrToWxStr(g_video_backend->GetDisplayName()));
	choice_backend->Bind(wxEVT_CHOICE, &VideoConfigDiag::Event_Backend, this);

	szr_basic->Add(label_backend, 1, wxALIGN_CENTER_VERTICAL, 5);
	szr_basic->Add(choice_backend, 1, 0, 0);
	}

	// adapter (D3D only)
	if (vconfig.backend_info.Adapters.size())
	{
		choice_adapter = CreateChoice(page_general, vconfig.iAdapter, adapter_desc);

		for (const std::string& adapter : vconfig.backend_info.Adapters)
		{
			choice_adapter->AppendString(StrToWxStr(adapter));
		}

		choice_adapter->Select(vconfig.iAdapter);

		label_adapter = new wxStaticText(page_general, wxID_ANY, _("Adapter:"));
		szr_basic->Add(label_adapter, 1, wxALIGN_CENTER_VERTICAL, 5);
		szr_basic->Add(choice_adapter, 1, 0, 0);
	}


	// - display
	wxFlexGridSizer* const szr_display = new wxFlexGridSizer(2, 5, 5);

	{

#if !defined(__APPLE__)
	// display resolution
	{
		wxArrayString res_list = GetListOfResolutions();
		if (res_list.empty())
			res_list.Add(_("<No resolutions found>"));
		label_display_resolution = new wxStaticText(page_general, wxID_ANY, _("Fullscreen resolution:"));
		choice_display_resolution = new wxChoice(page_general, wxID_ANY, wxDefaultPosition, wxDefaultSize, res_list);
		RegisterControl(choice_display_resolution, (display_res_desc));
		choice_display_resolution->Bind(wxEVT_CHOICE, &VideoConfigDiag::Event_DisplayResolution, this);

		choice_display_resolution->SetStringSelection(StrToWxStr(SConfig::GetInstance().strFullscreenResolution));

		szr_display->Add(label_display_resolution, 1, wxALIGN_CENTER_VERTICAL, 0);
		szr_display->Add(choice_display_resolution);
	}
#endif

	// aspect-ratio
	{
	const wxString ar_choices[] = { _("Auto"), _("Force 16:9"), _("Force 4:3"), _("Stretch to Window") };

	szr_display->Add(new wxStaticText(page_general, wxID_ANY, _("Aspect Ratio:")), 1, wxALIGN_CENTER_VERTICAL, 0);
	wxChoice* const choice_aspect = CreateChoice(page_general, vconfig.iAspectRatio, (ar_desc),
														sizeof(ar_choices)/sizeof(*ar_choices), ar_choices);
	szr_display->Add(choice_aspect, 1, 0, 0);
	}

	// various other display options
	{
	szr_display->Add(CreateCheckBox(page_general, _("V-Sync"), (vsync_desc), vconfig.bVSync));
	szr_display->Add(CreateCheckBox(page_general, _("Use Fullscreen"), (use_fullscreen_desc), SConfig::GetInstance().bFullscreen));
	}
	}

	// - other
	wxFlexGridSizer* const szr_other = new wxFlexGridSizer(2, 5, 5);

	{
	szr_other->Add(CreateCheckBox(page_general, _("Show FPS"), (show_fps_desc), vconfig.bShowFPS));
	szr_other->Add(CreateCheckBox(page_general, _("Auto adjust Window Size"), (auto_window_size_desc), SConfig::GetInstance().bRenderWindowAutoSize));
	szr_other->Add(CreateCheckBox(page_general, _("Keep window on top"), (keep_window_on_top_desc), SConfig::GetInstance().bKeepWindowOnTop));
	szr_other->Add(CreateCheckBox(page_general, _("Hide Mouse Cursor"), (hide_mouse_cursor_desc), SConfig::GetInstance().bHideCursor));
	szr_other->Add(render_to_main_checkbox = CreateCheckBox(page_general, _("Render to Main Window"), (render_to_main_win_desc), SConfig::GetInstance().bRenderToMain));
	}


	wxStaticBoxSizer* const group_basic = new wxStaticBoxSizer(wxVERTICAL, page_general, _("Basic"));
	group_basic->Add(szr_basic, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	szr_general->Add(group_basic, 0, wxEXPAND | wxALL, 5);

	wxStaticBoxSizer* const group_display = new wxStaticBoxSizer(wxVERTICAL, page_general, _("Display"));
	group_display->Add(szr_display, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	szr_general->Add(group_display, 0, wxEXPAND | wxALL, 5);

	wxStaticBoxSizer* const group_other = new wxStaticBoxSizer(wxVERTICAL, page_general, _("Other"));
	group_other->Add(szr_other, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	szr_general->Add(group_other, 0, wxEXPAND | wxALL, 5);
	}

	szr_general->AddStretchSpacer();
	CreateDescriptionArea(page_general, szr_general);
	page_general->SetSizerAndFit(szr_general);
	}

	// -- ENHANCEMENTS --
	{
	wxPanel* const page_enh = new wxPanel(notebook);
	notebook->AddPage(page_enh, _("Enhancements"));
	wxBoxSizer* const szr_enh_main = new wxBoxSizer(wxVERTICAL);

	// - enhancements
	wxFlexGridSizer* const szr_enh = new wxFlexGridSizer(3, 5, 5);

	// Internal resolution
	{
	const wxString efbscale_choices[] = { _("Auto (Window Size)"), _("Auto (Multiple of 640x528)"),
		_("1x (640x528)"), _("1.5x (960x792)"), _("2x (1280x1056) for 720p"), _("2.5x (1600x1320)"),
		_("3x (1920x1584) for 1080p"), _("4x (2560x2112) for WQHD"), _("5x (3200x2640)"),
		_("6x (3840x3168) for 4K UHD"), _("7x (4480x3696)"), _("8x (5120x4224)"), _("Custom") };


	wxChoice *const choice_efbscale = CreateChoice(page_enh,
		vconfig.iEFBScale, (internal_res_desc), (vconfig.iEFBScale > 11) ?
		ArraySize(efbscale_choices) : ArraySize(efbscale_choices) - 1, efbscale_choices);


	if (vconfig.iEFBScale > 11)
		choice_efbscale->SetSelection(12);

	szr_enh->Add(new wxStaticText(page_enh, wxID_ANY, _("Internal Resolution:")), 1, wxALIGN_CENTER_VERTICAL, 0);
	szr_enh->Add(choice_efbscale);
	szr_enh->AddSpacer(0);
	}
	
	// AA
	{
	text_aamode = new wxStaticText(page_enh, wxID_ANY, _("Anti-Aliasing:"));
	choice_aamode = CreateChoice(page_enh, vconfig.iMultisampleMode, (aa_desc));

	for (const std::string& mode : vconfig.backend_info.AAModes)
	{
		choice_aamode->AppendString((StrToWxStr(mode)));
	}

	choice_aamode->Select(vconfig.iMultisampleMode);
	szr_enh->Add(text_aamode, 1, wxALIGN_CENTER_VERTICAL, 0);
	szr_enh->Add(choice_aamode);
	szr_enh->AddSpacer(0);
	}

	// AF
	{
	const wxString af_choices[] = {wxT("1x"), wxT("2x"), wxT("4x"), wxT("8x"), wxT("16x")};
	szr_enh->Add(new wxStaticText(page_enh, wxID_ANY, _("Anisotropic Filtering:")), 1, wxALIGN_CENTER_VERTICAL, 0);
	szr_enh->Add(CreateChoice(page_enh, vconfig.iMaxAnisotropy, (af_desc), 5, af_choices));
	szr_enh->AddSpacer(0);
	}

	// postproc shader
	if (vconfig.backend_info.PPShaders.size())
	{
		choice_ppshader = new wxChoice(page_enh, wxID_ANY);
		RegisterControl(choice_ppshader, (ppshader_desc));
		button_config_pp = new wxButton(page_enh, wxID_ANY, _("Config"));

		PopulatePostProcessingShaders();

		choice_ppshader->Bind(wxEVT_CHOICE, &VideoConfigDiag::Event_PPShader, this);
		button_config_pp->Bind(wxEVT_BUTTON, &VideoConfigDiag::Event_ConfigurePPShader, this);

		szr_enh->Add(new wxStaticText(page_enh, wxID_ANY, _("Post-Processing Effect:")), 1, wxALIGN_CENTER_VERTICAL, 0);
		szr_enh->Add(choice_ppshader);
		szr_enh->Add(button_config_pp);
	}
	else
	{
		choice_ppshader = nullptr;
		button_config_pp = nullptr;
	}

	// Scaled copy, PL, Bilinear filter, 3D Vision
	szr_enh->Add(CreateCheckBox(page_enh, _("Scaled EFB Copy"), (scaled_efb_copy_desc), vconfig.bCopyEFBScaled));
	szr_enh->Add(CreateCheckBox(page_enh, _("Use Scaling Filter"), (Use_Scaling_filter_desc), vconfig.bUseScalingFilter));
	szr_enh->Add(CreateCheckBox(page_enh, _("Force Texture Filtering"), (force_filtering_desc), vconfig.bForceFiltering));
	
	szr_enh->Add(CreateCheckBox(page_enh, _("Widescreen Hack"), (ws_hack_desc), vconfig.bWidescreenHack));
	szr_enh->Add(CreateCheckBox(page_enh, _("Disable Fog"), (disable_fog_desc), vconfig.bDisableFog));
	szr_enh->Add(pixel_lighting = CreateCheckBox(page_enh, _("Per-Pixel Lighting"), (pixel_lighting_desc), vconfig.bEnablePixelLighting));

	wxStaticBoxSizer* const group_enh = new wxStaticBoxSizer(wxVERTICAL, page_enh, _("Enhancements"));
	group_enh->Add(szr_enh, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	szr_enh_main->Add(group_enh, 0, wxEXPAND | wxALL, 5);

	// - stereoscopy

	if (vconfig.backend_info.bSupportsGeometryShaders)
	{
		wxFlexGridSizer* const szr_stereo = new wxFlexGridSizer(3, 5, 5);

		szr_stereo->Add(new wxStaticText(page_enh, wxID_ANY, _("Stereoscopic 3D Mode:")), 1, wxALIGN_CENTER_VERTICAL, 0);

		const wxString stereo_choices[] = { "Off", "Side-by-Side", "Top-and-Bottom", "Anaglyph", "Interlaced", "Nvidia 3D Vision" };
		wxChoice* stereo_choice = CreateChoice(page_enh, vconfig.iStereoMode, (stereo_3d_desc), vconfig.backend_info.bSupports3DVision ? ArraySize(stereo_choices) : ArraySize(stereo_choices) - 1, stereo_choices);
		stereo_choice->Bind(wxEVT_CHOICE, &VideoConfigDiag::Event_StereoMode, this);
		szr_stereo->Add(stereo_choice);

		szr_stereo->Add(CreateCheckBox(page_enh, _("Swap Eyes"), (stereo_swap_desc), vconfig.bStereoSwapEyes), 1, wxALIGN_CENTER_VERTICAL, 0);

		wxSlider* const sep_slider = new wxSlider(page_enh, wxID_ANY, vconfig.iStereoDepth, 0, 100, wxDefaultPosition, wxDefaultSize);
		sep_slider->Bind(wxEVT_SLIDER, &VideoConfigDiag::Event_StereoDepth, this);
		RegisterControl(sep_slider, (stereo_separation_desc));

		szr_stereo->Add(new wxStaticText(page_enh, wxID_ANY, _("Separation:")), 1, wxALIGN_CENTER_VERTICAL, 0);
		szr_stereo->Add(sep_slider, 1, wxEXPAND | wxRIGHT);
		szr_stereo->AddSpacer(0);
		wxSlider* const conv_slider = new wxSlider(page_enh, wxID_ANY, vconfig.iStereoConvergence, 0, 500, wxDefaultPosition, wxDefaultSize);
		conv_slider->Bind(wxEVT_SLIDER, &VideoConfigDiag::Event_StereoConvergence, this);
		RegisterControl(conv_slider, (stereo_convergence_desc));

		szr_stereo->Add(new wxStaticText(page_enh, wxID_ANY, _("Convergence:")), 1, wxALIGN_CENTER_VERTICAL, 0);
		szr_stereo->Add(conv_slider, 1, wxEXPAND | wxRIGHT);

		wxStaticBoxSizer* const group_stereo = new wxStaticBoxSizer(wxVERTICAL, page_enh, _("Stereoscopy"));
		group_stereo->Add(szr_stereo, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
		szr_enh_main->Add(group_stereo, 2, wxEXPAND | wxALL, 5);
	}

	wxFlexGridSizer* const szr_texturescaling = new wxFlexGridSizer(3, 5, 5);

	szr_texturescaling->Add(new wxStaticText(page_enh, wxID_ANY, _("Texture Scaling Mode:")), 1, wxALIGN_CENTER_VERTICAL, 0);

	const wxString scaling_choices[] = { "Off", "XBRZ", "Hybrid", "Bicubic", "Hybrid-Bicubic" };
	wxChoice* scaling_choice = CreateChoice(page_enh, vconfig.iTexScalingType, (texture_scaling_desc), ArraySize(scaling_choices), scaling_choices);
	szr_texturescaling->Add(scaling_choice);
	
	szr_texturescaling->Add(CreateCheckBox(page_enh, _("DePosterize"), (texture_deposterize_desc), vconfig.bTexDeposterize), 1, wxALIGN_CENTER_VERTICAL);

	wxSlider* const factor_slider = new wxSlider(
		page_enh,
		wxID_ANY,
		vconfig.iTexScalingFactor,
		2,
		5,
		wxDefaultPosition, wxDefaultSize,
		wxSL_HORIZONTAL | wxSL_BOTTOM);
	factor_slider->Bind(wxEVT_SLIDER, &VideoConfigDiag::Event_ScalingFactor, this);
	RegisterControl(factor_slider, (scaling_factor_desc));
	
	szr_texturescaling->Add(new wxStaticText(page_enh, wxID_ANY, _("Scaling factor:")), 0, wxALL, 5);
	szr_texturescaling->Add(factor_slider, 3, wxRIGHT, 0);
	const wxString sf_choices[] = { wxT("1x"), wxT("2x"), wxT("3x"), wxT("4x"), wxT("5x") };
	szr_texturescaling->Add(label_TextureScale = new wxStaticText(page_enh, wxID_ANY, sf_choices[vconfig.iTexScalingFactor - 1]), 1, wxRIGHT | wxTOP | wxBOTTOM, 5);

	

	wxStaticBoxSizer* const group_scaling = new wxStaticBoxSizer(wxVERTICAL, page_enh, _("Texture Scaling"));
	group_scaling->Add(szr_texturescaling, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	szr_enh_main->Add(group_scaling, 0, wxEXPAND | wxALL, 5);
	
	szr_enh_main->AddStretchSpacer();
	CreateDescriptionArea(page_enh, szr_enh_main);
	page_enh->SetSizerAndFit(szr_enh_main);
	}




	// -- SPEED HACKS --
	{
	wxPanel* const page_hacks = new wxPanel(notebook, -1, wxDefaultPosition);
	notebook->AddPage(page_hacks, _("Hacks"));
	wxBoxSizer* const szr_hacks = new wxBoxSizer(wxVERTICAL);

	// - EFB hacks
	wxStaticBoxSizer* const szr_efb = new wxStaticBoxSizer(wxVERTICAL, page_hacks, _("Embedded Frame Buffer"));

	// format change emulation
	emulate_efb_format_changes = CreateCheckBox(page_hacks, _("Ignore Format Changes"), (efb_emulate_format_changes_desc), vconfig.bEFBEmulateFormatChanges, true);
	
	szr_efb->Add(CreateCheckBox(page_hacks, _("Skip EFB Access from CPU"), (efb_access_desc), vconfig.bEFBAccessEnable, true), 0, wxBOTTOM | wxLEFT, 5);
	Fast_efb_cache = CreateCheckBox(page_hacks, _("Fast EFB Access"), (efb_fast_access_desc), vconfig.bEFBFastAccess, false);
	szr_efb->Add(Fast_efb_cache, 0, wxBOTTOM | wxLEFT, 5);
	szr_efb->Add(emulate_efb_format_changes, 0, wxBOTTOM | wxLEFT, 5);
	szr_efb->Add(CreateCheckBox(page_hacks, _("Store EFB copies to Texture Only"), (skip_efb_copy_to_ram_desc), vconfig.bSkipEFBCopyToRam), 0, wxBOTTOM | wxLEFT, 5);
	szr_hacks->Add(szr_efb, 0, wxEXPAND | wxALL, 5);

	// Texture cache
	{
	wxStaticBoxSizer* const szr_safetex = new wxStaticBoxSizer(wxHORIZONTAL, page_hacks, _("Texture Cache"));

	// TODO: Use wxSL_MIN_MAX_LABELS or wxSL_VALUE_LABEL with wx 2.9.1
	wxSlider* const stc_slider = new wxSlider(page_hacks, wxID_ANY, 0, 0, 2, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL|wxSL_BOTTOM);
	stc_slider->Bind(wxEVT_COMMAND_SLIDER_UPDATED, &VideoConfigDiag::Event_Stc, this);
	RegisterControl(stc_slider, (stc_desc));

	if (vconfig.iSafeTextureCache_ColorSamples == 0) stc_slider->SetValue(0);
	else if (vconfig.iSafeTextureCache_ColorSamples == 512) stc_slider->SetValue(1);
	else if (vconfig.iSafeTextureCache_ColorSamples == 128) stc_slider->SetValue(2);
	else stc_slider->Disable(); // Using custom number of samples; TODO: Inform the user why this is disabled..

	szr_safetex->Add(new wxStaticText(page_hacks, wxID_ANY, _("Accuracy:")), 0, wxALL, 5);
	szr_safetex->AddStretchSpacer(1);
	szr_safetex->Add(new wxStaticText(page_hacks, wxID_ANY, _("Safe")), 0, wxLEFT|wxTOP|wxBOTTOM, 5);
	szr_safetex->Add(stc_slider, 2, wxRIGHT, 0);
	szr_safetex->Add(new wxStaticText(page_hacks, wxID_ANY, _("Fast")), 0, wxRIGHT|wxTOP|wxBOTTOM, 5);
	szr_hacks->Add(szr_safetex, 0, wxEXPAND | wxALL, 5);
	}
	// - XFB
	{
	wxStaticBoxSizer* const group_xfb = new wxStaticBoxSizer(wxHORIZONTAL, page_hacks, _("External Frame Buffer"));

	SettingCheckBox* disable_xfb = CreateCheckBox(page_hacks, _("Disable"), (xfb_desc), vconfig.bUseXFB, true);
	virtual_xfb = CreateRadioButton(page_hacks, _("Virtual"), (xfb_virtual_desc), vconfig.bUseRealXFB, true, wxRB_GROUP);
	real_xfb = CreateRadioButton(page_hacks, _("Real"), (xfb_real_desc), vconfig.bUseRealXFB);

	group_xfb->Add(disable_xfb, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
	group_xfb->AddStretchSpacer(1);
	group_xfb->Add(virtual_xfb, 0, wxRIGHT, 5);
	group_xfb->Add(real_xfb, 0, wxRIGHT, 5);
	szr_hacks->Add(group_xfb, 0, wxEXPAND | wxALL, 5);
	}	// xfb
	// Bounding Box
	{
	wxStaticBoxSizer* const group_bbox = new wxStaticBoxSizer(wxHORIZONTAL, page_hacks, _("Bounding Box"));

	wxSlider* const bbox_slider = new wxSlider(
		page_hacks,
		wxID_ANY, 0, 0,
		(vconfig.backend_info.APIType & API_D3D9) == 0 ? 2 : 1,
		wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_BOTTOM);
	bbox_slider->Bind(wxEVT_COMMAND_SLIDER_UPDATED, &VideoConfigDiag::Event_Bbox, this);
	RegisterControl(bbox_slider, (bbox_desc));



	group_bbox->Add(new wxStaticText(page_hacks, wxID_ANY, _("Mode:")), 0, wxALL, 5);
	group_bbox->AddStretchSpacer(0);
	group_bbox->Add(bbox_slider, 3, wxRIGHT, 0);
	group_bbox->Add(text_bboxmode = new wxStaticText(page_hacks, wxID_ANY, _("GPU")), 1, wxRIGHT | wxTOP | wxBOTTOM, 5);
	szr_hacks->Add(group_bbox, 0, wxEXPAND | wxALL, 5);
	bbox_slider->SetValue(vconfig.iBBoxMode);
	text_bboxmode->SetLabel((s_bbox_mode_text[vconfig.iBBoxMode]));
	}
	// - other hacks
	{
	wxGridSizer* const szr_other = new wxGridSizer(2, 5, 5);	
	szr_other->Add(CreateCheckBox(page_hacks, _("Disable Destination Alpha"), (disable_dstalpha_desc), vconfig.bDstAlphaPass));
	// Disable while i fix opencl
	//szr_other->Add(CreateCheckBox(page_hacks, _("OpenCL Texture Decoder"), (opencl_desc), vconfig.bEnableOpenCL));	
	szr_other->Add(CreateCheckBox(page_hacks, _("Fast Depth Calculation"), (fast_depth_calc_desc), vconfig.bFastDepthCalc));
	szr_other->Add(Predictive_FIFO = CreateCheckBox(page_hacks, _("Predictive FIFO"), (predictiveFifo_desc), vconfig.bPredictiveFifo));
	szr_other->Add(Wait_For_Shaders = CreateCheckBox(page_hacks, _("Wait for Shader Compilation"), (waitforshadercompilation_desc), vconfig.bWaitForShaderCompilation));
	szr_other->Add(Async_Shader_compilation = CreateCheckBox(page_hacks, _("Full Async Shader Compilation"), (fullAsyncShaderCompilation_desc), vconfig.bFullAsyncShaderCompilation));	
	wxStaticBoxSizer* const group_other = new wxStaticBoxSizer(wxVERTICAL, page_hacks, _("Other"));
	group_other->Add(szr_other, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	szr_hacks->Add(group_other, 0, wxEXPAND | wxALL, 5);
	}

	szr_hacks->AddStretchSpacer();
	CreateDescriptionArea(page_hacks, szr_hacks);
	page_hacks->SetSizerAndFit(szr_hacks);
	}

	// -- ADVANCED --
	{
	wxPanel* const page_advanced = new wxPanel(notebook, -1, wxDefaultPosition);
	notebook->AddPage(page_advanced, _("Advanced"));
	wxBoxSizer* const szr_advanced = new wxBoxSizer(wxVERTICAL);

	// - debug
	{
	wxGridSizer* const szr_debug = new wxGridSizer(2, 5, 5);

	szr_debug->Add(CreateCheckBox(page_advanced, _("Enable Wireframe"), (wireframe_desc), vconfig.bWireFrame));
	szr_debug->Add(CreateCheckBox(page_advanced, _("Show EFB Copy Regions"), (efb_copy_regions_desc), vconfig.bShowEFBCopyRegions));
	szr_debug->Add(CreateCheckBox(page_advanced, _("Show Statistics"), (show_stats_desc), vconfig.bOverlayStats));
	szr_debug->Add(CreateCheckBox(page_advanced, _("Texture Format Overlay"), (texfmt_desc), vconfig.bTexFmtOverlayEnable));

	wxStaticBoxSizer* const group_debug = new wxStaticBoxSizer(wxVERTICAL, page_advanced, _("Debugging"));
	szr_advanced->Add(group_debug, 0, wxEXPAND | wxALL, 5);
	group_debug->Add(szr_debug, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	}

	// - utility
	{
	wxGridSizer* const szr_utility = new wxGridSizer(2, 5, 5);

	szr_utility->Add(CreateCheckBox(page_advanced, _("Dump Textures"), (dump_textures_desc), vconfig.bDumpTextures));
	szr_utility->Add(CreateCheckBox(page_advanced, _("Dump Vertex Loaders"), (dump_VertexTranslators_desc), vconfig.bDumpVertexLoaders));
	szr_utility->Add(CreateCheckBox(page_advanced, _("Load Custom Textures"), (load_hires_textures_desc), vconfig.bHiresTextures));
	cache_hires_textures = CreateCheckBox(page_advanced, _("Prefetch Custom Textures"), cache_hires_textures_desc, vconfig.bCacheHiresTextures);
	szr_utility->Add(cache_hires_textures);
	szr_utility->Add(CreateCheckBox(page_advanced, _("Dump EFB Target"), (dump_efb_desc), vconfig.bDumpEFBTarget));
	szr_utility->Add(CreateCheckBox(page_advanced, _("Free Look"), (free_look_desc), vconfig.bFreeLook));
#if !defined WIN32 && defined HAVE_LIBAV
	szr_utility->Add(CreateCheckBox(page_advanced, _("Frame Dumps use FFV1"), (use_ffv1_desc), vconfig.bUseFFV1));
#endif

	wxStaticBoxSizer* const group_utility = new wxStaticBoxSizer(wxVERTICAL, page_advanced, _("Utility"));
	szr_advanced->Add(group_utility, 0, wxEXPAND | wxALL, 5);
	group_utility->Add(szr_utility, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	}

	// - misc
	{
	wxGridSizer* const szr_misc = new wxGridSizer(2, 5, 5);

	szr_misc->Add(CreateCheckBox(page_advanced, _("Show Input Display"), (show_input_display_desc), vconfig.bShowInputDisplay));
	szr_misc->Add(CreateCheckBox(page_advanced, _("Crop"), (crop_desc), vconfig.bCrop));

	// Progressive Scan
	{
	progressive_scan_checkbox = new wxCheckBox(page_advanced, wxID_ANY, _("Enable Progressive Scan"));
	RegisterControl(progressive_scan_checkbox, (prog_scan_desc));
	progressive_scan_checkbox->Bind(wxEVT_CHECKBOX, &VideoConfigDiag::Event_ProgressiveScan, this);

	progressive_scan_checkbox->SetValue(SConfig::GetInstance().bProgressive);
	// A bit strange behavior, but this needs to stay in sync with the main progressive boolean; TODO: Is this still necessary?
	SConfig::GetInstance().m_SYSCONF->SetData("IPL.PGS", SConfig::GetInstance().bProgressive);

	szr_misc->Add(progressive_scan_checkbox);
	}
#if defined WIN32
	// Borderless Fullscreen
	borderless_fullscreen = CreateCheckBox(page_advanced, _("Borderless Fullscreen"), (borderless_fullscreen_desc), vconfig.bBorderlessFullscreen);
	szr_misc->Add(borderless_fullscreen);
#endif
	wxStaticBoxSizer* const group_misc = new wxStaticBoxSizer(wxVERTICAL, page_advanced, _("Misc"));
	szr_advanced->Add(group_misc, 0, wxEXPAND | wxALL, 5);
	group_misc->Add(szr_misc, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	}

	szr_advanced->AddStretchSpacer();
	CreateDescriptionArea(page_advanced, szr_advanced);
	page_advanced->SetSizerAndFit(szr_advanced);
	}

	wxButton* const btn_close = new wxButton(this, wxID_OK, _("Close"), wxDefaultPosition);
	btn_close->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &VideoConfigDiag::Event_ClickClose, this);

	Bind(wxEVT_CLOSE_WINDOW, &VideoConfigDiag::Event_Close, this);

	wxBoxSizer* const szr_main = new wxBoxSizer(wxVERTICAL);
	szr_main->Add(notebook, 1, wxEXPAND | wxALL, 5);
	szr_main->Add(btn_close, 0, wxALIGN_RIGHT | wxRIGHT | wxBOTTOM, 5);

	SetSizerAndFit(szr_main);
	Center();
	SetFocus();

	UpdateWindowUI();
}

void VideoConfigDiag::Event_DisplayResolution(wxCommandEvent &ev)
{
	SConfig::GetInstance().strFullscreenResolution =
		WxStrToStr(choice_display_resolution->GetStringSelection());
#if defined(HAVE_XRANDR) && HAVE_XRANDR
	main_frame->m_XRRConfig->Update();
#endif
	ev.Skip();
}

SettingCheckBox* VideoConfigDiag::CreateCheckBox(wxWindow* parent, const wxString& label, const wxString& description, bool &setting, bool reverse, long style)
{
	SettingCheckBox* const cb = new SettingCheckBox(parent, label, wxString(), setting, reverse, style);
	RegisterControl(cb, description);
	return cb;
}

SettingChoice* VideoConfigDiag::CreateChoice(wxWindow* parent, int& setting, const wxString& description, int num, const wxString choices[], long style)
{
	SettingChoice* const ch = new SettingChoice(parent, setting, wxString(), num, choices, style);
	RegisterControl(ch, description);
	return ch;
}

SettingRadioButton* VideoConfigDiag::CreateRadioButton(wxWindow* parent, const wxString& label, const wxString& description, bool &setting, bool reverse, long style)
{
	SettingRadioButton* const rb = new SettingRadioButton(parent, label, wxString(), setting, reverse, style);
	RegisterControl(rb, description);
	return rb;
}

/* Use this to register descriptions for controls which have NOT been created using the Create* functions from above */
wxControl* VideoConfigDiag::RegisterControl(wxControl* const control, const wxString& description)
{
	ctrl_descs.insert(std::pair<wxWindow*,wxString>(control, description));
	control->Bind(wxEVT_ENTER_WINDOW, &VideoConfigDiag::Evt_EnterControl, this);
	control->Bind(wxEVT_LEAVE_WINDOW, &VideoConfigDiag::Evt_LeaveControl, this);
	return control;
}

void VideoConfigDiag::Evt_EnterControl(wxMouseEvent& ev)
{
	// TODO: Re-Fit the sizer if necessary!

	// Get settings control object from event
	wxWindow* ctrl = (wxWindow*)ev.GetEventObject();
	if (!ctrl) return;

	// look up description text object from the control's parent (which is the wxPanel of the current tab)
	wxStaticText* descr_text = desc_texts[ctrl->GetParent()];
	if (!descr_text) return;

	// look up the description of the selected control and assign it to the current description text object's label
	descr_text->SetLabel(ctrl_descs[ctrl]);
	descr_text->Wrap(descr_text->GetContainingSizer()->GetSize().x - 20);

	ev.Skip();
}

// TODO: Don't hardcode the size of the description area via line breaks
#define DEFAULT_DESC_TEXT _("Move the mouse pointer over an option to display a detailed description.\n\n\n\n\n\n\n")
void VideoConfigDiag::Evt_LeaveControl(wxMouseEvent& ev)
{
	// look up description text control and reset its label
	wxWindow* ctrl = (wxWindow*)ev.GetEventObject();
	if (!ctrl) return;
	wxStaticText* descr_text = desc_texts[ctrl->GetParent()];
	if (!descr_text) return;

	descr_text->SetLabel(DEFAULT_DESC_TEXT);
	descr_text->Wrap(descr_text->GetContainingSizer()->GetSize().x - 20);
	ev.Skip();
}

void VideoConfigDiag::CreateDescriptionArea(wxPanel* const page, wxBoxSizer* const sizer)
{
	// Create description frame
	wxStaticBoxSizer* const desc_sizer = new wxStaticBoxSizer(wxVERTICAL, page, _("Description"));
	sizer->Add(desc_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	// Need to call SetSizerAndFit here, since we don't want the description texts to change the dialog width
	page->SetSizerAndFit(sizer);

	// Create description text
	wxStaticText* const desc_text = new wxStaticText(page, wxID_ANY, DEFAULT_DESC_TEXT);
	desc_text->Wrap(desc_sizer->GetSize().x - 20);
	desc_sizer->Add(desc_text, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	// Store description text object for later lookup
	desc_texts.insert(std::pair<wxWindow*,wxStaticText*>(page, desc_text));
}

void VideoConfigDiag::Event_Backend(wxCommandEvent &ev)
{
	VideoBackend* new_backend = g_available_video_backends[ev.GetInt()];
	if (g_video_backend != new_backend)
	{
		bool do_switch = true;
		if (new_backend->GetName() == "Software Renderer")
		{
			do_switch = (wxYES == wxMessageBox(_("Software rendering is an order of magnitude slower than using the other backends.\nIt's only useful for debugging purposes.\nDo you really want to enable software rendering? If unsure, select 'No'."),
				_("Warning"), wxYES_NO | wxNO_DEFAULT | wxICON_EXCLAMATION, wxGetActiveWindow()));
		}

		if (do_switch)
		{
			// TODO: Only reopen the dialog if the software backend is
			// selected (make sure to reinitialize backend info)
			// reopen the dialog
			Close();

			g_video_backend = new_backend;
			SConfig::GetInstance().m_strVideoBackend = g_video_backend->GetName();

			g_video_backend->ShowConfig(GetParent());
		}
		else
		{
			// Select current backend again
			choice_backend->SetStringSelection(StrToWxStr(g_video_backend->GetName()));
		}
	}

	ev.Skip();
}

void VideoConfigDiag::Event_Adapter(wxCommandEvent &ev) { ev.Skip(); } // TODO

void VideoConfigDiag::Event_ProgressiveScan(wxCommandEvent &ev)
{
	SConfig::GetInstance().m_SYSCONF->SetData("IPL.PGS", ev.GetInt());
	SConfig::GetInstance().bProgressive = ev.IsChecked();

	ev.Skip();
}

void VideoConfigDiag::Event_Stc(wxCommandEvent &ev)
{
	int samples[] = { 0, 512, 128 };
	vconfig.iSafeTextureCache_ColorSamples = samples[ev.GetInt()];

	ev.Skip();
}

void VideoConfigDiag::Event_Bbox(wxCommandEvent &ev)
{
	vconfig.iBBoxMode =ev.GetInt();
	text_bboxmode->SetLabel((s_bbox_mode_text[vconfig.iBBoxMode]));
	ev.Skip();
}

void VideoConfigDiag::Event_PPShader(wxCommandEvent &ev)
{
	const int sel = ev.GetInt();
	if (sel)
		vconfig.sPostProcessingShader = WxStrToStr(ev.GetString());
	else
		vconfig.sPostProcessingShader.clear();

	// Should we enable the configuration button?
	PostProcessingShaderConfiguration postprocessing_shader;
	postprocessing_shader.LoadShader(vconfig.sPostProcessingShader);
	button_config_pp->Enable(postprocessing_shader.HasOptions());

	ev.Skip();
}

void VideoConfigDiag::Event_ConfigurePPShader(wxCommandEvent &ev)
{
	PostProcessingConfigDiag dialog(this, vconfig.sPostProcessingShader);
	dialog.ShowModal();

	ev.Skip();
}

void VideoConfigDiag::Event_StereoDepth(wxCommandEvent &ev)
{
	vconfig.iStereoDepth = ev.GetInt();

	ev.Skip();
}

void VideoConfigDiag::Event_ScalingFactor(wxCommandEvent &ev)
{
	const wxString sf_choices[] = { wxT("1x"), wxT("2x"), wxT("3x"), wxT("4x"), wxT("5x") };
	vconfig.iTexScalingFactor = ev.GetInt();
	label_TextureScale->SetLabel(sf_choices[vconfig.iTexScalingFactor - 1]);
	ev.Skip();
}

void VideoConfigDiag::Event_StereoConvergence(wxCommandEvent &ev)
{
	vconfig.iStereoConvergence = ev.GetInt();

	ev.Skip();
}

void VideoConfigDiag::Event_StereoMode(wxCommandEvent &ev)
{
	if (vconfig.backend_info.bSupportsPostProcessing)
	{
		// Anaglyph overrides post-processing shaders
		choice_ppshader->Clear();
	}

	ev.Skip();
}

// Enables/disables UI elements depending on current config
void VideoConfigDiag::OnUpdateUI(wxUpdateUIEvent& ev)
{
	// Anti-aliasing
	choice_aamode->Enable(vconfig.backend_info.AAModes.size() > 1);
	text_aamode->Enable(vconfig.backend_info.AAModes.size() > 1);

	// pixel lighting
	pixel_lighting->Enable(vconfig.backend_info.bSupportsPixelLighting);
#if defined WIN32
	// Borderless Fullscreen
	borderless_fullscreen->Enable((vconfig.backend_info.APIType & API_D3D9) == 0);
	borderless_fullscreen->Show((vconfig.backend_info.APIType & API_D3D9) == 0);
#endif	
	// EFB Access Cache
	Fast_efb_cache->Show(vconfig.bEFBAccessEnable);
	// XFB
	virtual_xfb->Enable(vconfig.bUseXFB);
	real_xfb->Enable(vconfig.bUseXFB);

	// custom textures
	cache_hires_textures->Enable(vconfig.bHiresTextures);
	
	// Repopulating the post-processing shaders can't be done from an event
	if (choice_ppshader && choice_ppshader->IsEmpty())
		PopulatePostProcessingShaders();

	// Predictive Fifo
	Async_Shader_compilation->Show(vconfig.backend_info.APIType != API_OPENGL);
	Predictive_FIFO->Show(vconfig.backend_info.APIType != API_OPENGL);
	Wait_For_Shaders->Show(vconfig.backend_info.APIType != API_OPENGL);
	bool WaitForShaderCompilationenabled = vconfig.bPredictiveFifo && !vconfig.bFullAsyncShaderCompilation;
	vconfig.bWaitForShaderCompilation = vconfig.bWaitForShaderCompilation && WaitForShaderCompilationenabled;
	Wait_For_Shaders->Enable(WaitForShaderCompilationenabled);
	Async_Shader_compilation->Enable(!vconfig.bWaitForShaderCompilation);
	// Things which shouldn't be changed during emulation
	if (Core::IsRunning())
	{
		choice_backend->Disable();
		label_backend->Disable();

		// D3D only
		if (vconfig.backend_info.Adapters.size())
		{
			choice_adapter->Disable();
			label_adapter->Disable();
		}

#ifndef __APPLE__
		// This isn't supported on OS X.

		choice_display_resolution->Disable();
		label_display_resolution->Disable();
#endif

		progressive_scan_checkbox->Disable();
		render_to_main_checkbox->Disable();
		Predictive_FIFO->Disable();
	}
	else
	{
		Predictive_FIFO->Enable(!vconfig.bWaitForShaderCompilation);
	}

	ev.Skip();
}

void VideoConfigDiag::PopulatePostProcessingShaders()
{
	std::vector<std::string> &shaders = (vconfig.iStereoMode == STEREO_ANAGLYPH || vconfig.iStereoMode == STEREO_INTERLACED) ?
		vconfig.backend_info.AnaglyphShaders : vconfig.backend_info.PPShaders;

	if (shaders.empty())
		return;

	choice_ppshader->AppendString(_("(off)"));

	for (const std::string& shader : shaders)
	{
		choice_ppshader->AppendString(StrToWxStr(shader));
	}

	if (!choice_ppshader->SetStringSelection(StrToWxStr(vconfig.sPostProcessingShader)))
	{
		// Invalid shader, reset it to default
		choice_ppshader->Select(0);
		vconfig.sPostProcessingShader.clear();
	}

	if (vconfig.iStereoMode == STEREO_ANAGLYPH)
	{
		vconfig.sPostProcessingShader = "dubois";
		choice_ppshader->SetStringSelection(StrToWxStr(vconfig.sPostProcessingShader));
	}
	else if (vconfig.iStereoMode == STEREO_INTERLACED)
	{
		vconfig.sPostProcessingShader = "interlaced";
		choice_ppshader->SetStringSelection(StrToWxStr(vconfig.sPostProcessingShader));
	}

	// Should the configuration button be loaded by default?
	PostProcessingShaderConfiguration postprocessing_shader;
	postprocessing_shader.LoadShader(vconfig.sPostProcessingShader);
	button_config_pp->Enable(postprocessing_shader.HasOptions());
}
