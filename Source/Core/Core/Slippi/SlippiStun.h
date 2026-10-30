#pragma once

#include <string>

#include <enet/enet.h>

#include "Common/CommonTypes.h"

// Finding how a socket looks from outside, and opening a router toward someone. Used where Slippi's
// matchmaking server isn't involved: rooms, and watching a room's match.
//
// Replies arrive on the socket's own ENet host, so a host that uses these sets Intercept as its
// intercept callback, which also keeps punch packets away from ENet
namespace SlippiStun
{
int ENET_CALLBACK Intercept(ENetHost *host, ENetEvent *event);

// Asks a STUN server how the host's socket looks from outside. The reply comes back through Intercept
void SendRequest(ENetHost *host, int server = 0);

// The outside address from a reply that has come in. False if none has
bool TakeReply(ENetHost *host, std::string &ip, u16 &port);

// Finds the host's outside address before anything else uses it, trying each server in turn. False
// when none answers
bool Discover(ENetHost *host, std::string &ip, u16 &port);

// Sends a few packets toward an address ("host:port") so this router lets its replies in
void Punch(ENetHost *host, const std::string &address);

// Drops what's kept for a host, before it's destroyed
void Forget(ENetHost *host);
} // namespace SlippiStun
