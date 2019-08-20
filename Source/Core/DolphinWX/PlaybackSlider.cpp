// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "PlaybackSlider.h"
#include <wx/utils.h>
#include "DolphinWX/DolphinSlider.h"
#include "Core/SlippiPlayback.h"
#include "Common/Logging/Log.h"

// Event table
BEGIN_EVENT_TABLE(PlaybackSlider, wxSlider)
EVT_LEFT_DOWN(PlaybackSlider::OnSliderClick)
END_EVENT_TABLE()

PlaybackSlider::PlaybackSlider() = default;
PlaybackSlider::~PlaybackSlider() = default;

bool PlaybackSlider::Create(wxWindow* parent, wxWindowID id, int value, int min_val, int max_val,
	const wxPoint& pos, const wxSize& size, long style,
	const wxValidator& validator, const wxString& name)
{
	return DolphinSlider::Create(parent, id, value, min_val, max_val, pos, size, style, validator, name);
}

void PlaybackSlider::OnSliderClick(wxMouseEvent &event) {
    int min = this->GetMin();
	int max = this->GetMax();
	int pos, dim;

	if (this->GetWindowStyle() & wxVERTICAL) {
		pos = event.GetPosition().y;
		dim = this->GetSize().y;
	} else {
		// hard code hack to address calculate width correctly by accounting for border
		// TODO: revisit?
		pos = event.GetPosition().x - 9;
		dim = this->GetSize().x - 18;
	}

	if (pos >= 0 && pos < dim) {
		// now we're sure the click is on the slider, and (width != 0)
		INFO_LOG(SLIPPI, "%d %d", pos, dim);
		int dim2 = (dim >> 1); // for proper rounding
		int val = (pos * (max - min) + dim2) / dim;
		g_targetFrameNum = min + val;
	}
}
