// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "PlaybackSlider.h"
#include <wx/utils.h>
#include "DolphinWX/DolphinSlider.h"
#include "Core/SlippiPlayback.h"

// Event table
BEGIN_EVENT_TABLE(PlaybackSlider, wxSlider)
EVT_LEFT_DOWN(PlaybackSlider::OnSliderDown)
EVT_LEFT_UP(PlaybackSlider::OnSliderClick)
EVT_SLIDER(wxID_ANY, PlaybackSlider::OnSliderMove)
END_EVENT_TABLE()

PlaybackSlider::~PlaybackSlider() = default;

bool PlaybackSlider::Create(wxStaticText *sliderLabel, wxWindow* parent, wxWindowID id, int value, int min_val, int max_val,
	const wxPoint& pos, const wxSize& size, long style,
	const wxValidator& validator, const wxString& name)
{
	seekBarText = sliderLabel;
	return DolphinSlider::Create(parent, id, value, min_val, max_val, pos, size, style, validator, name);
}

void PlaybackSlider::OnSliderClick(wxMouseEvent &event) {
	isDraggingSlider = false;
	g_targetFrameNum = (int32_t) CalculatePosition(event);
	event.Skip();
}

void PlaybackSlider::OnSliderDown(wxMouseEvent &event) {
	isDraggingSlider = true;
	int position = CalculatePosition(event);
	this->SetValue(position);
	event.Skip();
}

int PlaybackSlider::CalculatePosition(wxMouseEvent &event) {
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
		int dim2 = (dim >> 1); // for proper rounding
		int val = (pos * (max - min) + dim2) / dim;
		return min + val;
	}
	return INT_MAX;
}

void PlaybackSlider::OnSliderMove(wxCommandEvent &event)
{
	int value = event.GetInt();

	int totalSeconds = (int)((g_latestFrame + 123) / 60);
	int totalMinutes = (int)(totalSeconds / 60);
	int totalRemainder = (int)(totalSeconds % 60);

	int currSeconds = int((value + 123) / 60);
	int currMinutes = (int)(currSeconds / 60);
	int currRemainder = (int)(currSeconds % 60);
	// Position string (i.e. MM:SS)
	char endTime[5];
	sprintf(endTime, "%02d:%02d", totalMinutes, totalRemainder);
	char currTime[5];
	sprintf(currTime, "%02d:%02d", currMinutes, currRemainder);

	std::string time = std::string(currTime) + " / " + std::string(endTime);
	seekBarText->SetLabel(_(time));
	event.Skip();
}