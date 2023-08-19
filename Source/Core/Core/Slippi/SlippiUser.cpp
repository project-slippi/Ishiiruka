#include "SlippiUser.h"

const std::vector<std::string> SlippiUser::defaultChatMessages = {
    "ggs",
    "one more",
    "brb",
    "good luck",

    "well played",
    "that was fun",
    "thanks",
    "too good",

    "sorry",
    "my b",
    "lol",
    "wow",

    "gotta go",
    "one sec",
    "let's play again later",
    "bad connection",
};

SlippiUser::SlippiUser(uintptr_t rs_exi_device_ptr)
{
    slprs_exi_device_ptr = rs_exi_device_ptr;
}

SlippiUser::~SlippiUser()
{
}

bool SlippiUser::AttemptLogin()
{
    return false;
}

void SlippiUser::OpenLogInPage()
{
}

void SlippiUser::ListenForLogIn()
{
}

bool SlippiUser::UpdateApp()
{
    return false;
}

void SlippiUser::LogOut()
{
}

void SlippiUser::OverwriteLatestVersion(std::string version)
{
}

SlippiUser::UserInfo SlippiUser::GetUserInfo()
{
    return SlippiUser::UserInfo
	{
        "uid",
        "playKey",
        "displayName",
        "connectCode",
        "latestVersion",
        "fileContents",
        0,
        defaultChatMessages
    };
}

bool SlippiUser::IsLoggedIn()
{
    return false;
}
