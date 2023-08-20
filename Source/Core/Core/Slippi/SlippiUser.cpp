#include "SlippiUser.h"

#include "SlippiRustExtensions.h"

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
    return slprs_user_attempt_login(slprs_exi_device_ptr);
}

void SlippiUser::OpenLogInPage()
{
    slprs_user_open_login_page(slprs_exi_device_ptr);
}

void SlippiUser::ListenForLogIn()
{
    slprs_user_listen_for_login(slprs_exi_device_ptr);
}

bool SlippiUser::UpdateApp()
{
    return slprs_user_update_app(slprs_exi_device_ptr);
}

void SlippiUser::LogOut()
{
    slprs_user_logout(slprs_exi_device_ptr);
}

void SlippiUser::OverwriteLatestVersion(std::string version)
{
    slprs_user_overwrite_latest_version(slprs_exi_device_ptr, version.c_str());
}

SlippiUser::UserInfo SlippiUser::GetUserInfo()
{
    RustUserInfo* info = slprs_user_get_info(slprs_exi_device_ptr);

    SlippiUser::UserInfo userInfo;
    userInfo.uid = std::string(info->uid);
    userInfo.playKey = std::string(info->play_key);
    userInfo.displayName = std::string(info->display_name);
    userInfo.connectCode = std::string(info->connect_code);
    userInfo.latestVersion = std::string(info->latest_version);
    userInfo.chatMessages = defaultChatMessages;

    slprs_user_free_info(info);

    return userInfo;
}

bool SlippiUser::IsLoggedIn()
{
    return slprs_user_get_is_logged_in(slprs_exi_device_ptr);
}
