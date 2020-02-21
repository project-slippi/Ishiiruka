#pragma once

#include "Common/CommonTypes.h"

#include <curl/curl.h>

class SlippiMatchmaking
{
public:
	SlippiMatchmaking();
	~SlippiMatchmaking();

  void FindMatch();

protected:
	size_t receive(char *ptr, size_t size, size_t nmemb, void *userdata);

  CURL *m_curl = nullptr;
};

