/*
* Copyright (c) 2021-2022, The PurpleI2P Project
*
* This file is part of Purple i2pd project and licensed under BSD3
*
* See full license text in LICENSE file at top of project tree
*/

#ifndef CAPI_H__
#define CAPI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

// initialization start and stop
void C_InitI2P (int argc, char argv[], const char * appName);
// void C_InitI2P (int argc, char** argv, const char * appName);
void C_StartI2P ();
// write system log to logStream, if not specified to <appName>.log in application's folder
void C_CloseAcceptsTunnels ();
void C_StopI2P ();
void C_RunPeerTest (); // should be called after UPnP
// fetch status
int C_GetNetworkStatus (char* buf, size_t buf_len);
int C_GetTunnelCreationSuccessRate ();
uint64_t C_GetReceivedByte ();
uint32_t C_GetInBandwidth ();
uint64_t C_GetSentByte ();
uint32_t C_GetOutBandwidth ();
uint64_t C_GetTransitByte ();
uint32_t C_GetTransitBandwidth ();
int C_IsHTTPProxyEnabled ();
int C_IsSOCKSProxyEnabled ();
int C_IsBOBEnabled ();
int C_IsSAMEnabled ();
int C_IsI2CPEnabled ();
// fetch tunnels info
int C_GetClientTunnelsCount ();
int C_GetServerTunnelsCount ();
int C_GetClientTunnelsName (int index, char* buf, size_t buf_len);
int C_GetClientTunnelsIdent (int index, char* buf, size_t buf_len);
int C_GetHTTPProxyIdent (char* buf, size_t buf_len);
int C_GetSOCKSProxyIdent (char* buf, size_t buf_len);
int C_GetServerTunnelsName (int index, char* buf, size_t buf_len);
int C_GetServerTunnelsIdent (int index, char* buf, size_t buf_len);

int C_GetInboundTunnelsCount ();
int C_GetOutboundTunnelsCount ();
int C_GetInboundTunnelsFormattedInfo (int index, char* buf, size_t buf_len);
int C_GetOutboundTunnelsFormattedInfo (int index, char* buf, size_t buf_len);
// key related
int C_GenerateIdentToFile (const char * filename, const char * sk, uint16_t sigType, uint16_t cryptoType, char* buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif
