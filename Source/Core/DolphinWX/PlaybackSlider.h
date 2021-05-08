// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinSlider.h"
#include <wx/slider.h>
#include <wx/stattext.h>

class PlaybackSlider : public DolphinSlider
{
  public:
	PlaybackSlider(wxStaticText *sliderLabel, wxWindow *parent, wxWindowID id, int value, int min_value, int max_value,
	               const wxPoint &pos = wxDefaultPosition, const wxSize &size = wxDefaultSize,
	               long style = wxSL_HORIZONTAL, const wxValidator &validator = wxDefaultValidator,
	               const wxString &name = wxSliderNameStr)
	{
		Create(sliderLabel, parent, id, value, min_value, max_value, pos, size, style, validator, name);
	}

	~PlaybackSlider();

	wxStaticText *seekBarText;
	bool isDraggingSlider;

	DECLARE_EVENT_TABLE()

	bool Create(wxStaticText *sliderLabel, wxWindow *parent, wxWindowID id, int value, int min_value, int max_value,
	            const wxPoint &pos = wxDefaultPosition, const wxSize &size = wxDefaultSize,
	            long style = wxSL_HORIZONTAL, const wxValidator &validator = wxDefaultValidator,
	            const wxString &name = wxSliderNameStr);
	void OnSliderClick(wxMouseEvent &event);
	void OnSliderDown(wxMouseEvent &event);
	int CalculatePosition(wxMouseEvent &event);
	void OnSliderMove(wxCommandEvent &event);

  private:
	int lastMoveVal;
};
