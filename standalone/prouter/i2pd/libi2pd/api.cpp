/*
* Copyright (c) 2013-2020, The PurpleI2P Project
*
* This file is part of Purple i2pd project and licensed under BSD3
*
* See full license text in LICENSE file at top of project tree
*/

#include <string>
#include <map>
#include <ClientContext.h>
#include "Config.h"
#include "Log.h"
#include "NetDb.hpp"
#include "Transports.h"
#include "Tunnel.h"
#include "RouterContext.h"
#include "Identity.h"
#include "Destination.h"
#include "Crypto.h"
#include "FS.h"
#include "api.h"
#include "I18N.h"

namespace i2p
{
namespace api
{
	void InitI2P (int argc, char* argv[], const char * appName, std::shared_ptr<std::ostream> logstream)
	{
		i2p::config::Init ();
		i2p::config::ParseCmdline (argc, argv, true);
        std::string config;  i2p::config::GetOption("conf",    config);
        std::string datadir; i2p::config::GetOption("datadir", datadir);
        i2p::fs::SetAppName (appName);
        i2p::fs::DetectDataDir(datadir, false);
        i2p::fs::Init();
        datadir = i2p::fs::GetDataDir();

        if (config == "")
        {
            config = i2p::fs::DataDirPath("i2pd.conf");
            if (!i2p::fs::Exists (config)) {
                // use i2pd.conf only if exists
                config = ""; /* reset */
            }
        }
        i2p::config::ParseConfig(config);
        i2p::config::Finalize ();

        std::string certsdir; i2p::config::GetOption("certsdir", certsdir);
        i2p::fs::SetCertsDir(certsdir);

        std::string logs     = ""; i2p::config::GetOption("log",      logs);
        std::string logfile  = ""; i2p::config::GetOption("logfile",  logfile);
        std::string loglevel = ""; i2p::config::GetOption("loglevel", loglevel);
        bool logclftime;           i2p::config::GetOption("logclftime", logclftime);

        /* setup logging */
        if (logclftime)
            i2p::log::Logger().SetTimeFormat ("[%d/%b/%Y:%H:%M:%S %z]");

#ifdef WIN32_APP
        // Win32 app with GUI supports only logging to file
		logs = "file";
#else
        if (logs == "" || logs == "stdout")
            logs = "file";
#endif

        i2p::log::Logger().SetLogLevel(loglevel);

        if (logstream) {
            LogPrint(eLogInfo, "Log: will send messages to std::ostream");
            i2p::log::Logger().SendTo (logstream);
        } else if (logs == "file") {
            if (logfile == "")
                logfile = i2p::fs::DataDirPath("i2pd.log");
            LogPrint(eLogInfo, "Log: will send messages to ", logfile);
            i2p::log::Logger().SendTo (logfile);
#ifndef _WIN32
        } else if (logs == "syslog") {
            LogPrint(eLogInfo, "Log: will send messages to syslog");
            i2p::log::Logger().SendTo("i2pd", LOG_DAEMON);
#endif
        } else {
            // use stdout -- default
        }


        LogPrint(eLogNone,  "i2pd v", VERSION, " (", I2P_VERSION, ") starting");
        LogPrint(eLogDebug, "FS: main config file: ", config);
        LogPrint(eLogDebug, "FS: data directory: ", datadir);
        LogPrint(eLogDebug, "FS: certificates directory: ", certsdir);

        bool precomputation; i2p::config::GetOption("precomputation.elgamal", precomputation);
        bool aesni; i2p::config::GetOption("cpuext.aesni", aesni);
        bool avx; i2p::config::GetOption("cpuext.avx", avx);
        bool forceCpuExt; i2p::config::GetOption("cpuext.force", forceCpuExt);
        i2p::crypto::InitCrypto (precomputation, aesni, avx, forceCpuExt);

        int netID; i2p::config::GetOption("netid", netID);
        i2p::context.SetNetID (netID);
        i2p::context.Init ();

        bool ipv6; i2p::config::GetOption("ipv6", ipv6);
        bool ipv4; i2p::config::GetOption("ipv4", ipv4);
#ifdef MESHNET
        // manual override for meshnet
		ipv4 = false;
		ipv6 = true;
#endif
        // ifname -> address
        std::string ifname; i2p::config::GetOption("ifname", ifname);
        if (ipv4 && i2p::config::IsDefault ("address4"))
        {
            std::string ifname4; i2p::config::GetOption("ifname4", ifname4);
            if (!ifname4.empty ())
                i2p::config::SetOption ("address4", i2p::util::net::GetInterfaceAddress(ifname4, false).to_string ()); // v4
            else if (!ifname.empty ())
                i2p::config::SetOption ("address4", i2p::util::net::GetInterfaceAddress(ifname, false).to_string ()); // v4
        }
        if (ipv6 && i2p::config::IsDefault ("address6"))
        {
            std::string ifname6; i2p::config::GetOption("ifname6", ifname6);
            if (!ifname6.empty ())
                i2p::config::SetOption ("address6", i2p::util::net::GetInterfaceAddress(ifname6, true).to_string ()); // v6
            else if (!ifname.empty ())
                i2p::config::SetOption ("address6", i2p::util::net::GetInterfaceAddress(ifname, true).to_string ()); // v6
        }

        bool ygg; i2p::config::GetOption("meshnets.yggdrasil", ygg);
        boost::asio::ip::address_v6 yggaddr;
        if (ygg)
        {
            std::string yggaddress; i2p::config::GetOption ("meshnets.yggaddress", yggaddress);
            if (!yggaddress.empty ())
            {
                yggaddr = boost::asio::ip::address_v6::from_string (yggaddress);
                if (yggaddr.is_unspecified () || !i2p::util::net::IsYggdrasilAddress (yggaddr) ||
                    !i2p::util::net::IsLocalAddress (yggaddr))
                {
                    LogPrint(eLogWarning, "API: Can't find Yggdrasil address ", yggaddress);
                    ygg = false;
                }
            }
            else
            {
                yggaddr = i2p::util::net::GetYggdrasilAddress ();
                if (yggaddr.is_unspecified ())
                {
                    LogPrint(eLogWarning, "API: Yggdrasil is not running. Disabled");
                    ygg = false;
                }
            }
        }

        uint16_t port; i2p::config::GetOption("port", port);
        if (!i2p::config::IsDefault("port"))
        {
            LogPrint(eLogInfo, "API: accepting incoming connections at port ", port);
            i2p::context.UpdatePort (port);
        }
        i2p::context.SetSupportsV6 (ipv6);
        i2p::context.SetSupportsV4 (ipv4);
        i2p::context.SetSupportsMesh (ygg, yggaddr);

        i2p::context.RemoveNTCPAddress (!ipv6); // TODO: remove later
        bool ntcp2; i2p::config::GetOption("ntcp2.enabled", ntcp2);
        if (ntcp2)
        {
            bool published; i2p::config::GetOption("ntcp2.published", published);
            if (published)
            {
                std::string ntcp2proxy; i2p::config::GetOption("ntcp2.proxy", ntcp2proxy);
                if (!ntcp2proxy.empty ()) published = false;
            }
            if (published)
            {
                uint16_t ntcp2port; i2p::config::GetOption("ntcp2.port", ntcp2port);
                if (!ntcp2port) ntcp2port = port; // use standard port
                i2p::context.PublishNTCP2Address (ntcp2port, true, ipv4, ipv6, false); // publish
                if (ipv6)
                {
                    std::string ipv6Addr; i2p::config::GetOption("ntcp2.addressv6", ipv6Addr);
                    auto addr = boost::asio::ip::address_v6::from_string (ipv6Addr);
                    if (!addr.is_unspecified () && addr != boost::asio::ip::address_v6::any ())
                        i2p::context.UpdateNTCP2V6Address (addr); // set ipv6 address if configured
                }
            }
            else
                i2p::context.PublishNTCP2Address (port, false, ipv4, ipv6, false); // unpublish
        }
        if (ygg)
        {
            i2p::context.PublishNTCP2Address (port, true, false, false, true);
            i2p::context.UpdateNTCP2V6Address (yggaddr);
            if (!ipv4 && !ipv6)
                i2p::context.SetStatus (eRouterStatusMesh);
        }

        bool transit; i2p::config::GetOption("notransit", transit);
        i2p::context.SetAcceptsTunnels (!transit);
        uint16_t transitTunnels; i2p::config::GetOption("limits.transittunnels", transitTunnels);
        SetMaxNumTransitTunnels (transitTunnels);

        bool isFloodfill; i2p::config::GetOption("floodfill", isFloodfill);
        if (isFloodfill) {
            LogPrint(eLogInfo, "API: router will be floodfill");
            i2p::context.SetFloodfill (true);
        }
        else
        {
            i2p::context.SetFloodfill (false);
        }

        /* this section also honors 'floodfill' flag, if set above */
        std::string bandwidth; i2p::config::GetOption("bandwidth", bandwidth);
        if (bandwidth.length () > 0)
        {
            if (bandwidth[0] >= 'K' && bandwidth[0] <= 'X')
            {
                i2p::context.SetBandwidth (bandwidth[0]);
                LogPrint(eLogInfo, "API: bandwidth set to ", i2p::context.GetBandwidthLimit (), "KBps");
            }
            else
            {
                auto value = std::atoi(bandwidth.c_str());
                if (value > 0)
                {
                    i2p::context.SetBandwidth (value);
                    LogPrint(eLogInfo, "API: bandwidth set to ", i2p::context.GetBandwidthLimit (), " KBps");
                }
                else
                {
                    LogPrint(eLogInfo, "API: unexpected bandwidth ", bandwidth, ". Set to 'low'");
                    i2p::context.SetBandwidth (i2p::data::CAPS_FLAG_LOW_BANDWIDTH2);
                }
            }
        }
        else if (isFloodfill)
        {
            LogPrint(eLogInfo, "API: floodfill bandwidth set to 'extra'");
            i2p::context.SetBandwidth (i2p::data::CAPS_FLAG_EXTRA_BANDWIDTH2);
        }
        else
        {
            LogPrint(eLogInfo, "API: bandwidth set to 'low'");
            i2p::context.SetBandwidth (i2p::data::CAPS_FLAG_LOW_BANDWIDTH2);
        }

        int shareRatio; i2p::config::GetOption("share", shareRatio);
        i2p::context.SetShareRatio (shareRatio);

        std::string family; i2p::config::GetOption("family", family);
        i2p::context.SetFamily (family);
        if (family.length () > 0)
            LogPrint(eLogInfo, "API: family set to ", family);

        bool trust; i2p::config::GetOption("trust.enabled", trust);
        if (trust)
        {
            LogPrint(eLogInfo, "API: explicit trust enabled");
            std::string fam; i2p::config::GetOption("trust.family", fam);
            std::string routers; i2p::config::GetOption("trust.routers", routers);
            bool restricted = false;
            if (fam.length() > 0)
            {
                std::set<std::string> fams;
                size_t pos = 0, comma;
                do
                {
                    comma = fam.find (',', pos);
                    fams.insert (fam.substr (pos, comma != std::string::npos ? comma - pos : std::string::npos));
                    pos = comma + 1;
                }
                while (comma != std::string::npos);
                i2p::transport::transports.RestrictRoutesToFamilies(fams);
                restricted = fams.size() > 0;
            }
            if (routers.length() > 0) {
                std::set<i2p::data::IdentHash> idents;
                size_t pos = 0, comma;
                do
                {
                    comma = routers.find (',', pos);
                    i2p::data::IdentHash ident;
                    ident.FromBase64 (routers.substr (pos, comma != std::string::npos ? comma - pos : std::string::npos));
                    idents.insert (ident);
                    pos = comma + 1;
                }
                while (comma != std::string::npos);
                LogPrint(eLogInfo, "API: setting restricted routes to use ", idents.size(), " trusted routers");
                i2p::transport::transports.RestrictRoutesToRouters(idents);
                restricted = idents.size() > 0;
            }
            if(!restricted)
                LogPrint(eLogError, "API: no trusted routers of families specified");
        }

        bool hidden; i2p::config::GetOption("trust.hidden", hidden);
        if (hidden)
        {
            LogPrint(eLogInfo, "API: using hidden mode");
            i2p::data::netdb.SetHidden(true);
        }

        std::string httpLang; i2p::config::GetOption("http.lang", httpLang);
        i2p::i18n::SetLanguage(httpLang);
	}

	void StartI2P ()
	{
		i2p::log::Logger().Start ();
		LogPrint(eLogInfo, "API: Starting NetDB");
		i2p::data::netdb.Start();

        bool ntcp2; i2p::config::GetOption("ntcp2.enabled", ntcp2);
        bool ssu2; i2p::config::GetOption("ssu2.enabled", ssu2);
        bool ssu; i2p::config::GetOption("ssu", ssu);
        bool checkInReserved; i2p::config::GetOption("reservedrange", checkInReserved);
        LogPrint(eLogInfo, "API: starting Transports");
        if(!ssu) LogPrint(eLogInfo, "API: ssu disabled");
        if(!ntcp2) LogPrint(eLogInfo, "API: ntcp2 disabled");

        i2p::transport::transports.SetCheckReserved(checkInReserved);
        i2p::transport::transports.Start(ntcp2, ssu, ssu2);
        if (i2p::transport::transports.IsBoundSSU() || i2p::transport::transports.IsBoundNTCP2())
            LogPrint(eLogInfo, "API: Transports started");
        else
        {
            LogPrint(eLogError, "API: failed to start Transports");
            /** shut down netdb right away */
            i2p::transport::transports.Stop();
            i2p::data::netdb.Stop();
            return;
        }

        LogPrint(eLogInfo, "API: starting Tunnels");
        i2p::tunnel::tunnels.Start();

        LogPrint(eLogInfo, "API: starting Client");
        i2p::client::context.Start ();
	}

    void CloseAcceptsTunnels ()
    {
        i2p::context.SetAcceptsTunnels (false);
    }

	void StopI2P ()
	{
        LogPrint(eLogInfo, "API: shutting down");
        LogPrint(eLogInfo, "API: stopping Client");
        i2p::client::context.Stop();
        LogPrint(eLogInfo, "API: stopping Tunnels");
        i2p::tunnel::tunnels.Stop();
        LogPrint(eLogInfo, "API: stopping Transports");
        i2p::transport::transports.Stop();
        LogPrint(eLogInfo, "API: stopping NetDB");
        i2p::data::netdb.Stop();
        i2p::crypto::TerminateCrypto ();
        i2p::log::Logger().Stop ();
	}

	void RunPeerTest ()
	{
		i2p::transport::transports.PeerTest ();
	}

    int GetNetworkStatus (std::string& status)
    {
        auto contextStatus = i2p::context.GetStatus ();
        switch (contextStatus)
        {
            case eRouterStatusOK: status = "OK"; break;
            case eRouterStatusTesting: status = "Testing"; break;
            case eRouterStatusFirewalled: status = "Firewalled"; break;
            case eRouterStatusUnknown: status = "Unknown"; break;
            case eRouterStatusProxy: status = "Proxy"; break;
            case eRouterStatusMesh: status = "Mesh"; break;
            case eRouterStatusError:
            {
                switch (i2p::context.GetError ())
                {
                    case eRouterErrorClockSkew:
                        status = "Error - Clock skew";
                        break;
                    case eRouterErrorOffline:
                        status = "Error - Offline";
                        break;
                    case eRouterErrorSymmetricNAT:
                        status = "Error - Symmetric NAT";
                        break;
                    default:
                        status = "Error - Unknown";
                }
                break;
            }
            default: status = "Unknown";
        }
        return 0;
    }

    int GetTunnelCreationSuccessRate ()
    {
        int ret = i2p::tunnel::tunnels.GetTunnelCreationSuccessRate ();
        return ret;
    }

    uint64_t GetReceivedByte ()
    {
        uint64_t ret = i2p::transport::transports.GetTotalReceivedBytes ();
        return ret;
    }

    uint32_t GetInBandwidth ()
    {
        uint32_t ret = i2p::transport::transports.GetInBandwidth ();
        return ret;
    }

    uint64_t GetSentByte ()
    {
        uint64_t ret = i2p::transport::transports.GetTotalSentBytes ();
        return ret;
    }

    uint32_t GetOutBandwidth ()
    {
        uint32_t ret = i2p::transport::transports.GetOutBandwidth ();
        return ret;
    }

    uint64_t GetTransitByte ()
    {
        uint64_t ret = i2p::transport::transports.GetTotalTransitTransmittedBytes ();
        return ret;
    }

    uint32_t GetTransitBandwidth ()
    {
        uint32_t ret = i2p::transport::transports.GetTransitBandwidth ();
        return ret;
    }

    int IsHTTPProxyEnabled ()
    {
        int ret = i2p::client::context.GetHttpProxy () ? 1 : 0;
        return ret;
    }

    int IsSOCKSProxyEnabled ()
    {
        int ret = i2p::client::context.GetSocksProxy () ? 1 : 0;
        return ret;
    }

    int IsBOBEnabled ()
    {
        int ret = i2p::client::context.GetBOBCommandChannel () ? 1 : 0;
        return ret;
    }

    int IsSAMEnabled ()
    {
        int ret = i2p::client::context.GetSAMBridge () ? 1 : 0;
        return ret;
    }

    int IsI2CPEnabled ()
    {
        int ret = i2p::client::context.GetI2CPServer () ? 1 : 0;
        return ret;
    }

    int GetClientTunnelsCount ()
    {
        auto& tunnels = i2p::client::context.GetClientTunnels ();
        return tunnels.size();
    }

    int GetServerTunnelsCount ()
    {
        auto& tunnels = i2p::client::context.GetServerTunnels ();
        return tunnels.size();
    }

    int GetClientTunnelsName (std::string& name, int index)
    {
        if (index < GetClientTunnelsCount())
        {
            auto& tunnels = i2p::client::context.GetClientTunnels ();
            auto it = tunnels.begin();
            std::advance(it, index);
            name = it->second->GetName();
            return 0;
        }
        return 1;
    }

    int GetClientTunnelsIdent (std::string& ident, int index)
    {
        if (index < GetClientTunnelsCount())
        {
            auto& tunnels = i2p::client::context.GetClientTunnels ();
            auto it = tunnels.begin();
            std::advance(it, index);
            ident = it->second->GetLocalDestination ()->GetIdentHash().ToBase32 ();
            return 0;
        }
        return 1;
    }

    int GetHTTPProxyIdent (std::string& ident)
    {
        auto httpProxy = i2p::client::context.GetHttpProxy ();
        if (httpProxy)
        {
            ident = httpProxy->GetLocalDestination ()->GetIdentHash().ToBase32();
            return 0;
        }
        return 1;
    }

    int GetSOCKSProxyIdent (std::string& ident)
    {
        auto socksProxy = i2p::client::context.GetSocksProxy ();
        if (socksProxy)
        {
            ident = socksProxy->GetLocalDestination ()->GetIdentHash().ToBase32();
            return 0;
        }
        return 1;
    }

    int GetServerTunnelsName (std::string& name, int index)
    {
        if (index < GetServerTunnelsCount())
        {
            auto& tunnels = i2p::client::context.GetServerTunnels ();
            auto it = tunnels.begin();
            std::advance(it, index);
            name = it->second->GetName();
            return 0;
        }
        return 1;
    }

    int GetServerTunnelsIdent (std::string& ident, int index)
    {
        if (index < GetServerTunnelsCount())
        {
            auto& tunnels = i2p::client::context.GetServerTunnels ();
            auto it = tunnels.begin();
            std::advance(it, index);
            ident = it->second->GetLocalDestination ()->GetIdentHash().ToBase32 () + " : " + std::to_string(it->second->GetLocalPort ());
            return 0;
        }
        return 1;
    }

    int GetInboundTunnelsCount ()
    {
        auto& tunnels = i2p::tunnel::tunnels.GetInboundTunnels ();
        return tunnels.size();
    }

    int GetOutboundTunnelsCount ()
    {
        auto& tunnels = i2p::tunnel::tunnels.GetOutboundTunnels();
        return tunnels.size();
    }

    int GetInboundTunnelsFormattedInfo (std::string& info, int index)
    {
        if (index < GetInboundTunnelsCount())
        {
            auto ExplPool = i2p::tunnel::tunnels.GetExploratoryPool ();
            auto& tunnels = i2p::tunnel::tunnels.GetInboundTunnels ();
            auto it = tunnels.begin();
            std::advance(it, index);
            std::stringstream ss;
            (*it)->Print(ss);
            if ((*it)->LatencyIsKnown())
                ss << " ( " << (*it)->GetMeanLatency() << "ms )";

            std::string stateText;
            switch ((*it)->GetState()) {
                case i2p::tunnel::eTunnelStateBuildReplyReceived :
                case i2p::tunnel::eTunnelStatePending     : stateText = "building";    break;
                case i2p::tunnel::eTunnelStateBuildFailed :
                case i2p::tunnel::eTunnelStateTestFailed  :
                case i2p::tunnel::eTunnelStateFailed      : stateText = "failed";      break;
                case i2p::tunnel::eTunnelStateExpiring    : stateText = "expiring";    break;
                case i2p::tunnel::eTunnelStateEstablished : stateText = "established"; break;
                default: stateText = "unknown"; break;
            }

            ss << " " << stateText << (((*it)->GetTunnelPool () == ExplPool) ? " (exploratory)," : ",");
            ss << " " << (int) ((*it)->GetNumReceivedBytes() / 1024) << " KiB";
            info = ss.str();
            return 0;
        }
        return 1;
    }

    int GetOutboundTunnelsFormattedInfo (std::string& info, int index)
    {
        if (index < GetOutboundTunnelsCount())
        {
            auto ExplPool = i2p::tunnel::tunnels.GetExploratoryPool ();
            auto& tunnels = i2p::tunnel::tunnels.GetOutboundTunnels ();
            auto it = tunnels.begin();
            std::advance(it, index);
            std::stringstream ss;
            (*it)->Print(ss);
            if ((*it)->LatencyIsKnown())
                ss << " ( " << (*it)->GetMeanLatency() << "ms )";

            std::string stateText;
            switch ((*it)->GetState()) {
                case i2p::tunnel::eTunnelStateBuildReplyReceived :
                case i2p::tunnel::eTunnelStatePending     : stateText = "building";    break;
                case i2p::tunnel::eTunnelStateBuildFailed :
                case i2p::tunnel::eTunnelStateTestFailed  :
                case i2p::tunnel::eTunnelStateFailed      : stateText = "failed";      break;
                case i2p::tunnel::eTunnelStateExpiring    : stateText = "expiring";    break;
                case i2p::tunnel::eTunnelStateEstablished : stateText = "established"; break;
                default: stateText = "unknown"; break;
            }
            ss << " " << stateText << (((*it)->GetTunnelPool () == ExplPool) ? " (exploratory)," : ",");
            ss << " " << (int) ((*it)->GetNumSentBytes () / 1024) << " KiB";
            info = ss.str();
            return 0;
        }
        return 1;
    }

	std::shared_ptr<i2p::client::ClientDestination> CreateLocalDestination (const i2p::data::PrivateKeys& keys, bool isPublic,
		const std::map<std::string, std::string> * params)
	{
		auto localDestination = std::make_shared<i2p::client::RunnableClientDestination> (keys, isPublic, params);
		localDestination->Start ();
		return localDestination;
	}

	std::shared_ptr<i2p::client::ClientDestination> CreateLocalDestination (bool isPublic, i2p::data::SigningKeyType sigType,
		const std::map<std::string, std::string> * params)
	{
		i2p::data::PrivateKeys keys = i2p::data::PrivateKeys::CreateRandomKeys (sigType);
		auto localDestination = std::make_shared<i2p::client::RunnableClientDestination> (keys, isPublic, params);
		localDestination->Start ();
		return localDestination;
	}

	void DestroyLocalDestination (std::shared_ptr<i2p::client::ClientDestination> dest)
	{
		if (dest)
			dest->Stop ();
	}

	void RequestLeaseSet (std::shared_ptr<i2p::client::ClientDestination> dest, const i2p::data::IdentHash& remote)
	{
		if (dest)
			dest->RequestDestination (remote);
	}

	std::shared_ptr<i2p::stream::Stream> CreateStream (std::shared_ptr<i2p::client::ClientDestination> dest, const i2p::data::IdentHash& remote)
	{
		if (!dest) return nullptr;
		auto leaseSet = dest->FindLeaseSet (remote);
		if (leaseSet)
		{
			auto stream = dest->CreateStream (leaseSet);
			stream->Send (nullptr, 0); // connect
			return stream;
		}
		else
		{
			RequestLeaseSet (dest, remote);
			return nullptr;
		}
	}

	void AcceptStream (std::shared_ptr<i2p::client::ClientDestination> dest, const i2p::stream::StreamingDestination::Acceptor& acceptor)
	{
		if (dest)
			dest->AcceptStreams (acceptor);
	}

	void DestroyStream (std::shared_ptr<i2p::stream::Stream> stream)
	{
		if (stream)
			stream->Close ();
	}

    int GenerateIdentToFile (std::string& ident, const std::string& filename, uint8_t * sk,
                              i2p::data::SigningKeyType sigType, i2p::data::CryptoKeyType cryptoType)
    {
        size_t SK_LENGTH = 64; // 64 bytes
        size_t PADDING_LENGTH = 96;
        const std::string& fullPath = filename;    // filename is an absolute path
        i2p::data::PrivateKeys keys = i2p::data::PrivateKeys::CreateKeysBySk (sk, SK_LENGTH, sigType, cryptoType);

        std::ofstream f (fullPath, std::ofstream::binary | std::ofstream::out | std::ofstream::trunc);
        size_t len = keys.GetFullLen ();
        uint8_t * buf = new uint8_t[len];
        len = keys.ToBuffer (buf, len);
        memset(buf + 256, 0, PADDING_LENGTH);

        i2p::data::PrivateKeys ident_keys;
        if(!ident_keys.FromBuffer (buf, len))
        {
            return 1;
        }
        else
        {
            ident = ident_keys.GetPublic() -> GetIdentHash().ToBase32().append(".b32.i2p");
            f.write ((char *)buf, len);
            return 0;
        }
    }
}
}
