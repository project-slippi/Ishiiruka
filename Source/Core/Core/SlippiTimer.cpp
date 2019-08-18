#include "SlippiTimer.h"
#include "Core/SlippiPlayback.h"

void slippiTimer::Notify()
{
	int totalSeconds = (int) ((g_lastFrame + 123) / 60);
	int totalMinutes = (int)(totalSeconds / 60);
	int totalRemainder = (int)(totalSeconds % 60);

	int currSeconds = int ((g_currentPlaybackFrame + 123) / 60);
	int currMinutes = (int)(currSeconds / 60);
	int currRemainder = (int)(currSeconds % 60);
	// Position string (i.e. MM:SS)
	std::string endTime = std::to_string(totalMinutes) + ":" + std::to_string(totalRemainder);
	std::string currTime = std::to_string(currMinutes) + ":" + std::to_string(currRemainder);

	std::string time = currTime + " / " + endTime;

	// Setup the slider and gauge min/max values
	if (!hasSetRange)
	{
		m_slider->SetRange(0, totalSeconds);
		hasSetRange = true;
	}

	// Update text showing current position and time
	m_text->SetLabel(_(time));

	// if the slider is not being dragged then update it with the song position
	if (m_frame->isDraggingSlider == false)
		m_slider->SetValue(currSeconds);
}