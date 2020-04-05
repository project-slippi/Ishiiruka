#include "SlippiTimer.h"
#include "DolphinWX/Frame.h"

#include "SlippiPlayback/SlippiPlayback.h"
extern std::unique_ptr<SlippiPlaybackStatus> g_playback_status;

void slippiTimer::Notify()
{
	unsigned int totalSeconds = (g_playback_status->latestFrame + 123) / 60;
	unsigned char totalMinutes = totalSeconds / 60;
	unsigned char totalRemainder = totalSeconds % 60;

	unsigned int currSeconds = (g_playback_status->currentPlaybackFrame + 123) / 60;
	unsigned char currMinutes = currSeconds / 60;
	unsigned char currRemainder = currSeconds % 60;

	// Position string (i.e. MM:SS)
	char endTime[6];
	sprintf(endTime, "%02d:%02d", totalMinutes%100, totalRemainder);
	char currTime[6];
	sprintf(currTime, "%02d:%02d", currMinutes%100, currRemainder);
	std::string time = std::string(currTime) + " / " + std::string(endTime);

	// Setup the slider and gauge min/max values
	int minValue = m_slider->GetMin();
	int maxValue = m_slider->GetMax();
	if (maxValue != (int)g_playback_status->latestFrame || minValue != -123)
	{
		m_slider->SetRange(-123, (int)(g_playback_status->latestFrame));
	}

	// Only update values while not actively seeking
	if (g_playback_status->targetFrameNum == INT_MAX && m_slider->isDraggingSlider == false)
	{
		m_text->SetLabel(_(time));
		m_slider->SetValue(g_playback_status->currentPlaybackFrame);
	}
}
