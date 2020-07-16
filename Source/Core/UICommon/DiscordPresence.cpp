// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "UICommon/DiscordPresence.h"

#include "Common/Hash.h"
#include "Common/StringUtil.h"
#include <Common/FileUtil.h>
#include <Common/CommonPaths.h>

#include "Core/ConfigManager.h"
#include <Core/Slippi/SlippiUser.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#ifdef USE_DISCORD_PRESENCE

#include <algorithm>
#include <cctype>
#include <ctime>
#include <discord-rpc/include/discord_rpc.h>
#include <string>
#include <set>

#endif


namespace Discord
{
#ifdef USE_DISCORD_PRESENCE
    namespace
    {
        Handler* event_handler = nullptr;
        const char* username = "";

        void HandleDiscordReady(const DiscordUser* user)
        {
            username = user->username;
        }

        std::string ArtworkForGameId(const std::string& gameid)
        {
            static const std::set<std::string> REGISTERED_GAMES{
                "GAL",  // GALE01: Super Smash Bros. Melee
            };

            std::string region_neutral_gameid = gameid.substr(0, 3);
            if (REGISTERED_GAMES.count(region_neutral_gameid) != 0)
            {
                // Discord asset keys can only be lowercase.
                std::transform(region_neutral_gameid.begin(), region_neutral_gameid.end(),
                    region_neutral_gameid.begin(), tolower);
                return "game_" + region_neutral_gameid;
            }
            return "";
        }

    }  // namespace
#endif

    Discord::Handler::~Handler() = default;

    void Init()
    {
#ifdef USE_DISCORD_PRESENCE
        if (!SConfig::GetInstance().m_DiscordPresence)
            return;

        DiscordEventHandlers handlers = {};

        handlers.ready = HandleDiscordReady;
        // The number is the client ID for Dolphin, it's used for images and the application name
        Discord_Initialize("733171318555410432", &handlers, 1, nullptr);
        UpdateDiscordPresence();
#endif
    }

    void CallPendingCallbacks()
    {
#ifdef USE_DISCORD_PRESENCE
        if (!SConfig::GetInstance().m_DiscordPresence)
            return;

        Discord_RunCallbacks();

#endif
    }

    void InitNetPlayFunctionality(Handler& handler)
    {
#ifdef USE_DISCORD_PRESENCE
        event_handler = &handler;
#endif
    }

    void UpdateDiscordPresence(int party_size, SecretType type, const std::string& secret,
        const std::string& current_game)
    {
#ifdef USE_DISCORD_PRESENCE
        if (!SConfig::GetInstance().m_DiscordPresence)
            return;

#if defined(__APPLE__)
        std::string userFilePath = File::GetBundleDirectory() + "/Contents/Resources" + DIR_SEP + "user.json";
#elif defined(_WIN32)
        std::string userFilePath = File::GetExeDirectory() + DIR_SEP + "user.json";
#else
        std::string userFilePath = File::GetUserPath(F_USERJSON_IDX);
#endif
        std::string title;

        if (File::Exists(userFilePath)) 
        {
            std::ifstream i(userFilePath);
            json j;
            i >> j;

            std::string connectCode = j["connectCode"].get<std::string>();

            title = connectCode;
        }
        
        std::string game_artwork = ArtworkForGameId(SConfig::GetInstance().GetGameID());

        DiscordRichPresence discord_presence = {};
        if (game_artwork.empty())
        {
            discord_presence.largeImageKey = "slippi_logo";
            discord_presence.largeImageText = "Dolphin is an emulator for the GameCube and the Wii.";
        }
        else
        {
            discord_presence.largeImageKey = game_artwork.c_str();
            discord_presence.largeImageText = title.c_str();
            discord_presence.smallImageKey = "slippi_logo";
            discord_presence.smallImageText = "Dolphin is an emulator for the GameCube and the Wii.";
        }

        if (!title.empty())
        {
            discord_presence.details = title.c_str();
        }
        else
        {
            discord_presence.details = "Not logged in";
        }

        //discord_presence.details = title.empty() ? "Not in-game" : title.c_str();
        discord_presence.startTimestamp = std::time(nullptr);

        if (party_size > 0)
        {
            if (party_size < 4)
            {
                discord_presence.state = "In a party";
                discord_presence.partySize = party_size;
                discord_presence.partyMax = 4;
            }
            else
            {
                // others can still join to spectate
                discord_presence.state = "In a full party";
                discord_presence.partySize = party_size;
                // Note: joining still works without partyMax
            }
        }

        std::string party_id;
        std::string secret_final;
        if (type != SecretType::Empty)
        {
            // Declearing party_id or secret_final here will deallocate the variable before passing the
            // values over to Discord_UpdatePresence.

            const size_t secret_length = secret.length();
            party_id = std::to_string(
                HashAdler32(reinterpret_cast<const u8*>(secret.c_str()), secret_length));

            const std::string secret_type = std::to_string(static_cast<int>(type));
            secret_final.reserve(secret_type.length() + 1 + secret_length);
            secret_final += secret_type;
            secret_final += '\n';
            secret_final += secret;
        }
        discord_presence.partyId = party_id.c_str();
        discord_presence.joinSecret = secret_final.c_str();

        Discord_UpdatePresence(&discord_presence);
#endif
    }

    std::string CreateSecretFromIPAddress(const std::string& ip_address, int port)
    {
        const std::string port_string = std::to_string(port);
        std::string secret;
        secret.reserve(ip_address.length() + 1 + port_string.length());
        secret += ip_address;
        secret += ':';
        secret += port_string;
        return secret;
    }

    void Shutdown()
    {
#ifdef USE_DISCORD_PRESENCE
        if (!SConfig::GetInstance().m_DiscordPresence)
            return;

        Discord_ClearPresence();
        Discord_Shutdown();
#endif
    }

    void SetDiscordPresenceEnabled(bool enabled)
    {
        if (SConfig::GetInstance().m_DiscordPresence == enabled)
            return;

        if (SConfig::GetInstance().m_DiscordPresence)
            Discord::Shutdown();

        SConfig::GetInstance().m_DiscordPresence = enabled;

        if (SConfig::GetInstance().m_DiscordPresence)
            Discord::Init();
    }

}  // namespace Discord