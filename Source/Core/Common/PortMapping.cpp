#include "Common/PortMapping.h"

#include "Common/Logging/Log.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <miniupnpc.h>
#include <miniwget.h>
#include <natpmp.h>
#include <string>
#include <thread>
#include <upnpcommands.h>
#include <upnperrors.h>
#include <vector>

#ifndef _WIN32
#include <arpa/inet.h>
#endif

static std::array<char, 20> s_our_ip;
static u16 s_mapped = 0;
static std::thread s_thread;

static natpmp_t s_natpmp;

// called from ---Portmapping--- thread
static int GetNatpmpResponse(natpmpresp_t *response)
{
	int result;
	int i = 0;
	do
	{
		fd_set fds;
		struct timeval timeout;
		FD_ZERO(&fds);
		FD_SET(s_natpmp.s, &fds);
		result = getnatpmprequesttimeout(&s_natpmp, &timeout);
		if (result != 0)
			break;

		select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
		result = readnatpmpresponseorretry(&s_natpmp, response);
		i++;
	} while (i < 2 && result == NATPMP_TRYAGAIN);
	// 2 tries takes 750ms. Doesn't seem good to wait longer than that.

	return result;
}

// called from ---Portmapping--- thread
// discovers the NAT-PMP/PCP gateway
static bool InitNatpmp()
{
	static bool s_natpmpInited = false;
	static bool s_natpmpError = false;

	// Don't init if already inited
	if (s_natpmpInited)
		return true;

	// Don't init if it failed before
	if (s_natpmpError)
		return false;

	int result = initnatpmp(&s_natpmp, /* forcegw */ 0, /* forcedgw */ 0);
	if (result != 0)
	{
		WARN_LOG(NETPLAY, "[NAT-PMP] initnatpmp failed: %d", result);
		s_natpmpError = true;
		return false;
	}
	result = sendpublicaddressrequest(&s_natpmp);
	if (result != 2)
	{
		WARN_LOG(NETPLAY, "[NAT-PMP] sendpublicaddressrequest failed: %d", result);
		s_natpmpError = true;
		return false;
	}
	natpmpresp_t response;
	result = GetNatpmpResponse(&response);
	if (result != 0)
	{
		WARN_LOG(NETPLAY, "[NAT-PMP] publicaddress error: %d", result);
		s_natpmpError = true;
		return false;
	}

	WARN_LOG(NETPLAY, "[NAT-PMP] Inited, publicaddress: %s", inet_ntoa(response.pnu.publicaddress.addr));
	s_natpmpInited = true;
	return true;
}

// called from ---Portmapping--- thread
static bool UnmapPortNatpmp()
{
	sendnewportmappingrequest(&s_natpmp, NATPMP_PROTOCOL_UDP, s_mapped, s_mapped, 0);
	natpmpresp_t response;
	GetNatpmpResponse(&response);
	s_mapped = 0;
	return true;
}

// called from ---Portmapping--- thread
static bool MapPortNatpmp(const u16 port)
{
	if (s_mapped > 0 && s_mapped != port)
		UnmapPortNatpmp();

	int result = sendnewportmappingrequest(&s_natpmp, NATPMP_PROTOCOL_UDP, port, port, 604800);
	if (result != 12)
	{
		WARN_LOG(NETPLAY, "[NAT-PMP] sendnewportmappingrequest failed: %d", result);
		return false;
	}
	natpmpresp_t response;
	result = GetNatpmpResponse(&response);
	if (result != 0)
	{
		WARN_LOG(NETPLAY, "[NAT-PMP] portmapping error: %d", result);
		return false;
	}

	s_mapped = port;
	return true;
}

static UPNPUrls s_upnpUrls;
static IGDdatas s_igdDatas;

// called from ---Portmapping--- thread
// discovers the UPnP IGD
static bool InitUPnP()
{
	static bool s_upnpInited = false;
	static bool s_upnpError = false;

	// Don't init if already inited
	if (s_upnpInited)
		return true;

	// Don't init if it failed before
	if (s_upnpError)
		return false;

	s_upnpUrls = {};
	s_igdDatas = {};

	// Find all UPnP devices
	int upnperror = 0;
	std::unique_ptr<UPNPDev, decltype(&freeUPNPDevlist)> devlist(nullptr, freeUPNPDevlist);
#if MINIUPNPC_API_VERSION >= 14
	devlist.reset(upnpDiscover(2000, nullptr, nullptr, 0, 0, 2, &upnperror));
#else
	devlist.reset(upnpDiscover(2000, nullptr, nullptr, 0, 0, &upnperror));
#endif
	if (!devlist)
	{
		if (upnperror == UPNPDISCOVER_SUCCESS)
			WARN_LOG(NETPLAY, "[UPnP] No UPnP devices found");
		else
			WARN_LOG(NETPLAY, "[UPnP] Error while discovering UPnP devices: %s", strupnperror(upnperror));
		s_upnpError = true;
		return false;
	}

	// Look for the IGD
	bool found_valid_igd = false;
	for (UPNPDev *dev = devlist.get(); dev; dev = dev->pNext)
	{
		if (!std::strstr(dev->st, "InternetGatewayDevice"))
			continue;

		int desc_xml_size = 0;
		std::unique_ptr<char, decltype(&std::free)> desc_xml(nullptr, std::free);
		int statusCode = 200;
#if MINIUPNPC_API_VERSION >= 16
		desc_xml.reset(static_cast<char *>(miniwget_getaddr(dev->descURL, &desc_xml_size, s_our_ip.data(),
		                                                    static_cast<int>(s_our_ip.size()), 0, &statusCode)));
#else
		desc_xml.reset(static_cast<char *>(
		    miniwget_getaddr(dev->descURL, &desc_xml_size, s_our_ip.data(), static_cast<int>(s_our_ip.size()), 0)));
#endif
		if (desc_xml && statusCode == 200)
		{
			parserootdesc(desc_xml.get(), desc_xml_size, &s_igdDatas);
			GetUPNPUrls(&s_upnpUrls, &s_igdDatas, dev->descURL, 0);

			found_valid_igd = true;
			WARN_LOG(NETPLAY, "[UPnP] Got info from IGD at %s.", dev->descURL);
			break;
		}
		else
		{
			WARN_LOG(NETPLAY, "[UPnP] Error getting info from IGD at %s.", dev->descURL);
		}
	}

	if (!found_valid_igd)
	{
		WARN_LOG(NETPLAY, "[UPnP] Could not find IGD.");
		s_upnpError = true;
		return false;
	}

	WARN_LOG(NETPLAY, "[UPnP] Inited");
	s_upnpInited = true;
	return true;
}

// called from ---Portmapping--- thread
// Attempt to stop portforwarding.
// --
// NOTE: It is important that this happens! A few very crappy routers
// apparently do not delete UPnP mappings on their own, so if you leave them
// hanging, the NVRAM will fill with portmappings, and eventually all UPnP
// requests will fail silently, with the only recourse being a factory reset.
// --
static bool UnmapPortUPnP()
{
	std::string port_str = std::to_string(s_mapped);
	UPNP_DeletePortMapping(s_upnpUrls.controlURL, s_igdDatas.first.servicetype, port_str.c_str(), "UDP", nullptr);
	s_mapped = 0;
	return true;
}

// called from ---Portmapping--- thread
// Attempt to portforward!
static bool MapPortUPnP(const char *addr, const u16 port)
{
	if (s_mapped > 0 && s_mapped != port)
		UnmapPortUPnP();

	std::string port_str = std::to_string(port);
	int result =
	    UPNP_AddPortMapping(s_upnpUrls.controlURL, s_igdDatas.first.servicetype, port_str.c_str(), port_str.c_str(), addr,
	                        (std::string("dolphin-emu UDP on ") + addr).c_str(), "UDP", nullptr, nullptr);
	if (result != 0)
	{
		WARN_LOG(NETPLAY, "[UPnP] Failed to map port %d: %d", port, result);
		return false;
	}

	s_mapped = port;
	return true;
}

// Portmapping thread: try to map a port
static void MapPortThread(const u16 port)
{
	bool mapped = false;
	if (InitNatpmp())
	{
		if (MapPortNatpmp(port))
			mapped = true;
	}
	else if (InitUPnP())
	{
		if (MapPortUPnP(s_our_ip.data(), port))
			mapped = true;
	}

	if (mapped)
	{
		NOTICE_LOG(NETPLAY, "Successfully mapped port %d", port);
		return;
	}
}

// Portmapping thread: try to unmap a port
static void UnmapPortThread()
{
	if (s_mapped > 0)
	{
		u16 port = s_mapped;
		if (InitNatpmp())
			UnmapPortNatpmp();
		else if (InitUPnP())
			UnmapPortUPnP();
		NOTICE_LOG(NETPLAY, "Successfully unmapped port %d", port);
	}
}

void Common::TryPortmapping(u16 port)
{
	if (s_thread.joinable())
		s_thread.join();
	s_thread = std::thread(&MapPortThread, port);
}

void Common::TryPortmappingBlocking(u16 port)
{
	if (s_thread.joinable())
		s_thread.join();
	s_thread = std::thread(&MapPortThread, port);
	s_thread.join();
}

void Common::StopPortmapping()
{
	if (s_thread.joinable())
		s_thread.join();
	s_thread = std::thread(&UnmapPortThread);
	s_thread.join();
}
