#include <wx/timer.h> // timer for updating status bar
#include <wx/stattext.h>
#include "DolphinWX/Frame.h"
#include "DolphinWX/DolphinSlider.h"
#include "Common/Logging/Log.h"


class slippiTimer : public wxTimer
{
  public:
	slippiTimer(CFrame* frame, DolphinSlider *slider, wxStaticText *text) { 
		m_frame = frame;
		m_slider = slider; 
		m_text = text;
	}

	// Called each time the timer's timeout expires
	void Notify() wxOVERRIDE;

	CFrame *m_frame;
	DolphinSlider *m_slider;
	wxStaticText *m_text;

	bool hasSetRange = false;
};
