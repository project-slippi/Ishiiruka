// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <Core/Slippi/SlippiPlayback.h>
#include <SlippiLib/SlippiGame.h>
#include <wx/utils.h>

#include "PlaybackSlider.h"

extern std::unique_ptr<SlippiPlaybackStatus> g_playbackStatus;

// Event table
BEGIN_EVENT_TABLE(PlaybackSlider, wxSlider)
EVT_LEFT_DOWN(PlaybackSlider::OnSliderDown)
EVT_LEFT_UP(PlaybackSlider::OnSliderClick)
EVT_SLIDER(wxID_ANY, PlaybackSlider::OnSliderMove)
END_EVENT_TABLE()

PlaybackSlider::~PlaybackSlider() = default;

bool PlaybackSlider::Create(wxStaticText *sliderLabel, wxWindow *parent, wxWindowID id, int value, int min_val,
                            int max_val, const wxPoint &pos, const wxSize &size, long style,
                            const wxValidator &validator, const wxString &name)
{
	seekBarText = sliderLabel;
	return DolphinSlider::Create(parent, id, value, min_val, max_val, pos, size, style, validator, name);
}

void PlaybackSlider::OnSliderClick(wxMouseEvent &event)
{
	// This handler is the confirmation handler that actually sets the frame we
	// want to skip to
	isDraggingSlider = false;
	g_playbackStatus->targetFrameNum = lastMoveVal;
	event.Skip();
}

void PlaybackSlider::OnSliderDown(wxMouseEvent &event)
{
	// This handler sets the slider position on a mouse down event. Normally
	// the Dolphin slider can only be changed by clicking and dragging
	isDraggingSlider = true;
	int value = CalculatePosition(event);

	// Sets the lastMoveVal for Windows because on a normal click the move event
	// doesn't fire fast enough
	lastMoveVal = value;

	this->SetValue(value);
	event.Skip();
}

int PlaybackSlider::CalculatePosition(wxMouseEvent &event)
{
	// This function calculates a frame value based on an event click postiion
	int min = this->GetMin();
	int max = this->GetMax();
	int pos, dim;

	if (this->GetWindowStyle() & wxVERTICAL)
	{
		pos = event.GetPosition().y;
		dim = this->GetSize().y;
	}
	else
	{
		// hard code hack to address calculate width correctly by accounting for border
		// TODO: revisit?
		pos = event.GetPosition().x - 9;
		dim = this->GetSize().x - 18;
	}

	if (pos >= 0 && pos < dim)
	{
		// now we're sure the click is on the slider, and (width != 0)
		int dim2 = (dim >> 1); // for proper rounding
		int val = (pos * (max - min) + dim2) / dim;

		return min + val;
	}

	return INT_MAX;
}

void PlaybackSlider::OnSliderMove(wxCommandEvent &event)
{
	if (!event.ShouldPropagate())
	{
		// On mac for some reason this event handler will infinitely trigger
		// itself, by adding this check, we can prevent that
		return;
	}

	// This function is responsible with updating the time text
	// while clicking and dragging
	int value = event.GetInt();

	// On mac the mouse up event always has the same position as the mouse down
	// event, this means clicking and dragging does not work. So instead let's
	// save the value of the last move here and use that to set the game pos
	lastMoveVal = value;

	int totalSeconds = (int)((g_playbackStatus->latestFrame - Slippi::GAME_FIRST_FRAME) / 60);
	int totalMinutes = (int)(totalSeconds / 60);
	int totalRemainder = (int)(totalSeconds % 60);

	int currSeconds = int((value - Slippi::GAME_FIRST_FRAME) / 60);
	int currMinutes = (int)(currSeconds / 60);
	int currRemainder = (int)(currSeconds % 60);
	// Position string (i.e. MM:SS)
	char endTime[6];
	sprintf(endTime, "%02d:%02d", totalMinutes, totalRemainder);
	char currTime[6];
	sprintf(currTime, "%02d:%02d", currMinutes, currRemainder);

	std::string time = std::string(currTime) + " / " + std::string(endTime);
	seekBarText->SetLabel(_(time));
	event.Skip();
	event.StopPropagation();
}
