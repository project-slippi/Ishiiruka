// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstdio>
#include <string>
#include <ctime>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "Core/Debugger/StackWalker.h"


StackWalker::StackWalker(bool log)
  : _log(log)
{
#if defined(__APPLE__)
	std::string path = File::GetBundleDirectory() + "/Contents/Resources";
#elif defined(_WIN32)
	std::string path = File::GetExeDirectory();
#else
	std::string path = File::GetSysDirectory();
#endif

    // This conversion is necessary for *reasons*
    char sep = DIR_SEP_CHR;
    if (path.back() != sep) {
        path.push_back(sep);
    }
    
    int timestamp = std::time(0);

    path = StringFromFormat("%s%d.txt", path.c_str(), timestamp);
    
    if (_log) 
    {
        printf("%s\n", path.c_str());
        File::CreateEmptyFile(path);
        log_file = File::IOFile(path, "wb");
    }
}


void StackWalker::OnStackFrame(const wxStackFrame& frame)
{
    const std::string head = StringFromFormat(
        "Frame@ %p\n",
        frame.GetAddress()
    );

    const std::string body = StringFromFormat(
        "%lu %lu %s %s %s\n",
        frame.GetLine(),
        frame.GetLevel(),
        frame.GetFileName().c_str().AsChar(),
        frame.GetModule().c_str().AsChar(),
        frame.GetName().c_str().AsChar()
    );

    if (_log) {
        log_file.WriteBytes(head.data(), head.length());
        log_file.WriteBytes(body.data(), body.length());
    }
    
    // To stdout as well
    printf("%s%s", head.c_str(), body.c_str());
}