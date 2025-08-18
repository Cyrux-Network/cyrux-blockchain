/*
* Copyright (c) 2021-2022, The PurpleI2P Project
*
* This file is part of Purple i2pd project and licensed under BSD3
*
* See full license text in LICENSE file at top of project tree
*/

#include "../libi2pd/api.h"
#include "capi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Uses the example from: https://stackoverflow.com/a/9210560
// See also https://stackoverflow.com/questions/9210528/split-string-with-delimiters-in-c/9210560#
// Does not handle consecutive delimiters, this is only for passing
// lists of arguments by value to InitI2P from C_InitI2P
char** str_split(char* a_str, const char a_delim)
{
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = (char**) malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}

#ifdef __cplusplus
extern "C" {
#endif

void C_InitI2P (int argc, char argv[], const char * appName)
{
    const char* delim = " ";
    char* vargs = strdup(argv);
    char** args = str_split(vargs, *delim);
//    std::shared_ptr<std::ostream> p_cout(&std::cout, [](std::ostream*){});
    return i2p::api::InitI2P(argc, args, appName, nullptr);
}

void C_StartI2P ()
{
    return i2p::api::StartI2P();
}

void C_CloseAcceptsTunnels () {
    return i2p::api::CloseAcceptsTunnels();
}

void C_StopI2P ()
{
    return i2p::api::StopI2P();
}

void C_RunPeerTest ()
{
    return i2p::api::RunPeerTest();
}

int C_GetNetworkStatus (char* buf, size_t buf_len)
{
    std::string ret_str;
    int ret = i2p::api::GetNetworkStatus(ret_str);
    if (ret == 0)
    {
        snprintf(buf, buf_len, "%s", ret_str.c_str());
    }
    return ret;
}

int C_GetTunnelCreationSuccessRate ()
{
    int ret = i2p::api::GetTunnelCreationSuccessRate();
    return ret;
}

uint64_t C_GetReceivedByte ()
{
    uint64_t ret = i2p::api::GetReceivedByte();
    return ret;
}

uint32_t C_GetInBandwidth ()
{
    uint32_t ret = i2p::api::GetInBandwidth();
    return ret;
}

uint64_t C_GetSentByte ()
{
    uint64_t ret = i2p::api::GetSentByte();
    return ret;
}

uint32_t C_GetOutBandwidth ()
{
    uint32_t ret = i2p::api::GetOutBandwidth();
    return ret;
}

uint64_t C_GetTransitByte ()
{
    uint64_t ret = i2p::api::GetTransitByte();
    return ret;
}

uint32_t C_GetTransitBandwidth ()
{
    uint32_t ret = i2p::api::GetTransitBandwidth();
    return ret;
}

int C_IsHTTPProxyEnabled ()
{
    int ret = i2p::api::IsHTTPProxyEnabled();
    return ret;
}

int C_IsSOCKSProxyEnabled ()
{
    int ret = i2p::api::IsSOCKSProxyEnabled();
    return ret;
}

int C_IsBOBEnabled ()
{
    int ret = i2p::api::IsBOBEnabled();
    return ret;
}

int C_IsSAMEnabled ()
{
    int ret = i2p::api::IsSAMEnabled();
    return ret;
}

int C_IsI2CPEnabled ()
{
    int ret = i2p::api::IsI2CPEnabled();
    return ret;
}

int C_GetClientTunnelsCount ()
{
    int ret = i2p::api::GetClientTunnelsCount();
    return ret;
}

int C_GetServerTunnelsCount ()
{
    int ret = i2p::api::GetServerTunnelsCount();
    return ret;
}

int C_GetClientTunnelsName (int index, char* buf, size_t buf_len)
{
    if (index < C_GetClientTunnelsCount())
    {
        std::string ret_str;
        int ret = i2p::api::GetClientTunnelsName(ret_str, index);
        if (ret == 0)
        {
            snprintf(buf, buf_len, "%s", ret_str.c_str());
        }
        return ret;
    }
    return 1;
}

int C_GetClientTunnelsIdent (int index, char* buf, size_t buf_len)
{
    if (index < C_GetClientTunnelsCount())
    {
        std::string ret_str;
        int ret = i2p::api::GetClientTunnelsIdent(ret_str, index);
        if (ret == 0)
        {
            snprintf(buf, buf_len, "%s", ret_str.c_str());
        }
        return ret;
    }
    return 1;
}

int C_GetHTTPProxyIdent (char* buf, size_t buf_len)
{
    std::string ret_str;
    int ret = i2p::api::GetHTTPProxyIdent(ret_str);
    if (ret == 0)
    {
        snprintf(buf, buf_len, "%s", ret_str.c_str());
    }
    return ret;
}

int C_GetSOCKSProxyIdent (char* buf, size_t buf_len)
{
    std::string ret_str;
    int ret = i2p::api::GetSOCKSProxyIdent(ret_str);
    if (ret == 0)
    {
        snprintf(buf, buf_len, "%s", ret_str.c_str());
    }
    return ret;
}

int C_GetServerTunnelsName (int index, char* buf, size_t buf_len)
{
    if (index < C_GetServerTunnelsCount())
    {
        std::string ret_str;
        int ret = i2p::api::GetServerTunnelsName(ret_str, index);
        if (ret == 0)
        {
            snprintf(buf, buf_len, "%s", ret_str.c_str());
        }
        return ret;
    }
    return 1;
}

int C_GetServerTunnelsIdent (int index, char* buf, size_t buf_len)
{
    if (index < C_GetServerTunnelsCount())
    {
        std::string ret_str;
        int ret = i2p::api::GetServerTunnelsIdent(ret_str, index);
        if (ret == 0)
        {
            snprintf(buf, buf_len, "%s", ret_str.c_str());
        }
        return ret;
    }
    return 1;
}

int C_GetInboundTunnelsCount ()
{
    int ret = i2p::api::GetInboundTunnelsCount ();
    return ret;
}

int C_GetOutboundTunnelsCount ()
{
    int ret = i2p::api::GetOutboundTunnelsCount ();
    return ret;
}

int C_GetInboundTunnelsFormattedInfo (int index, char* buf, size_t buf_len)
{
    if (index < C_GetInboundTunnelsCount())
    {
        std::string ret_str;
        int ret = i2p::api::GetInboundTunnelsFormattedInfo(ret_str, index);
        if (ret == 0)
        {
            snprintf(buf, buf_len, "%s", ret_str.c_str());
        }
        return ret;
    }
    return 1;
}

int C_GetOutboundTunnelsFormattedInfo (int index, char* buf, size_t buf_len)
{
    if (index < C_GetOutboundTunnelsCount())
    {
        std::string ret_str;
        int ret = i2p::api::GetOutboundTunnelsFormattedInfo(ret_str, index);
        if (ret == 0)
        {
            snprintf(buf, buf_len, "%s", ret_str.c_str());
        }
        return ret;
    }
    return 1;
}

int C_GenerateIdentToFile (const char * filename, const char * sk, uint16_t sigType, uint16_t cryptoType, char* buf, size_t buf_len)
{
    size_t SK_LENGTH = 64; // 64 bytes
    uint8_t uint_sk[SK_LENGTH];
    memcpy(uint_sk, sk, SK_LENGTH);
    std::string str_filename(filename);
    std::string ret_str;
    int ret = i2p::api::GenerateIdentToFile(ret_str, str_filename, uint_sk, sigType, cryptoType);
    if (ret == 0)
    {
        snprintf(buf, buf_len, "%s", ret_str.c_str());
    }
    return ret;
}

#ifdef __cplusplus
}
#endif
