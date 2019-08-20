// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/slider.h>
#include "DolphinSlider.h"

class PlaybackSlider : public DolphinSlider
{
  public:
	PlaybackSlider();
	~PlaybackSlider();

	PlaybackSlider(wxWindow *parent, wxWindowID id, int value, int min_value, int max_value,
	              const wxPoint &pos = wxDefaultPosition, const wxSize &size = wxDefaultSize,
	              long style = wxSL_HORIZONTAL, const wxValidator &validator = wxDefaultValidator,
	              const wxString &name = wxSliderNameStr)
	{
		Create(parent, id, value, min_value, max_value, pos, size, style, validator, name);
	}

	DECLARE_EVENT_TABLE()

	bool Create(wxWindow *parent, wxWindowID id, int value, int min_value, int max_value,
	            const wxPoint &pos = wxDefaultPosition, const wxSize &size = wxDefaultSize,
	            long style = wxSL_HORIZONTAL, const wxValidator &validator = wxDefaultValidator,
	            const wxString &name = wxSliderNameStr);

	void OnSliderClick(wxMouseEvent &event);
};
