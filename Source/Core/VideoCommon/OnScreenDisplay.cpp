// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <list>
#include <map>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/Timer.h"

#include "Core/ConfigManager.h"
#include "Core/NetPlayClient.h"

#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoConfig.h"

namespace OSD
{
// https://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

namespace Chat
{
    bool toggled = false;
    bool keep_open = false;
    std::string current_msg;

    static bool last_toggled = false;

    void Update()
    {
        if(NetPlay::IsNetPlayRunning())
        {
            if(!last_toggled && toggled)
                current_msg = "";

            if(last_toggled && !toggled)
            {
                trim(current_msg);

                if(current_msg != "")
                {
                    netplay_client->SendChatMessage(current_msg);
                    netplay_client->dialog->AppendChat(current_msg, true);

                    current_msg = "";
                }
                else
                    keep_open = false;
            }

            if(last_toggled && !toggled && keep_open)
            {
                keep_open = false;
                toggled = true;
            }
        }

        last_toggled = toggled;
    }
};

static std::multimap<CallbackType, Callback> s_callbacks;
static std::multimap<MessageType, Message> s_messages;
static std::mutex s_messages_mutex;

void AddTypedMessage(MessageType type, const std::string& message, u32 ms, u32 rgba)
{
	std::lock_guard<std::mutex> lock(s_messages_mutex);
	s_messages.erase(type);
	s_messages.emplace(type, Message(message, Common::Timer::GetTimeMs() + ms, rgba));
}

void AddMessage(const std::string& message, u32 ms, u32 rgba)
{
	std::lock_guard<std::mutex> lock(s_messages_mutex);
	s_messages.emplace(MessageType::Typeless,
		Message(message, Common::Timer::GetTimeMs() + ms, rgba));
}

void DrawMessage(const Message& msg, int top, int left, int time_left)
{
	float alpha = std::min(1.0f, std::max(0.0f, time_left / 1024.0f));
	u32 color = (msg.m_rgba & 0xFFFFFF) | ((u32)((msg.m_rgba >> 24) * alpha) << 24);

	g_renderer->RenderText(msg.m_str, left, top, color);
}

void DrawMessages()
{
	if (!SConfig::GetInstance().bOnScreenDisplayMessages)
		return;

	{
		std::lock_guard<std::mutex> lock(s_messages_mutex);

		u32 now = Common::Timer::GetTimeMs();
		int left = 20, top = 35 + (g_ActiveConfig.bShowOSDClock ? ((g_ActiveConfig.backend_info.APIType & API_D3D9) ||
			(g_ActiveConfig.backend_info.APIType & API_D3D11) ? 35 : 15) : 0);

		auto it = s_messages.begin();
		while (it != s_messages.end())
		{
			const Message& msg = it->second;
			int time_left = (int)(msg.m_timestamp - now);
			DrawMessage(msg, top, left, time_left);

			if (time_left <= 0)
				it = s_messages.erase(it);
			else
				++it;
			top += 15;
		}
	}
}

void ClearMessages()
{
	std::lock_guard<std::mutex> lock(s_messages_mutex);
	s_messages.clear();
}

// On-Screen Display Callbacks
void AddCallback(CallbackType type, Callback cb)
{
	s_callbacks.emplace(type, cb);
}

void DoCallbacks(CallbackType type)
{
	auto it_bounds = s_callbacks.equal_range(type);
	for (auto it = it_bounds.first; it != it_bounds.second; ++it)
	{
		it->second();
	}

	// Wipe all callbacks on shutdown
	if (type == CallbackType::Shutdown)
		s_callbacks.clear();
}

}  // namespace
