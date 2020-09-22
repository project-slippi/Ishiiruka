// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wx/string.h>
#include <wx/stackwalk.h>

#include "Common/FileUtil.h"

class StackWalker : public wxStackWalker
{
	public:
		StackWalker(bool);
		~StackWalker() { log_file.Close(); };

		const File::IOFile& get_log_file() { return log_file; };
		bool will_write() {return _log; };

	protected:
    	void OnStackFrame(const wxStackFrame&);
	
	private:
		File::IOFile log_file;
		bool _log;
};
