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

#include "Common/Assert.h"
#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
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
#include "VideoCommon/RenderBase.h"
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
	g_Config.Save(File::GetUserPath(D_CONFIG_IDX) + "GFX.ini");

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
static wxString phong_lighting_desc = _("Use Phong specular model when pixel ligthing is enabled.");
static wxString phong_intensity_desc = _("Controls Global intensity of specular reflection.");
static wxString rim_intensity_desc = _("Controls Intensity of rim effect.");
static wxString rim_power_desc = _("Controls exponent of rim effect.");
static wxString rim_base_desc = _("Controls minimun rim color.");
static wxString bump_desc = _("Enable generation of bumpmaps to improve ligthing effects.");
static wxString bump_strength_desc = _("Controls strength of simulated bumpmaps. Needs to be adjusted per game co achive better quality");
static wxString bump_detail_frequency_desc = _("Controls detail bumpmap frequency. This will change the size of the noise pattern");
static wxString bump_detail_blend_desc = _("Controls the detail bumpmap strength. Detail bump will add noise to existing textures.");
static wxString bump_threshold_desc = _("Controls simulated bumpmap detail detection threshold. Big values can detect more details as bumps but can cause glitches");
static wxString hacked_buffer_upload_desc = _("Uses unsafe operations to speed up vertex streaming in OpenGL. There are no known problems on supported GPUs, but it will cause severe stability and graphical issues otherwise.\n\nIf unsure, leave this unchecked.");
static wxString fast_depth_calc_desc = _("Use a less accurate algorithm to calculate depth values.\nCauses issues in a few games but might give a decent speedup.\n\nIf unsure, leave this checked.");
static wxString force_filtering_desc = _("Force texture filtering even if the emulated game explicitly disabled it.\nImproves texture quality slightly but causes glitches in some games.\n\nIf unsure, leave this unchecked.");
static wxString disable_filtering_desc = _("Disable texture filtering even if the emulated game explicitly enable it.\n\nIf unsure, leave this unchecked.");
static wxString Use_Scaling_filter_desc = _("Use filtering when efb scaled size is larger than the target resolution.");
static wxString borderless_fullscreen_desc = _("Implement fullscreen mode with a borderless window spanning the whole screen instead of using exclusive mode.\nAllows for faster transitions between fullscreen and windowed mode, but increases input latency, makes movement less smooth and slightly decreases performance.\nExclusive mode is required to support Nvidia 3D Vision in the Direct3D backend.\n\nIf unsure, leave this unchecked.");
static wxString internal_res_desc = _("Specifies the resolution used to render at. A high resolution greatly improves visual quality, but also greatly increases GPU load and can cause issues in certain games.\n\"Multiple of 640x528\" will result in a size slightly larger than \"Window Size\" but yield fewer issues. Generally speaking, the lower the internal resolution is, the better your performance will be. Auto (Window Size), 1.5x, and 2.5x may cause issues in some games.\n\nIf unsure, select Native.");
static wxString efb_access_desc = _("Ignore any requests of the CPU to read from or write to the EFB.\nImproves performance in some games, but might disable some gameplay-related features or graphical effects.\n\nIf unsure, leave this unchecked.");
static wxString efb_fast_access_desc = _("Use a fast efb caching method to speed up access. This method is inaccurate but will make games run faster and efb reads and writes will still work.");
static wxString efb_emulate_format_changes_desc = _("Ignore any changes to the EFB format.\nImproves performance in many games without any negative effect. Causes graphical defects in a small number of other games though.\n\nIf unsure, leave this checked.");
static wxString viewport_correction_desc = _("Some games uses viewport values that are not compatible with D3D backends, to solve issues on those games check this.\n\nIf unsure, leave this unchecked.");
static wxString skip_efb_copy_to_ram_desc = _("Stores EFB Copies exclusively on the GPU, bypassing system memory. Causes graphical defects in a small number of games.\n\nEnabled = EFB Copies to Texture\nDisabled = EFB Copies to RAM (and Texture)\n\nIf unsure, leave this checked.");
static wxString stc_desc = _("The safer you adjust this, the less likely the emulator will be missing any texture updates from RAM.\n\nIf unsure, use the rightmost value.");
static wxString bbox_desc = _("Selects wish implementation is used to emulate Bounding Box. By Default GPU will be used if supported.");
static wxString wireframe_desc = _("Render the scene as a wireframe.\n\nIf unsure, leave this unchecked.");
static wxString disable_fog_desc = _("Makes distant objects more visible by removing fog, thus increasing the overall detail.\nDisabling fog will break some games which rely on proper fog emulation.\n\nIf unsure, leave this unchecked.");
static wxString true_color_desc = _("Forces the game to render the RGB color channels in 24-bit, thereby increasing "
	"quality by reducing color banding.\nIt improves performance and causes "
	"few graphical issues.\n\n\nIf unsure, leave this checked.");
static wxString disable_dstalpha_desc = _("Disables emulation of a hardware feature called destination alpha, which is used in many games for various graphical effects.\n\nIf unsure, leave this unchecked.");
static wxString show_fps_desc =
wxTRANSLATE("Show the number of frames rendered per second as a measure of "
	"emulation speed.\n\nIf unsure, leave this unchecked.");
static wxString show_netplay_ping_desc =
wxTRANSLATE("Show the players' maximum Ping while playing on "
	"NetPlay.\n\nIf unsure, leave this unchecked.");
static wxString show_osd_clock_desc =
wxTRANSLATE("Show the current time on the on-screen display.\n\nIf "
	"unsure, leave this unchecked.");
static wxString show_frame_times_desc =
wxTRANSLATE("Show frame times on the on-screen display in addition to FPS,\nThis option only works when polling method is set to \"On SI Read\".\n\nIf "
	"unsure, leave this unchecked.");
static wxString log_render_time_to_file_desc =
wxTRANSLATE("Log the render time of every frame to User/Logs/render_time.txt. Use this "
	"feature when you want to measure the performance of Dolphin.\n\nIf "
	"unsure, leave this unchecked.");
static wxString show_stats_desc =
wxTRANSLATE("Show various rendering statistics.\n\nIf unsure, leave this unchecked.");
static wxString show_netplay_messages_desc =
wxTRANSLATE("When playing on NetPlay, show chat messages, buffer changes and "
	"desync alerts.\n\nIf unsure, leave this unchecked.");
static wxString show_input_display_desc = _("Display the inputs read by the emulator.\n\nIf unsure, leave this unchecked.");
static wxString texfmt_desc = _("Modify textures to show the format they're encoded in. Needs an emulation reset in most cases.\n\nIf unsure, leave this unchecked.");
static wxString xfb_desc = _("Disable any XFB emulation.\nSpeeds up emulation a lot but causes heavy glitches in many games which rely on them (especially homebrew applications).\n\nIf unsure, leave this checked.");
static wxString xfb_virtual_desc = _("Emulate XFBs using GPU texture objects.\nFixes many games which don't work without XFB emulation while not being as slow as real XFB emulation. However, it may still fail for a lot of other games (especially homebrew applications).\n\nIf unsure, leave this checked.");
static wxString xfb_real_desc = _("Emulate XFBs accurately.\nSlows down emulation a lot and prohibits high-resolution rendering but is necessary to emulate a number of games properly.\n\nIf unsure, check virtual XFB emulation instead.");
static wxString dump_textures_desc = _("Dump decoded game textures to User/Dump/Textures/<game_id>/\n\nIf unsure, leave this unchecked.");
static wxString dump_VertexTranslators_desc = _("Dump Vertex translator code to User/Dump/\n\nIf unsure, leave this unchecked.");
static wxString fullAsyncShaderCompilation_desc = _("Make shader compilation proccess fully asynchronous. This can cause glitches but will give a smooth game experience.");
static wxString compute_texture_decoding_desc = _("Decode Textures using compute shaders. Can Increase Performance in some scenarios.");
static wxString Compute_texture_encoding_desc = _("Encode Textures using compute shaders. Can Increase Performance in some scenarios.");
static wxString waitforshadercompilation_desc = _("Wait for shader compilation in the cpu to avoid fifo problems. This option prevents loops in F-Zero, Metroid Prime fifo resets and others.");
static wxString predictiveFifo_desc = _("Generate a secondary fifo to predict resource usage and improve loading time.");
static wxString load_hires_textures_desc = _("Load custom textures from User/Load/Textures/<game_id>/\n\nIf unsure, leave this unchecked.");
static wxString load_hires_material_maps_desc = _("Load custom material maps from User/Load/Textures/<game_id>/\nUsed to Enable Advanced lighting, Requires Pixel Lighting and Hires Textures Enabled\nIf unsure, leave this unchecked.");
static wxString cache_hires_textures_desc = _("Cache custom textures to system RAM on startup.\nThis can require exponentially more RAM but fixes possible stuttering.\n\nIf unsure, leave this unchecked.");
static wxString cache_hires_textures_gpu_desc = _("Cache custom textures to GPU RAM after loading.\nThis can require exponentially more RAM but fixes stuttering the second time the texture is required.\n\nIf unsure, leave this unchecked.");
static wxString dump_efb_desc = _("Dump the contents of EFB copies to User/Dump/Textures/\n\nIf unsure, leave this unchecked.");
static wxString internal_resolution_frame_dumping_desc = _(
	"Create frame dumps and screenshots at the internal resolution of the renderer, rather than "
	"the size of the window it is displayed within. If the aspect ratio is widescreen, the output "
	"image will be scaled horizontally to preserve the vertical resolution.\n\nIf unsure, leave "
	"this unchecked.");
static wxString dump_frames_desc = _("Dump all rendered frames to an AVI file in User/Dump/Frames/\n\nIf unsure, leave this unchecked.");
#if !defined WIN32 && defined HAVE_LIBAV
static wxString use_ffv1_desc = _("Encode frame dumps using the FFV1 codec.\n\nIf unsure, leave this unchecked.");
#endif
static wxString free_look_desc = _("This feature allows you to change the game's camera.\nMove the mouse while holding the right mouse button to pan and while holding the middle button to move.\nHold SHIFT and press one of the WASD keys to move the camera by a certain step distance (SHIFT+0 to move faster and SHIFT+9 to move slower). Press SHIFT+R to reset the camera.\n\nIf unsure, leave this unchecked.");
static wxString shader_precompile_desc = _("If a database of shader for the current game exists, precompile all known shaders to void issues and stutering during gameplay. This option will increase startup time but will improve gaming experience. Warning: with a clean shader cache dx9 can have up to 20 minutes shader compilation time in some games.");
static wxString crop_desc = _("Crop the picture from its native aspect ratio to 4:3 or 16:9.\n\nIf unsure, leave this unchecked.");
static wxString opencl_desc = _("[EXPERIMENTAL]\nAims to speed up emulation by offloading texture decoding to the GPU using the OpenCL framework.\nHowever, right now it's known to cause texture defects in various games. Also it's slower than regular CPU texture decoding in most cases.\n\nIf unsure, leave this unchecked.");
static wxString pptrigger_desc = _("Determines when to apply post-processing.\nOn Swap will apply post-processing before presenting to the screen. On Projection applies post-processing before the game draws 2D elements on the screen. However, this may not work with all games. On EFB Copy applies post-processing when an EFB copy of a perspective scene is requested. This may work for for other games. After blit will apply post processing after bliting reducing gpu usage when using High efb scales.\n\nIf unsure, select On Swap.");
static wxString ppshader_list_desc = _("Applies post-processing effects when the trigger chosen in the occurs, by default this is at the end of a frame.\n\nPost-processing is performed at the selected internal resolution.\n\nIf unsure, leave the list empty.");
static wxString ppshader_options_desc = _("Some effects offer user-tweakable options. This will open a dialog where you can change the values of these options.");
static wxString scalingshader_desc = _("Use a custom shader for resizing from internal resolution to display resolution. This shader can also perform additional post-processing effects.\n\nIf unsure, select (default).");
static wxString scalingshader_options_desc = _("Some filters offer user-tweakable options. This will open a dialog where you can change the values of these options.");
static wxString shader_errors_desc = _("Usually if shader compilation fails, an error message is displayed.\nHowever, one may skip the popups to allow interruption free gameplay by checking this option.\n\nIf unsure, leave this unchecked.");
static wxString stereo_3d_desc = _("Select the stereoscopic 3D  mode, stereoscopy allows you to get a better feeling of depth if you have the necessary hardware.\nSide-by-Side and Top-and-Bottom are used by most 3D TVs.\nAnaglyph is used for Red-Cyan colored glasses.\nHeavily decreases emulation speed and sometimes causes issues.\n\nIf unsure, select Off.");
static wxString stereo_separation_desc = _("Control the separation distance, this is the distance between the virtual cameras.\nA higher value creates a stronger feeling of depth while a lower value is more comfortable.");
static wxString stereo_convergence_desc = _("Control the convergence distance, this controls the apparant distance of virtual objects.\nA higher value creates stronger out-of-screen effects while a lower value is more comfortable.");
static wxString stereo_swap_desc = _("Swap the left and right eye, mostly useful if you want to view side-by-side cross-eyed.\n\nIf unsure, leave this unchecked.");
static const char *s_bbox_mode_text[] = { "Disabled", "CPU", "GPU" };
static wxString texture_scaling_desc = _("Apply the selected scaling algorithm to improve texture quality.");
static wxString Tessellation_desc = _("Apply the selected Tessellation levels to increase geometry detail.");
static wxString Tessellation_forced_lights_desc = _("Force Lighting in all 3d elements.");
static wxString Tessellation_early_culling_desc = _("Remove surfaces outside the viewport before Tessellation to increase performace. Can cause glitches if the camera is near a surface.");
static wxString Tessellation_distance_desc = _("Decay of Tessellation level in the distance. High values reduce tesselation amounts depending on the distance to the camera.");
static wxString Tessellation_max_desc = _("Maximum Tessellation level applied. The real tessellation level will depend on the size in pixels of the triangle and will be at most the value selected here.");
static wxString Tessellation_round_desc = _("Select the intensity of the rounding filter. Phong Smoothing is used but can cause holes and cracks in geometry with divergent normals.");
static wxString Tessellation_displacement_desc = _("Select the intensity of the displacement effect when using custom materials.");
static wxString scaling_factor_desc = _("Multiplier applied to the texture size.");
static wxString texture_deposterize_desc = _("Decrease some gradient's artifacts caused by scaling.");
static wxString stereoshader_desc = _("Selects which shader will be used to transform the two images when stereoscopy is enabled.");
static wxString forcedLogivOp_desc = _("Force Logic blending support.\nBy default dx11/12 supports logic op blending only on UINT formats, but in some drivers UNORM is also supported but is not detectable.\nThis option will allow you to test if your driver really supports logic blending, but it will crash the emulator if enabled in a platform that does not support it.\n\nIf unsure, leave this unchecked.");
static wxString backend_multithreading_desc =
_("Enables multi-threading in the video backend, which may result in performance "
	"gains in some scenarios.\n\nIf unsure, leave this unchecked.");
static wxString validation_layer_desc =
wxTRANSLATE("Enables validation of API calls made by the video backend, which may assist in "
	"debugging graphical issues.\n\nIf unsure, leave this unchecked.");

static wxString vertex_rounding_desc =
wxTRANSLATE("Round 2D vertices to whole pixels.  Fixes some "
	"games at higher internal resolutions.  This setting is disabled and turned off "
	"at 1x IR.\n\nIf unsure, leave this unchecked.");

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

VideoConfigDiag::VideoConfigDiag(wxWindow* parent, const std::string &title)
	: wxDialog(parent, wxID_ANY,
		wxString::Format(_("Dolphin %s Graphics Configuration"), StrToWxStr(title)),
		wxDefaultPosition, wxDefaultSize)
	, vconfig(g_Config)
{
	//if (File::Exists(File::GetUserPath(D_CONFIG_IDX) + "GFX.ini"))
	vconfig.Load(File::GetUserPath(D_CONFIG_IDX) + "GFX.ini");

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

				for (auto& backend : g_available_video_backends)
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
					const wxString ar_choices[] = { _("Auto"), _("Force Analog 16:9"), _("Force Analog 4:3"), _("Stretch to Window"), _("Force 4:3"), _("Force 73:60 (Melee)"), _("Force 16:9"), _("Force 16:10"), _("Integer scaling") };

					szr_display->Add(new wxStaticText(page_general, wxID_ANY, _("Aspect Ratio:")), 1, wxALIGN_CENTER_VERTICAL, 0);
					wxChoice* const choice_aspect = CreateChoice(page_general, vconfig.iAspectRatio, (ar_desc),
						sizeof(ar_choices) / sizeof(*ar_choices), ar_choices);
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
				szr_other->Add(CreateCheckBox(page_general, _("Show NetPlay Ping"),
					wxGetTranslation(show_netplay_ping_desc),
					vconfig.bShowNetPlayPing));
                szr_other->Add(CreateCheckBox(page_general, _("Show OSD Clock"),
					wxGetTranslation(show_osd_clock_desc),
					vconfig.bShowOSDClock));
                szr_other->Add(CreateCheckBox(page_general, _("Show frame times on FPS display"),
					wxGetTranslation(show_frame_times_desc),
					vconfig.bShowFrameTimes));
				szr_other->Add(CreateCheckBox(page_general, _("Auto Adjust Window Size"), (auto_window_size_desc), SConfig::GetInstance().bRenderWindowAutoSize));
				szr_other->Add(CreateCheckBox(page_general, _("Show NetPlay Messages"),
					wxGetTranslation(show_netplay_messages_desc),
					vconfig.bShowNetPlayMessages));
				szr_other->Add(CreateCheckBox(page_general, _("Keep window on top"), (keep_window_on_top_desc), SConfig::GetInstance().bKeepWindowOnTop));
				szr_other->Add(CreateCheckBox(page_general, _("Hide Mouse Cursor"), (hide_mouse_cursor_desc), SConfig::GetInstance().bHideCursor));
				szr_other->Add(render_to_main_checkbox = CreateCheckBox(page_general, _("Render to Main Window"), (render_to_main_win_desc), SConfig::GetInstance().bRenderToMain));
				if (vconfig.backend_info.bSupportsMultithreading)
				{
					szr_other->Add(backend_multithreading = CreateCheckBox(page_general, _("Enable Multi-threading"),
						wxGetTranslation(backend_multithreading_desc),
						vconfig.bBackendMultithreading));
				}
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
			choice_aamode = new wxChoice(page_enh, wxID_ANY);
			PopulateAAList();
			choice_aamode->Bind(wxEVT_CHOICE, &VideoConfigDiag::OnAAChanged, this);
			szr_enh->Add(text_aamode, 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_enh->Add(choice_aamode);
			szr_enh->AddSpacer(0);
		}

		// AF
		{
			const wxString af_choices[] = { wxT("1x"), wxT("2x"), wxT("4x"), wxT("8x"), wxT("16x") };
			szr_enh->Add(new wxStaticText(page_enh, wxID_ANY, _("Anisotropic Filtering:")), 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_enh->Add(CreateChoice(page_enh, vconfig.iMaxAnisotropy, (af_desc), 5, af_choices));
			szr_enh->AddSpacer(0);
		}

		// Scaled copy, PL, Bilinear filter, 3D Vision
		szr_enh->Add(CreateCheckBox(page_enh, _("Scaled EFB Copy"), (scaled_efb_copy_desc), vconfig.bCopyEFBScaled));
		if (vconfig.backend_info.bSupportsScaling)
		{
			szr_enh->Add(CreateCheckBox(page_enh, _("Use Scaling Filter"), (Use_Scaling_filter_desc), vconfig.bUseScalingFilter));
		}
		szr_enh->Add(CreateCheckBox(page_enh, _("Force Texture Filtering"), (force_filtering_desc), vconfig.bForceFiltering));
		szr_enh->Add(CreateCheckBox(page_enh, _("Disable Texture Filtering"), (disable_filtering_desc), vconfig.bDisableTextureFiltering));

		szr_enh->Add(CreateCheckBox(page_enh, _("Widescreen Hack"), (ws_hack_desc), vconfig.bWidescreenHack));
		szr_enh->Add(CreateCheckBox(page_enh, _("Disable Fog"), (disable_fog_desc), vconfig.bDisableFog));
		szr_enh->Add(CreateCheckBox(page_enh, _("Force 24-bit Color"), (true_color_desc), vconfig.bForceTrueColor));
		szr_enh->Add(pixel_lighting = CreateCheckBox(page_enh, _("Per-Pixel Lighting"), (pixel_lighting_desc), vconfig.bEnablePixelLighting));
		szr_enh->Add(phong_lighting = CreateCheckBox(page_enh, _("Phong Lighting"), (phong_lighting_desc), vconfig.bForcePhongShading));
		szr_enh->Add(sim_bump = CreateCheckBox(page_enh, _("Auto Bumps"), (bump_desc), vconfig.bSimBumpEnabled));
		wxStaticBoxSizer* const group_enh = new wxStaticBoxSizer(wxVERTICAL, page_enh, _("Enhancements"));
		group_enh->Add(szr_enh, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
		szr_enh_main->Add(group_enh, 0, wxEXPAND | wxALL, 5);
		// Feature disabled in Slippi: Texture scaling causes crashes on Pokémon Stadium
#if ISHIIRUKA_ALLOW_TEXTURE_SCALING
		{
			wxFlexGridSizer* const szr_texturescaling = new wxFlexGridSizer(3, 5, 5);
			szr_texturescaling->AddGrowableCol(1, 1);

			szr_texturescaling->Add(new wxStaticText(page_enh, wxID_ANY, _("Texture Scaling Mode:")), 1, wxALIGN_CENTER_VERTICAL, 0);
			const wxString scaling_choices[] = { "Off", "XBRZ", "Hybrid", "Bicubic", "Hybrid-Bicubic", "Jinc", "Jinc-Sharper", "Smoothstep", "3-Point", "DDT", "DDT-Sharp" };
			wxChoice* scaling_choice = CreateChoice(page_enh, vconfig.iTexScalingType, (texture_scaling_desc), ArraySize(scaling_choices), scaling_choices);
			szr_texturescaling->Add(scaling_choice, 1, wxEXPAND | wxRIGHT);
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

			szr_texturescaling->Add(new wxStaticText(page_enh, wxID_ANY, _("Scaling factor:")), 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_texturescaling->Add(factor_slider, 1, wxEXPAND | wxRIGHT, 0);
			const wxString sf_choices[] = { wxT("1x"), wxT("2x"), wxT("3x"), wxT("4x"), wxT("5x") };
			szr_texturescaling->Add(label_TextureScale = new wxStaticText(page_enh, wxID_ANY, sf_choices[vconfig.iTexScalingFactor - 1]), 1, wxRIGHT | wxTOP | wxBOTTOM, 5);

			wxStaticBoxSizer* const group_scaling = new wxStaticBoxSizer(wxVERTICAL, page_enh, _("Texture Scaling"));
			group_scaling->Add(szr_texturescaling, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
			szr_enh_main->Add(group_scaling, 0, wxEXPAND | wxALL, 5);
		}
#endif
		{
			wxFlexGridSizer* const szr_phong = new wxFlexGridSizer(4, 5, 5);
			szr_phong->AddGrowableCol(1, 1);
			szr_phong->AddGrowableCol(3, 1);
			{
				wxSlider* const pintensity_slider = new wxSlider(page_enh, wxID_ANY, vconfig.iSpecularMultiplier, 0, 510, wxDefaultPosition, wxDefaultSize);
				pintensity_slider->Bind(wxEVT_SLIDER, &VideoConfigDiag::Event_SpecularIntensity, this);
				RegisterControl(pintensity_slider, (phong_intensity_desc));

				szr_phong->Add(new wxStaticText(page_enh, wxID_ANY, _("Specular Intensity:")), 1, wxALIGN_CENTER_VERTICAL, 0);
				szr_phong->Add(pintensity_slider, 1, wxEXPAND | wxRIGHT);
			}
			{
				wxSlider* const rimintensity_slider = new wxSlider(page_enh, wxID_ANY, vconfig.iRimIntesity, 0, 255, wxDefaultPosition, wxDefaultSize);
				rimintensity_slider->Bind(wxEVT_SLIDER, &VideoConfigDiag::Event_RimIntensity, this);
				RegisterControl(rimintensity_slider, (rim_intensity_desc));

				szr_phong->Add(new wxStaticText(page_enh, wxID_ANY, _("Rim Intensity:")), 1, wxALIGN_CENTER_VERTICAL, 0);
				szr_phong->Add(rimintensity_slider, 1, wxEXPAND | wxRIGHT);
			}
			{
				wxSlider* const rimpower_slider = new wxSlider(page_enh, wxID_ANY, vconfig.iRimPower, 0, 255, wxDefaultPosition, wxDefaultSize);
				rimpower_slider->Bind(wxEVT_SLIDER, &VideoConfigDiag::Event_RimPower, this);
				RegisterControl(rimpower_slider, (rim_intensity_desc));

				szr_phong->Add(new wxStaticText(page_enh, wxID_ANY, _("Rim Exponent:")), 1, wxALIGN_CENTER_VERTICAL, 0);
				szr_phong->Add(rimpower_slider, 1, wxEXPAND | wxRIGHT);
			}
			{
				wxSlider* const rimbase_slider = new wxSlider(page_enh, wxID_ANY, vconfig.iRimBase, 0, 127, wxDefaultPosition, wxDefaultSize);
				rimbase_slider->Bind(wxEVT_SLIDER, &VideoConfigDiag::Event_RimBase, this);
				RegisterControl(rimbase_slider, (rim_base_desc));

				szr_phong->Add(new wxStaticText(page_enh, wxID_ANY, _("Rim Base:")), 1, wxALIGN_CENTER_VERTICAL, 0);
				szr_phong->Add(rimbase_slider, 1, wxEXPAND | wxRIGHT);
			}
			{
				bump_strenght_slider = new wxSlider(page_enh, wxID_ANY, vconfig.iSimBumpStrength, 0, 1023, wxDefaultPosition, wxDefaultSize);
				bump_strenght_slider->Bind(wxEVT_SLIDER, &VideoConfigDiag::Event_BumpStrength, this);
				RegisterControl(bump_strenght_slider, (bump_strength_desc));

				szr_phong->Add(new wxStaticText(page_enh, wxID_ANY, _("Bump Strength:")), 1, wxALIGN_CENTER_VERTICAL, 0);
				szr_phong->Add(bump_strenght_slider, 1, wxEXPAND | wxRIGHT);
			}
			{
				bump_threshold_slider = new wxSlider(page_enh, wxID_ANY, vconfig.iSimBumpThreshold, 0, 255, wxDefaultPosition, wxDefaultSize);
				bump_threshold_slider->Bind(wxEVT_SLIDER, &VideoConfigDiag::Event_BumpThreshold, this);
				RegisterControl(bump_threshold_slider, (bump_threshold_desc));

				szr_phong->Add(new wxStaticText(page_enh, wxID_ANY, _("Bump Threshold:")), 1, wxALIGN_CENTER_VERTICAL, 0);
				szr_phong->Add(bump_threshold_slider, 1, wxEXPAND | wxRIGHT);
			}
			{
				bump_blend_slider = new wxSlider(page_enh, wxID_ANY, vconfig.iSimBumpDetailBlend, 0, 255, wxDefaultPosition, wxDefaultSize);
				bump_blend_slider->Bind(wxEVT_SLIDER, &VideoConfigDiag::Event_BumpDetailBlend, this);
				RegisterControl(bump_blend_slider, (bump_detail_blend_desc));

				szr_phong->Add(new wxStaticText(page_enh, wxID_ANY, _("Detail Strength:")), 1, wxALIGN_CENTER_VERTICAL, 0);
				szr_phong->Add(bump_blend_slider, 1, wxEXPAND | wxRIGHT);
			}
			{
				bump_frequency_slider = new wxSlider(page_enh, wxID_ANY, vconfig.iSimBumpDetailFrequency, 4, 255, wxDefaultPosition, wxDefaultSize);
				bump_frequency_slider->Bind(wxEVT_SLIDER, &VideoConfigDiag::Event_BumpDetailFrequency, this);
				RegisterControl(bump_frequency_slider, (bump_detail_frequency_desc));

				szr_phong->Add(new wxStaticText(page_enh, wxID_ANY, _("Detail Frequency:")), 1, wxALIGN_CENTER_VERTICAL, 0);
				szr_phong->Add(bump_frequency_slider, 1, wxEXPAND | wxRIGHT);
			}


			group_phong = new wxStaticBoxSizer(wxVERTICAL, page_enh, _("Light Parameters"));
			group_phong->Add(szr_phong, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
			szr_enh_main->Add(group_phong, 0, wxEXPAND | wxALL, 5);
		}
		szr_enh_main->AddStretchSpacer();
		CreateDescriptionArea(page_enh, szr_enh_main);
		page_enh->SetSizerAndFit(szr_enh_main);
	}

	// -- ENHANCEMENTS 2 --
	if (vconfig.backend_info.bSupportsGeometryShaders || vconfig.backend_info.bSupportsTessellation)
	{
		wxPanel* const page_enh = new wxPanel(notebook);
		notebook->AddPage(page_enh, _("Enhancements"));
		wxBoxSizer* const szr_enh_main = new wxBoxSizer(wxVERTICAL);
		if (vconfig.backend_info.bSupportsGeometryShaders)
		{
			wxFlexGridSizer* const szr_stereo = new wxFlexGridSizer(3, 5, 5);

			szr_stereo->Add(new wxStaticText(page_enh, wxID_ANY, _("Stereoscopic 3D Mode:")), 1, wxALIGN_CENTER_VERTICAL, 0);

			const wxString stereo_choices[] = { "Off", "Side-by-Side", "Top-and-Bottom", "Shader", "Nvidia 3D Vision" };
			wxChoice* stereo_choice = CreateChoice(page_enh, vconfig.iStereoMode, (stereo_3d_desc), vconfig.backend_info.bSupports3DVision ? ArraySize(stereo_choices) : ArraySize(stereo_choices) - 1, stereo_choices);
			stereo_choice->Bind(wxEVT_CHOICE, &VideoConfigDiag::Event_StereoMode, this);
			szr_stereo->Add(stereo_choice, 0, wxEXPAND);

			choice_stereoshader = new wxChoice(page_enh, wxID_ANY);
			RegisterControl(choice_stereoshader, wxGetTranslation(stereoshader_desc));
			choice_stereoshader->Bind(wxEVT_CHOICE, &VideoConfigDiag::Event_StereoShader, this);
			szr_stereo->AddSpacer(0);
			szr_stereo->Add(new wxStaticText(page_enh, wxID_ANY, _("Stereoscopy Shader:")), 0, wxALIGN_CENTER_VERTICAL, 0);
			szr_stereo->Add(choice_stereoshader, 0, wxEXPAND);
			PopulateStereoShaders();

			szr_stereo->Add(CreateCheckBox(page_enh, _("Swap Eyes"), (stereo_swap_desc), vconfig.bStereoSwapEyes), 1, wxALIGN_CENTER_VERTICAL, 0);

			wxSlider* const sep_slider = new wxSlider(page_enh, wxID_ANY, vconfig.iStereoDepth, 0, 100, wxDefaultPosition, wxDefaultSize);
			sep_slider->Bind(wxEVT_SLIDER, &VideoConfigDiag::Event_StereoDepth, this);
			RegisterControl(sep_slider, (stereo_separation_desc));

			szr_stereo->Add(new wxStaticText(page_enh, wxID_ANY, _("Separation:")), 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_stereo->Add(sep_slider, 1, wxEXPAND | wxRIGHT);
			szr_stereo->AddSpacer(0);
			conv_slider = new wxSlider(page_enh, wxID_ANY, vconfig.iStereoConvergencePercentage, 0, 200, wxDefaultPosition, wxDefaultSize, wxSL_AUTOTICKS);
			conv_slider->ClearTicks();
			conv_slider->SetTick(100);
			conv_slider->Bind(wxEVT_SLIDER, &VideoConfigDiag::Event_StereoConvergence, this);
			RegisterControl(conv_slider, (stereo_convergence_desc));

			szr_stereo->Add(new wxStaticText(page_enh, wxID_ANY, _("Convergence:")), 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_stereo->Add(conv_slider, 1, wxEXPAND | wxRIGHT);

			wxStaticBoxSizer* const group_stereo = new wxStaticBoxSizer(wxVERTICAL, page_enh, _("Stereoscopy"));
			group_stereo->Add(szr_stereo, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
			szr_enh_main->Add(group_stereo, 0, wxEXPAND | wxALL, 5);
		}
		if (vconfig.backend_info.bSupportsTessellation)
		{
			wxFlexGridSizer* const szr_tessellation = new wxFlexGridSizer(3, 5, 5);
			szr_tessellation->AddGrowableCol(2, 1);
			szr_tessellation->Add(CreateCheckBox(page_enh, _("Enable"), (Tessellation_desc), vconfig.bTessellation), 1, wxALIGN_CENTER_VERTICAL, 0);

			wxSlider* const min_slider = new wxSlider(page_enh, wxID_ANY, vconfig.iTessellationDistance, 5, 1000, wxDefaultPosition, wxDefaultSize);
			min_slider->Bind(wxEVT_SLIDER, &VideoConfigDiag::Event_TessellationDistance, this);
			RegisterControl(min_slider, (Tessellation_distance_desc));

			szr_tessellation->Add(new wxStaticText(page_enh, wxID_ANY, _("Distance Decay:")), 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_tessellation->Add(min_slider, 1, wxEXPAND | wxRIGHT);

			szr_tessellation->Add(CreateCheckBox(page_enh, _("Early Culling"), (Tessellation_early_culling_desc), vconfig.bTessellationEarlyCulling), 1, wxALIGN_CENTER_VERTICAL, 0);

			wxSlider* const max_slider = new wxSlider(page_enh, wxID_ANY, vconfig.iTessellationMax, 2, 63, wxDefaultPosition, wxDefaultSize);
			max_slider->Bind(wxEVT_SLIDER, &VideoConfigDiag::Event_TessellationMax, this);
			RegisterControl(max_slider, (Tessellation_max_desc));

			szr_tessellation->Add(new wxStaticText(page_enh, wxID_ANY, _("Maximun Detail:")), 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_tessellation->Add(max_slider, 1, wxEXPAND | wxRIGHT);

			szr_tessellation->Add(CreateCheckBox(page_enh, _("Forced Lighting"), (Tessellation_forced_lights_desc), vconfig.bForcedLighting), 1, wxALIGN_CENTER_VERTICAL, 0);

			wxSlider* const round_slider = new wxSlider(page_enh, wxID_ANY, vconfig.iTessellationRoundingIntensity, 0, 100, wxDefaultPosition, wxDefaultSize);
			round_slider->Bind(wxEVT_SLIDER, &VideoConfigDiag::Event_TessellationRounding, this);
			RegisterControl(round_slider, (Tessellation_round_desc));

			szr_tessellation->Add(new wxStaticText(page_enh, wxID_ANY, _("Rounding Intensity:")), 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_tessellation->Add(round_slider, 1, wxEXPAND | wxRIGHT);

			szr_tessellation->AddSpacer(0);

			wxSlider* const displacement_slider = new wxSlider(page_enh, wxID_ANY, vconfig.iTessellationDisplacementIntensity, 0, 150, wxDefaultPosition, wxDefaultSize);
			displacement_slider->Bind(wxEVT_SLIDER, &VideoConfigDiag::Event_TessellationDisplacement, this);
			RegisterControl(displacement_slider, (Tessellation_displacement_desc));

			szr_tessellation->Add(new wxStaticText(page_enh, wxID_ANY, _("Displacement Intensity:")), 1, wxALIGN_CENTER_VERTICAL, 0);
			szr_tessellation->Add(displacement_slider, 1, wxEXPAND | wxRIGHT);

			group_Tessellation = new wxStaticBoxSizer(wxVERTICAL, page_enh, _("Tessellation"));
			group_Tessellation->Add(szr_tessellation, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
			szr_enh_main->Add(group_Tessellation, 0, wxEXPAND | wxALL, 5);
		}
		szr_enh_main->AddStretchSpacer();
		CreateDescriptionArea(page_enh, szr_enh_main);
		page_enh->SetSizerAndFit(szr_enh_main);
	}

	// -- POSTPROCESSING --
	if (vconfig.backend_info.bSupportsPostProcessing)
	{
		wxPanel* const page_postprocessing = new wxPanel(notebook);
		notebook->AddPage(page_postprocessing, _("Post-Processing"));

		wxBoxSizer* const szr_postprocessing = new wxBoxSizer(wxVERTICAL);

		// Selected Shaders
		{
			wxBoxSizer* const szr_selected_shaders = new wxBoxSizer(wxVERTICAL);

			// List box
			wxBoxSizer* const szr_pp_shader_list = new wxBoxSizer(wxHORIZONTAL);
			listbox_selected_ppshaders = new wxListBox(page_postprocessing, wxID_ANY);
			listbox_selected_ppshaders->Bind(wxEVT_LISTBOX, &VideoConfigDiag::Event_PPShaderList, this);
			listbox_selected_ppshaders->Bind(wxEVT_LISTBOX_DCLICK, &VideoConfigDiag::Event_PPShaderListOptions, this);
			szr_pp_shader_list->Add(listbox_selected_ppshaders, 1, wxEXPAND | wxALIGN_TOP);
			RegisterControl(listbox_selected_ppshaders, wxGetTranslation(ppshader_list_desc));

			// List manipulation buttons
			wxBoxSizer* const szr_pp_shader_list_buttons = new wxBoxSizer(wxVERTICAL);
			button_move_ppshader_up = new wxButton(page_postprocessing, wxID_ANY, _("Move &Up"));
			button_move_ppshader_up->Bind(wxEVT_BUTTON, &VideoConfigDiag::Event_PPShaderListMoveUp, this);
			szr_pp_shader_list_buttons->Add(button_move_ppshader_up);
			button_move_ppshader_down = new wxButton(page_postprocessing, wxID_ANY, _("Move &Down"));
			button_move_ppshader_down->Bind(wxEVT_BUTTON, &VideoConfigDiag::Event_PPShaderListMoveDown, this);
			szr_pp_shader_list_buttons->Add(button_move_ppshader_down);
			button_config_ppshader = new wxButton(page_postprocessing, wxID_ANY, _("&Options..."));
			button_config_ppshader->Bind(wxEVT_BUTTON, &VideoConfigDiag::Event_PPShaderListOptions, this);
			RegisterControl(button_config_ppshader, wxGetTranslation(ppshader_options_desc));
			szr_pp_shader_list_buttons->Add(button_config_ppshader);
			button_remove_ppshader = new wxButton(page_postprocessing, wxID_ANY, _("&Remove"));
			button_remove_ppshader->Bind(wxEVT_BUTTON, &VideoConfigDiag::Event_PPShaderListRemove, this);
			szr_pp_shader_list_buttons->Add(button_remove_ppshader);
			szr_pp_shader_list->Add(szr_pp_shader_list_buttons, 0, wxLEFT | wxALIGN_TOP, 5);

			szr_selected_shaders->Add(szr_pp_shader_list, 1, wxEXPAND | wxBOTTOM, 5);

			// Add dropdown and button
			wxBoxSizer* const szr_pp_add_shader = new wxBoxSizer(wxHORIZONTAL);
			szr_pp_add_shader->Add(new wxStaticText(page_postprocessing, wxID_ANY, _("Add Shader:")), 0, wxALIGN_CENTER_VERTICAL, 0);
			choice_ppshader = new wxChoice(page_postprocessing, wxID_ANY);
			szr_pp_add_shader->Add(choice_ppshader, 1, wxLEFT | wxALIGN_CENTER_VERTICAL, 5);
			button_add_ppshader = new wxButton(page_postprocessing, wxID_ANY, _("&Add"));
			button_add_ppshader->Bind(wxEVT_BUTTON, &VideoConfigDiag::Event_PPShaderAdd, this);
			szr_pp_add_shader->Add(button_add_ppshader, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, 5);
			szr_selected_shaders->Add(szr_pp_add_shader, 0, wxEXPAND);

			// Fill data from config
			PopulatePostProcessingShaders();
			UpdatePostProcessingShaderListButtons();

			wxStaticBoxSizer* const group_shader_list = new wxStaticBoxSizer(wxVERTICAL, page_postprocessing, _T("Selected Shaders"));
			group_shader_list->Add(szr_selected_shaders, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
			szr_postprocessing->Add(group_shader_list, 0, wxEXPAND | wxALL, 5);
		}

		// Options
		{
			wxFlexGridSizer* szr_options = new wxFlexGridSizer(2, 5, 5);
			szr_options->AddGrowableCol(1, 1);

			// Trigger
			const wxString pptrigger_choices[] = { _("On Swap"), _("On Projection"), _("On EFB Copy"), _("After Blit") };
			choice_pptrigger = CreateChoice(page_postprocessing, vconfig.iPostProcessingTrigger, wxGetTranslation(pptrigger_desc), 4, pptrigger_choices);
			szr_options->Add(new wxStaticText(page_postprocessing, wxID_ANY, _("Post-Processing Trigger:")), 0, wxALIGN_CENTER_VERTICAL, 0);
			szr_options->Add(choice_pptrigger, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);

			choice_scalingshader = new wxChoice(page_postprocessing, wxID_ANY);
			choice_scalingshader->Bind(wxEVT_CHOICE, &VideoConfigDiag::Event_ScalingShader, this);
			RegisterControl(choice_scalingshader, wxGetTranslation(scalingshader_desc));
			szr_options->Add(new wxStaticText(page_postprocessing, wxID_ANY, _("Display/Resize Shader:")), 0, wxALIGN_CENTER_VERTICAL, 0);
			szr_options->Add(choice_scalingshader, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);

			button_config_scalingshader = new wxButton(page_postprocessing, wxID_ANY, _("Display Shader Options..."));
			button_config_scalingshader->Bind(wxEVT_BUTTON, &VideoConfigDiag::Event_ConfigureScalingShader, this);
			RegisterControl(button_config_scalingshader, wxGetTranslation(scalingshader_options_desc));
			szr_options->AddSpacer(1);
			szr_options->Add(button_config_scalingshader);

			PopulateScalingShaders();

			wxStaticBoxSizer* const group_options = new wxStaticBoxSizer(wxVERTICAL, page_postprocessing, _("Options"));
			group_options->Add(szr_options, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
			szr_postprocessing->Add(group_options, 0, wxEXPAND | wxALL, 5);
		}

		szr_postprocessing->AddStretchSpacer();
		CreateDescriptionArea(page_postprocessing, szr_postprocessing);
		page_postprocessing->SetSizerAndFit(szr_postprocessing);
	}
	else
	{
		listbox_selected_ppshaders = nullptr;
		button_move_ppshader_up = nullptr;
		button_remove_ppshader = nullptr;
		button_config_ppshader = nullptr;
		button_remove_ppshader = nullptr;
		choice_ppshader = nullptr;
		button_add_ppshader = nullptr;
		choice_pptrigger = nullptr;
		choice_scalingshader = nullptr;
		button_config_scalingshader = nullptr;
		choice_stereoshader = nullptr;
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
			wxSlider* const stc_slider = new wxSlider(page_hacks, wxID_ANY, 0, 0, 2, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_BOTTOM);
			stc_slider->Bind(wxEVT_COMMAND_SLIDER_UPDATED, &VideoConfigDiag::Event_Stc, this);
			RegisterControl(stc_slider, (stc_desc));

			if (vconfig.iSafeTextureCache_ColorSamples == 0) stc_slider->SetValue(0);
			else if (vconfig.iSafeTextureCache_ColorSamples == 512) stc_slider->SetValue(1);
			else if (vconfig.iSafeTextureCache_ColorSamples == 128) stc_slider->SetValue(2);
			else stc_slider->Disable(); // Using custom number of samples; TODO: Inform the user why this is disabled..

			szr_safetex->Add(new wxStaticText(page_hacks, wxID_ANY, _("Accuracy:")), 0, wxALL, 5);
			szr_safetex->AddStretchSpacer(1);
			szr_safetex->Add(new wxStaticText(page_hacks, wxID_ANY, _("Safe")), 0, wxLEFT | wxTOP | wxBOTTOM, 5);
			szr_safetex->Add(stc_slider, 2, wxRIGHT, 0);
			szr_safetex->Add(new wxStaticText(page_hacks, wxID_ANY, _("Fast")), 0, wxRIGHT | wxTOP | wxBOTTOM, 5);
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
			// Disable while i fix opencl
			//szr_other->Add(CreateCheckBox(page_hacks, _("OpenCL Texture Decoder"), (opencl_desc), vconfig.bEnableOpenCL));
			szr_other->Add(CreateCheckBox(page_hacks, _("Fast Depth Calculation"), (fast_depth_calc_desc), vconfig.bFastDepthCalc));
			vertex_rounding_checkbox =
				CreateCheckBox(page_hacks, _("Vertex Rounding"), wxGetTranslation(vertex_rounding_desc),
					vconfig.bVertexRounding);
			szr_other->Add(vertex_rounding_checkbox);
			szr_other->Add(Forced_LogicOp = CreateCheckBox(page_hacks, _("Force Logic Blending"), (forcedLogivOp_desc), vconfig.bForceLogicOpBlend));
			//szr_other->Add(Predictive_FIFO = CreateCheckBox(page_hacks, _("Predictive FIFO"), (predictiveFifo_desc), vconfig.bPredictiveFifo));
			//szr_other->Add(Wait_For_Shaders = CreateCheckBox(page_hacks, _("Wait for Shader Compilation"), (waitforshadercompilation_desc), vconfig.bWaitForShaderCompilation));
			szr_other->Add(Async_Shader_compilation = CreateCheckBox(page_hacks, _("Full Async Shader Compilation"), (fullAsyncShaderCompilation_desc), vconfig.bFullAsyncShaderCompilation));
			szr_other->Add(GPU_Texture_decoding = CreateCheckBox(page_hacks, _("GPU Texture Decoding"), (compute_texture_decoding_desc), vconfig.bEnableGPUTextureDecoding));
			szr_other->Add(Compute_Shader_encoding = CreateCheckBox(page_hacks, _("Compute Texture Encoding"), (Compute_texture_encoding_desc), vconfig.bEnableComputeTextureEncoding));

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
			szr_debug->Add(CreateCheckBox(page_advanced, _("Show Statistics"), (show_stats_desc), vconfig.bOverlayStats));
			szr_debug->Add(CreateCheckBox(page_advanced, _("Texture Format Overlay"), (texfmt_desc), vconfig.bTexFmtOverlayEnable));
			if (vconfig.backend_info.bSupportsValidationLayer)
			{
				szr_debug->Add(validation_layer = CreateCheckBox(page_advanced, _("Enable API Validation Layers"),
					wxGetTranslation(validation_layer_desc),
					vconfig.bEnableValidationLayer));
			}


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
			hires_texturemaps = CreateCheckBox(page_advanced, _("Load Custom Material Maps"), load_hires_material_maps_desc, vconfig.bHiresMaterialMaps);
			szr_utility->Add(cache_hires_textures);
			if (vconfig.backend_info.bSupportsInternalResolutionFrameDumps)
			{
				szr_utility->Add(CreateCheckBox(page_advanced, _("Full Resolution Frame Dumps"),
					wxGetTranslation(internal_resolution_frame_dumping_desc),
					vconfig.bInternalResolutionFrameDumps));
			}
			szr_utility->Add(hires_texturemaps);
			szr_utility->Add(CreateCheckBox(page_advanced, _("Dump EFB Target"), (dump_efb_desc), vconfig.bDumpEFBTarget));
			szr_utility->Add(CreateCheckBox(page_advanced, _("Free Look"), (free_look_desc), vconfig.bFreeLook));
			szr_utility->Add(shaderprecompile = CreateCheckBox(page_advanced, _("Compile Shaders on Startup"), (shader_precompile_desc), vconfig.bCompileShaderOnStartup));

#if !defined WIN32 && defined HAVE_LIBAV
			szr_utility->Add(CreateCheckBox(page_advanced, _("Frame Dumps Use FFV1"), (use_ffv1_desc), vconfig.bUseFFV1));
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
	ctrl_descs.insert(std::pair<wxWindow*, wxString>(control, description));
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
	desc_texts.insert(std::pair<wxWindow*, wxStaticText*>(page, desc_text));
}

void VideoConfigDiag::Event_Backend(wxCommandEvent &ev)
{
	auto& new_backend = g_available_video_backends[ev.GetInt()];
	if (g_video_backend != new_backend.get())
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

			g_video_backend = new_backend.get();
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

void VideoConfigDiag::Event_Adapter(wxCommandEvent &ev)
{
	ev.Skip();
} // TODO

void VideoConfigDiag::Event_ProgressiveScan(wxCommandEvent &ev)
{
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
	vconfig.iBBoxMode = ev.GetInt();
	text_bboxmode->SetLabel((s_bbox_mode_text[vconfig.iBBoxMode]));
	ev.Skip();
}

static void ReloadPostProcessingShaders()
{
	// Reload the shader next frame.
	// Have to check post processor pointer here, if it is not supported by the backend.
	if (g_renderer && g_renderer->GetPostProcessor())
		g_renderer->GetPostProcessor()->SetReloadFlag();
}

void VideoConfigDiag::UpdatePostProcessingShadersConfig()
{
	if (listbox_selected_ppshaders->IsEmpty())
	{
		vconfig.bPostProcessingEnable = false;
		vconfig.sPostProcessingShaders.clear();
	}
	else
	{
		vconfig.bPostProcessingEnable = true;
		vconfig.sPostProcessingShaders.clear();
		for (unsigned int i = 0; i < listbox_selected_ppshaders->GetCount(); i++)
		{
			if (i > 0)
				vconfig.sPostProcessingShaders += ':';

			vconfig.sPostProcessingShaders += WxStrToStr(listbox_selected_ppshaders->GetString(i));
		}
	}

	ReloadPostProcessingShaders();
}

void VideoConfigDiag::UpdatePostProcessingShaderListButtons()
{
	int sel = listbox_selected_ppshaders->GetSelection();
	if (sel < 0 || listbox_selected_ppshaders->IsEmpty())
	{
		// Disable all list manipulation
		button_move_ppshader_up->Disable();
		button_move_ppshader_down->Disable();
		button_config_ppshader->Disable();
		button_remove_ppshader->Disable();
		return;
	}

	// Update move up/down button state
	button_move_ppshader_up->Enable((sel > 0));
	button_move_ppshader_down->Enable((sel != (int)listbox_selected_ppshaders->GetCount() - 1));
	button_remove_ppshader->Enable(true);

	// Load the shader config, and check if it has options
	std::string shader_name = WxStrToStr(listbox_selected_ppshaders->GetStringSelection());
	PostProcessingShaderConfiguration shader_config;
	if (shader_config.LoadShader(POSTPROCESSING_SHADER_SUBDIR, shader_name))
		button_config_ppshader->Enable(shader_config.HasOptions());
	else
		button_config_ppshader->Disable();
}

void VideoConfigDiag::Event_PPShaderList(wxCommandEvent& ev)
{
	UpdatePostProcessingShaderListButtons();
}

void VideoConfigDiag::Event_PPShaderListMoveUp(wxCommandEvent& ev)
{
	int sel = listbox_selected_ppshaders->GetSelection();
	if (sel <= 0)
		return;

	// Remove and re-insert at the correct position
	wxString shader_name = listbox_selected_ppshaders->GetString(sel);
	listbox_selected_ppshaders->Delete(sel);
	listbox_selected_ppshaders->Insert(shader_name, sel - 1);
	listbox_selected_ppshaders->SetSelection(sel - 1);
	UpdatePostProcessingShaderListButtons();
	UpdatePostProcessingShadersConfig();
	ReloadPostProcessingShaders();
}

void VideoConfigDiag::Event_PPShaderListMoveDown(wxCommandEvent& ev)
{
	int sel = listbox_selected_ppshaders->GetSelection();
	if (sel < 0 || (unsigned int)sel >= (listbox_selected_ppshaders->GetCount() - 1))
		return;

	// Remove and re-insert at the correct position
	wxString shader_name = listbox_selected_ppshaders->GetString(sel);
	listbox_selected_ppshaders->Delete(sel);
	listbox_selected_ppshaders->Insert(shader_name, sel + 1);
	listbox_selected_ppshaders->SetSelection(sel + 1);
	UpdatePostProcessingShaderListButtons();
	UpdatePostProcessingShadersConfig();
	ReloadPostProcessingShaders();
}

void VideoConfigDiag::Event_PPShaderListOptions(wxCommandEvent& ev)
{
	int sel = listbox_selected_ppshaders->GetSelection();
	if (sel < 0)
		return;

	std::string shader_name = WxStrToStr(listbox_selected_ppshaders->GetStringSelection());
	PostProcessingShaderConfiguration* shader_config = (g_renderer) ? g_renderer->GetPostProcessor()->GetPostShaderConfig(shader_name) : nullptr;
	PostProcessingConfigDiag dialog(this, POSTPROCESSING_SHADER_SUBDIR, shader_name, shader_config);
	dialog.ShowModal();
}

void VideoConfigDiag::Event_PPShaderListRemove(wxCommandEvent& ev)
{
	int sel = listbox_selected_ppshaders->GetSelection();
	if (sel < 0)
		return;

	listbox_selected_ppshaders->Delete(sel);
	if (!listbox_selected_ppshaders->IsEmpty())
	{
		if (sel >(int)listbox_selected_ppshaders->GetCount() - 1)
			listbox_selected_ppshaders->SetSelection(sel - 1);
		else
			listbox_selected_ppshaders->SetSelection(sel);
	}

	UpdatePostProcessingShaderListButtons();
	UpdatePostProcessingShadersConfig();
	ReloadPostProcessingShaders();
}

void VideoConfigDiag::Event_PPShaderAdd(wxCommandEvent& ev)
{
	wxString shader_name = choice_ppshader->GetStringSelection();
	listbox_selected_ppshaders->AppendString(shader_name);
	listbox_selected_ppshaders->SetSelection(listbox_selected_ppshaders->GetCount() - 1);
	UpdatePostProcessingShaderListButtons();
	UpdatePostProcessingShadersConfig();
	ReloadPostProcessingShaders();
}

void VideoConfigDiag::Event_ScalingShader(wxCommandEvent& ev)
{
	const int sel = ev.GetInt();
	if (sel)
		vconfig.sScalingShader = WxStrToStr(ev.GetString());
	else
		vconfig.sScalingShader.clear();

	// Load shader, determine whether to enable options button
	PostProcessingShaderConfiguration shader_config;
	if (shader_config.LoadShader(SCALING_SHADER_SUBDIR, vconfig.sScalingShader))
		button_config_scalingshader->Enable(shader_config.HasOptions());
	else
		button_config_scalingshader->Disable();

	ReloadPostProcessingShaders();
}

void VideoConfigDiag::Event_StereoShader(wxCommandEvent& ev)
{
	vconfig.sStereoShader = WxStrToStr(ev.GetString());
	ReloadPostProcessingShaders();
}

void VideoConfigDiag::Event_ConfigureScalingShader(wxCommandEvent &ev)
{
	PostProcessingConfigDiag dialog(this, SCALING_SHADER_SUBDIR, vconfig.sScalingShader, (g_renderer) ? g_renderer->GetPostProcessor()->GetScalingShaderConfig() : nullptr);
	dialog.ShowModal();
}

void VideoConfigDiag::Event_StereoDepth(wxCommandEvent &ev)
{
	vconfig.iStereoDepth = ev.GetInt();

	ev.Skip();
}

void VideoConfigDiag::Event_SpecularIntensity(wxCommandEvent &ev)
{
	vconfig.iSpecularMultiplier = ev.GetInt();

	ev.Skip();
}

void VideoConfigDiag::Event_RimIntensity(wxCommandEvent &ev)
{
	vconfig.iRimIntesity = ev.GetInt();

	ev.Skip();
}

void VideoConfigDiag::Event_RimPower(wxCommandEvent &ev)
{
	vconfig.iRimPower = ev.GetInt();

	ev.Skip();
}
void VideoConfigDiag::Event_RimBase(wxCommandEvent &ev)
{
	vconfig.iRimBase = ev.GetInt();

	ev.Skip();
}

void VideoConfigDiag::Event_BumpStrength(wxCommandEvent &ev)
{
	vconfig.iSimBumpStrength = ev.GetInt();

	ev.Skip();
}

void VideoConfigDiag::Event_BumpDetailBlend(wxCommandEvent &ev)
{
	vconfig.iSimBumpDetailBlend = ev.GetInt();

	ev.Skip();
}

void VideoConfigDiag::Event_BumpDetailFrequency(wxCommandEvent &ev)
{
	vconfig.iSimBumpDetailFrequency = ev.GetInt();

	ev.Skip();
}

void VideoConfigDiag::Event_BumpThreshold(wxCommandEvent &ev)
{
	vconfig.iSimBumpThreshold = ev.GetInt();

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
	// Snap the slider
	int value = ev.GetInt();
	if (90 < value && value < 110)
		conv_slider->SetValue(100);

	vconfig.iStereoConvergencePercentage = conv_slider->GetValue();

	ev.Skip();
}

void VideoConfigDiag::Event_TessellationDistance(wxCommandEvent &ev)
{
	vconfig.iTessellationDistance = ev.GetInt();

	ev.Skip();
}

void VideoConfigDiag::Event_TessellationMax(wxCommandEvent &ev)
{
	vconfig.iTessellationMax = ev.GetInt();

	ev.Skip();
}

void VideoConfigDiag::Event_TessellationRounding(wxCommandEvent &ev)
{
	vconfig.iTessellationRoundingIntensity = ev.GetInt();

	ev.Skip();
}

void VideoConfigDiag::Event_TessellationDisplacement(wxCommandEvent &ev)
{
	vconfig.iTessellationDisplacementIntensity = ev.GetInt();

	ev.Skip();
}

void VideoConfigDiag::Event_StereoMode(wxCommandEvent &ev)
{
	// Disable blit shader choice when anaglyph shader on
	vconfig.iStereoMode = ev.GetInt();
	choice_stereoshader->Enable((ev.GetInt() == STEREO_SHADER));

	ReloadPostProcessingShaders();

	ev.Skip();
}

// Enables/disables UI elements depending on current config
void VideoConfigDiag::OnUpdateUI(wxUpdateUIEvent& ev)
{
	bool phongEnabled = vconfig.backend_info.bSupportsPixelLighting && vconfig.bEnablePixelLighting && vconfig.bForcePhongShading;
	// Simulated bumps
	bump_strenght_slider->Enable(phongEnabled && vconfig.bSimBumpEnabled);
	bump_threshold_slider->Enable(phongEnabled && vconfig.bSimBumpEnabled);
	bump_blend_slider->Enable(phongEnabled && vconfig.bSimBumpEnabled);
	bump_frequency_slider->Enable(phongEnabled && vconfig.bSimBumpEnabled);

	// Anti-aliasing
	choice_aamode->Enable(vconfig.backend_info.AAModes.size() > 1);
	text_aamode->Enable(vconfig.backend_info.AAModes.size() > 1);

	// pixel lighting
	pixel_lighting->Enable(vconfig.backend_info.bSupportsPixelLighting);
	phong_lighting->Enable(vconfig.backend_info.bSupportsPixelLighting && vconfig.bEnablePixelLighting);
	sim_bump->Enable(phongEnabled);
	group_phong->Show(phongEnabled);
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
	hires_texturemaps->Enable(vconfig.bHiresTextures && vconfig.bEnablePixelLighting);
	hires_texturemaps->Show(vconfig.backend_info.bSupportsNormalMaps);


	Async_Shader_compilation->Show(vconfig.backend_info.bSupportsAsyncShaderCompilation);
	GPU_Texture_decoding->Show(vconfig.backend_info.bSupportsGPUTextureDecoding);
	Compute_Shader_encoding->Show(vconfig.backend_info.bSupportsComputeTextureEncoding);
	Forced_LogicOp->Show(vconfig.backend_info.APIType == API_D3D11);

	/*Predictive_FIFO->Show(vconfig.backend_info.APIType != API_OPENGL);
	Wait_For_Shaders->Show(vconfig.backend_info.APIType != API_OPENGL);
	bool WaitForShaderCompilationenabled = vconfig.bPredictiveFifo && !vconfig.bFullAsyncShaderCompilation;
	vconfig.bWaitForShaderCompilation = vconfig.bWaitForShaderCompilation && WaitForShaderCompilationenabled;
	Wait_For_Shaders->Enable(WaitForShaderCompilationenabled);
	Async_Shader_compilation->Enable(!vconfig.bWaitForShaderCompilation);*/
	// Things which shouldn't be changed during emulation
	if (Core::IsRunning())
	{
		if (vconfig.backend_info.bSupportsComputeTextureEncoding)
		{
			Compute_Shader_encoding->Disable();
		}
		shaderprecompile->Disable();
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
		if (vconfig.backend_info.bSupportsMultithreading)
		{
			backend_multithreading->Disable();
		}
		if (vconfig.backend_info.bSupportsValidationLayer)
		{
			validation_layer->Disable();
		}
		//Predictive_FIFO->Disable();
	}
	else
	{
		//Predictive_FIFO->Enable(!vconfig.bWaitForShaderCompilation);
	}
	// Don't enable 'vertex rounding' at native
	if (vconfig.iEFBScale == SCALE_1X)
	{
		vertex_rounding_checkbox->Enable(false);
	}
	else
	{
		vertex_rounding_checkbox->Enable(true);
	}

	ev.Skip();
}

void VideoConfigDiag::PopulatePostProcessingShaders()
{
	std::vector<std::string> shaders = PostProcessingShaderConfiguration::GetAvailableShaderNames(POSTPROCESSING_SHADER_SUBDIR);

	// No shaders found -> disable list and add button
	if (shaders.empty())
	{
		choice_ppshader->Disable();
		button_add_ppshader->Disable();
		return;
	}
	else
	{
		// Populate the list of shaders to add
		for (const std::string& shader : shaders)
			choice_ppshader->AppendString(StrToWxStr(shader));

		// Leave the first shader selected by default
		choice_ppshader->Select(0);
	}

	// Split the list of post-processing shaders, and fill the list box
	std::vector<std::string> ppshader_list;
	SplitString(vconfig.sPostProcessingShaders, ':', ppshader_list);
	for (const std::string& shader_name : ppshader_list)
		listbox_selected_ppshaders->AppendString(StrToWxStr(shader_name));

	if (!listbox_selected_ppshaders->IsEmpty())
		listbox_selected_ppshaders->SetSelection(0);

}

void VideoConfigDiag::PopulateScalingShaders()
{
	std::vector<std::string> shaders = PostProcessingShaderConfiguration::GetAvailableShaderNames(SCALING_SHADER_SUBDIR);

	choice_scalingshader->AppendString(_("(default)"));

	if (shaders.empty())
	{
		choice_scalingshader->Select(0);
		button_config_scalingshader->Disable();
		return;
	}

	for (const std::string& shader : shaders)
		choice_scalingshader->AppendString(StrToWxStr(shader));

	if (choice_scalingshader->SetStringSelection(StrToWxStr(vconfig.sScalingShader)))
	{
		// Load shader, determine whether to enable options button
		PostProcessingShaderConfiguration shader_config;
		if (shader_config.LoadShader(SCALING_SHADER_SUBDIR, vconfig.sScalingShader))
			button_config_scalingshader->Enable(shader_config.HasOptions());
		else
			button_config_scalingshader->Disable();
	}
	else
	{
		// Invalid shader, reset it to default
		choice_scalingshader->Select(0);
		button_config_scalingshader->Disable();
	}
}

void VideoConfigDiag::PopulateStereoShaders()
{
	std::vector<std::string> shaders = PostProcessingShaderConfiguration::GetAvailableShaderNames(STEREO_SHADER_SUBDIR);
	if (!shaders.empty())
	{
		for (const std::string& shader : shaders)
			choice_stereoshader->AppendString(StrToWxStr(shader));

		if (!choice_stereoshader->SetStringSelection(StrToWxStr(vconfig.sStereoShader)))
		{
			// Invalid shader, reset it to default
			choice_stereoshader->Select(0);
		}
	}

	// Set enabled based on stereo mode
	choice_stereoshader->Enable(vconfig.iStereoMode == STEREO_SHADER);
}

void VideoConfigDiag::PopulateAAList()
{
	const std::vector<int>& aa_modes = vconfig.backend_info.AAModes;
	const bool supports_ssaa = vconfig.backend_info.bSupportsSSAA;
	for (int mode : aa_modes)
	{
		if (mode == 1)
		{
			choice_aamode->AppendString(_("None"));
		}
		else
		{
			if ((vconfig.backend_info.APIType & API_D3D9) != 0)
			{
				choice_aamode->AppendString(std::to_string(mode * mode) + "x SSAA");
			}
			else
			{
				choice_aamode->AppendString(std::to_string(mode) + "x MSAA");
			}
		}
	}

	if (supports_ssaa)
	{
		for (int mode : aa_modes)
		{
			if (mode != 1)
				choice_aamode->AppendString(std::to_string(mode) + "x SSAA");
		}
	}

	int selected_mode_index = 0;

	auto index = std::find(aa_modes.begin(), aa_modes.end(), vconfig.iMultisamples);
	if (index != aa_modes.end())
		selected_mode_index = index - aa_modes.begin();

	// Select one of the SSAA modes at the end of the list if SSAA is enabled
	if (supports_ssaa && vconfig.bSSAA && aa_modes[selected_mode_index] != 1)
		selected_mode_index += aa_modes.size() - 1;

	choice_aamode->SetSelection(selected_mode_index);
}

void VideoConfigDiag::OnAAChanged(wxCommandEvent& ev)
{
	int mode = ev.GetInt();
	ev.Skip();
	const std::vector<int>& aa_modes = vconfig.backend_info.AAModes;
	vconfig.bSSAA = mode >= aa_modes.size();
	mode -= vconfig.bSSAA * (aa_modes.size() - 1);
	vconfig.iMultisamples = aa_modes[mode];
}
