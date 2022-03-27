#pragma once

#include "sdkconfig.h"

// system includes
#include <string>
#include <array>
#include <optional>

// esp-idf includes
#include <esp_sntp.h>

// 3rdparty lib includes
#include <fmt/core.h>
#include <configmanager.h>
#include <configconstraints_base.h>
#include <configconstraints_espchrono.h>
#include <configwrapper.h>
#include <espwifiutils.h>
#include <espchrono.h>
#include <makearray.h>

// local includes

using namespace espconfig;

std::string defaultHostname();

class WiFiConfig
{
public:
    WiFiConfig(const char *ssidNvsKey, const char *keyNvsKey,
               const char *useStaticIpKey, const char *staticIpKey, const char *staticSubnetKey, const char *staticGatewayKey,
               const char *useStaticDnsKey, const char *staticDns0Key, const char *staticDns1Key, const char *staticDns2Key) :
        ssid         {std::string{},              DoReset, StringMaxSize<32>,                              ssidNvsKey         },
        key          {std::string{},              DoReset, StringOr<StringEmpty, StringMinMaxSize<8, 64>>, keyNvsKey          },
        useStaticIp  {false,                      DoReset, {},                                             useStaticIpKey     },
        staticIp     {wifi_stack::ip_address_t{}, DoReset, {},                                             staticIpKey        },
        staticSubnet {wifi_stack::ip_address_t{}, DoReset, {},                                             staticSubnetKey    },
        staticGateway{wifi_stack::ip_address_t{}, DoReset, {},                                             staticGatewayKey   },
        useStaticDns {false,                      DoReset, {},                                             useStaticDnsKey    },
        staticDns0   {wifi_stack::ip_address_t{}, DoReset, {},                                             staticDns0Key      },
        staticDns1   {wifi_stack::ip_address_t{}, DoReset, {},                                             staticDns1Key      },
        staticDns2   {wifi_stack::ip_address_t{}, DoReset, {},                                             staticDns2Key      }
    {}

    ConfigWrapper<std::string> ssid;
    ConfigWrapper<std::string> key;
    ConfigWrapper<bool> useStaticIp;
    ConfigWrapper<wifi_stack::ip_address_t> staticIp;
    ConfigWrapper<wifi_stack::ip_address_t> staticSubnet;
    ConfigWrapper<wifi_stack::ip_address_t> staticGateway;
    ConfigWrapper<bool> useStaticDns;
    ConfigWrapper<wifi_stack::ip_address_t> staticDns0;
    ConfigWrapper<wifi_stack::ip_address_t> staticDns1;
    ConfigWrapper<wifi_stack::ip_address_t> staticDns2;
};

class ConfigContainer
{
    using mac_t = wifi_stack::mac_t;

public:
    //                                            default                                 allowReset constraints                    nvsName
    ConfigWrapper<std::optional<mac_t>> baseMacAddressOverride{std::nullopt,              DoReset,   {},                            "baseMacAddrOver"     };
    ConfigWrapper<std::string> hostname           {defaultHostname,                       DoReset,   StringMinMaxSize<4, 32>,       "hostname"            };

#ifdef CONFIG_ETH_ENABLED
    ConfigWrapper<bool> ethEnabled                {true,                                   DoReset,   {},                         "ethEnabled"          };
    ConfigWrapper<bool> ethUseStaticIp            {false,                                  DoReset,   {},                         "ethUseStatIp"        };
    ConfigWrapper<ip_address_t> ethStaticIp       {ip_address_t{},                         DoReset,   {},                         "ethStaticIp"         };
    ConfigWrapper<ip_address_t> ethStaticSubnet   {ip_address_t{},                         DoReset,   {},                         "ethStaticSub"        };
    ConfigWrapper<ip_address_t> ethStaticGateway  {ip_address_t{},                         DoReset,   {},                         "ethStaticGw"         };
    ConfigWrapper<bool> ethUseStaticDns           {false,                                  DoReset,   {},                         "ethUseStatDns"       };
    ConfigWrapper<ip_address_t> ethStaticDns0     {ip_address_t{},                         DoReset,   {},                         "ethStaticDns0"       };
    ConfigWrapper<ip_address_t> ethStaticDns1     {ip_address_t{},                         DoReset,   {},                         "ethStaticDns1"       };
    ConfigWrapper<ip_address_t> ethStaticDns2     {ip_address_t{},                         DoReset,   {},                         "ethStaticDns2"       };
#endif

    ConfigWrapper<bool>        wifiStaEnabled     {true,                                  DoReset,   {},                            "wifiStaEnabled"      };
    std::array<WiFiConfig, 10> wifi_configs {
        WiFiConfig {"wifi_ssid0", "wifi_key0", "wifi_usestatic0", "wifi_static_ip0", "wifi_stati_sub0", "wifi_stat_gate0", "wifi_usestadns0", "wifi_stat_dnsA0", "wifi_stat_dnsB0", "wifi_stat_dnsC0"},
        WiFiConfig {"wifi_ssid1", "wifi_key1", "wifi_usestatic1", "wifi_static_ip1", "wifi_stati_sub1", "wifi_stat_gate1", "wifi_usestadns1", "wifi_stat_dnsA1", "wifi_stat_dnsB1", "wifi_stat_dnsC1"},
        WiFiConfig {"wifi_ssid2", "wifi_key2", "wifi_usestatic2", "wifi_static_ip2", "wifi_stati_sub2", "wifi_stat_gate2", "wifi_usestadns2", "wifi_stat_dnsA2", "wifi_stat_dnsB2", "wifi_stat_dnsC2"},
        WiFiConfig {"wifi_ssid3", "wifi_key3", "wifi_usestatic3", "wifi_static_ip3", "wifi_stati_sub3", "wifi_stat_gate3", "wifi_usestadns3", "wifi_stat_dnsA3", "wifi_stat_dnsB3", "wifi_stat_dnsC3"},
        WiFiConfig {"wifi_ssid4", "wifi_key4", "wifi_usestatic4", "wifi_static_ip4", "wifi_stati_sub4", "wifi_stat_gate4", "wifi_usestadns4", "wifi_stat_dnsA4", "wifi_stat_dnsB4", "wifi_stat_dnsC4"},
        WiFiConfig {"wifi_ssid5", "wifi_key5", "wifi_usestatic5", "wifi_static_ip5", "wifi_stati_sub5", "wifi_stat_gate5", "wifi_usestadns5", "wifi_stat_dnsA5", "wifi_stat_dnsB5", "wifi_stat_dnsC5"},
        WiFiConfig {"wifi_ssid6", "wifi_key6", "wifi_usestatic6", "wifi_static_ip6", "wifi_stati_sub6", "wifi_stat_gate6", "wifi_usestadns6", "wifi_stat_dnsA6", "wifi_stat_dnsB6", "wifi_stat_dnsC6"},
        WiFiConfig {"wifi_ssid7", "wifi_key7", "wifi_usestatic7", "wifi_static_ip7", "wifi_stati_sub7", "wifi_stat_gate7", "wifi_usestadns7", "wifi_stat_dnsA7", "wifi_stat_dnsB7", "wifi_stat_dnsC7"},
        WiFiConfig {"wifi_ssid8", "wifi_key8", "wifi_usestatic8", "wifi_static_ip8", "wifi_stati_sub8", "wifi_stat_gate8", "wifi_usestadns8", "wifi_stat_dnsA8", "wifi_stat_dnsB8", "wifi_stat_dnsC8"},
        WiFiConfig {"wifi_ssid9", "wifi_key9", "wifi_usestatic9", "wifi_static_ip9", "wifi_stati_sub9", "wifi_stat_gate9", "wifi_usestadns9", "wifi_stat_dnsA9", "wifi_stat_dnsB9", "wifi_stat_dnsC9"}
    };
    ConfigWrapper<int8_t>      wifiStaMinRssi     {-90,                                    DoReset,   {},                           "wifiStaMinRssi"      };

    ConfigWrapper<bool>        wifiApEnabled      {true,                                   DoReset,   {},                           "wifiApEnabled"       };
    ConfigWrapper<std::string> wifiApName         {defaultHostname,                        DoReset,   StringMinMaxSize<4, 32>,      "wifiApName"          };
    ConfigWrapper<std::string> wifiApKey          {"Passwort_123",                         DoReset,   StringOr<StringEmpty, StringMinMaxSize<8, 64>>, "wifiApKey" };
    ConfigWrapper<wifi_stack::ip_address_t> wifiApIp{wifi_stack::ip_address_t{10, 0, 0, 1},DoReset,   {},                           "wifiApIp"            };
    ConfigWrapper<wifi_stack::ip_address_t> wifiApMask{wifi_stack::ip_address_t{255, 255, 255, 0},DoReset, {},                      "wifiApMask"          };
    ConfigWrapper<uint8_t>     wifiApChannel      {1,                                      DoReset,   MinMaxValue<uint8_t, 1, 14>,  "wifiApChannel"       };
    ConfigWrapper<wifi_auth_mode_t> wifiApAuthmode{WIFI_AUTH_WPA2_PSK,                     DoReset,   {},                           "wifiApAuthmode"      };

    ConfigWrapper<bool>     timeServerEnabled     {true,                                   DoReset,   {},                           "timeServerEnabl"     };
    ConfigWrapper<std::string>   timeServer       {"europe.pool.ntp.org",                  DoReset,   StringMaxSize<64>,            "timeServer"          };
    ConfigWrapper<sntp_sync_mode_t> timeSyncMode  {SNTP_SYNC_MODE_IMMED,                   DoReset,   {},                           "timeSyncMode"        };
    ConfigWrapper<espchrono::milliseconds32> timeSyncInterval{espchrono::milliseconds32{CONFIG_LWIP_SNTP_UPDATE_DELAY}, DoReset, MinTimeSyncInterval, "timeSyncInterva" };
    ConfigWrapper<espchrono::minutes32> timezoneOffset{espchrono::minutes32{60},           DoReset,   {},                           "timezoneOffset"      }; // MinMaxValue<minutes32, -1440m, 1440m>
    ConfigWrapper<espchrono::DayLightSavingMode>timeDst{espchrono::DayLightSavingMode::EuropeanSummerTime, DoReset, {},             "time_dst"            };

    ConfigWrapper<std::string> otaUrl             {std::string{},                          DoReset,   StringOr<StringEmpty, StringValidUrl>, "otaUrl"   };

    template<typename T>
    void callForEveryConfig(T &&callable)
    {
#define REGISTER_CONFIG(name) \
        if (callable(name)) return;

        REGISTER_CONFIG(baseMacAddressOverride)
        REGISTER_CONFIG(hostname)

#ifdef CONFIG_ETH_ENABLED
        REGISTER_CONFIG(ethEnabled)
        REGISTER_CONFIG(ethUseStaticIp)
        REGISTER_CONFIG(ethStaticIp)
        REGISTER_CONFIG(ethStaticSubnet)
        REGISTER_CONFIG(ethStaticGateway)
        REGISTER_CONFIG(ethUseStaticDns)
        REGISTER_CONFIG(ethStaticDns0)
        REGISTER_CONFIG(ethStaticDns1)
        REGISTER_CONFIG(ethStaticDns2)
#endif

        REGISTER_CONFIG(wifiStaEnabled)

        for (auto &entry : wifi_configs)
        {
            REGISTER_CONFIG(entry.ssid)
            REGISTER_CONFIG(entry.key)
            REGISTER_CONFIG(entry.useStaticIp)
            REGISTER_CONFIG(entry.staticIp)
            REGISTER_CONFIG(entry.staticSubnet)
            REGISTER_CONFIG(entry.staticGateway)
            REGISTER_CONFIG(entry.useStaticDns)
            REGISTER_CONFIG(entry.staticDns0)
            REGISTER_CONFIG(entry.staticDns1)
            REGISTER_CONFIG(entry.staticDns2)
        }

        REGISTER_CONFIG(wifiStaMinRssi)

        REGISTER_CONFIG(wifiApEnabled)
        REGISTER_CONFIG(wifiApName)
        REGISTER_CONFIG(wifiApKey)
        REGISTER_CONFIG(wifiApIp)
        REGISTER_CONFIG(wifiApMask)
        REGISTER_CONFIG(wifiApChannel)
        REGISTER_CONFIG(wifiApAuthmode)

        REGISTER_CONFIG(timeServerEnabled)
        REGISTER_CONFIG(timeServer)
        REGISTER_CONFIG(timeSyncMode)
        REGISTER_CONFIG(timeSyncInterval)
        REGISTER_CONFIG(timezoneOffset)
        REGISTER_CONFIG(timeDst)

        REGISTER_CONFIG(otaUrl)

#undef REGISTER_API_VALUE
    }
};

extern ConfigManager<ConfigContainer> configs;
