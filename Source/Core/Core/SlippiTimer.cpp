#include "SlippiTimer.h"
#include "Core/SlippiPlayback.h"
#include "DolphinWX/Frame.h"

void slippiTimer::Notify()
{
	int totalSeconds = (int) ((g_latestFrame + 123) / 60);
	int totalMinutes = (int)(totalSeconds / 60);
	int totalRemainder = (int)(totalSeconds % 60);

	int currSeconds = int ((g_currentPlaybackFrame + 123) / 60);
	int currMinutes = (int)(currSeconds / 60);
	int currRemainder = (int)(currSeconds % 60);
	// Position string (i.e. MM:SS)
	char endTime[5];
	sprintf(endTime, "%02d:%02d", totalMinutes, totalRemainder);
	char currTime[5];
	sprintf(currTime, "%02d:%02d", currMinutes, currRemainder);

	std::string time = std::string(currTime) + " / " + std::string(endTime);

	// Setup the slider and gauge min/max values
	if (!hasSetRange)
	{
		m_slider->SetRange(-123, (int)(g_latestFrame));
		hasSetRange = true;
	}

	// Only update values while not actively seeking
	if (g_targetFrameNum == INT_MAX)
	{
		m_text->SetLabel(_(time));

		if (m_frame->isDraggingSlider == false)
			m_slider->SetValue(g_currentPlaybackFrame);
	}
}