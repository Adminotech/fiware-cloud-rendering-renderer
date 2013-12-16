/**
    @author Admino Technologies Oy

    Copyright 2013 Admino Technologies Oy. All rights reserved.
    See LICENCE for conditions of distribution and use.

    @file   
    @brief   */

#pragma once

#include "talk/base/common.h"

namespace WebRTC
{   
    static std::string GetEnvVarOrDefault(const char* env_var_name, const char* default_value)
    {
        std::string value;
        const char* env_var = getenv(env_var_name);
        if (env_var)
            value = env_var;

        if (value.empty())
            value = default_value;

        return value;
    }

    static std::string GetPeerConnectionString()
    {
        return GetEnvVarOrDefault("WEBRTC_CONNECT", "stun:stun.l.google.com:19302");
    }

    static std::string GetDefaultServerName()
    {
        return GetEnvVarOrDefault("WEBRTC_SERVER", "localhost");
    }

    static std::string GetPeerName()
    {
        char computer_name[256];
        if (gethostname(computer_name, ARRAY_SIZE(computer_name)) != 0)
            strcpy(computer_name, "host");
        std::string ret(GetEnvVarOrDefault("USERNAME", "user"));
        ret += '@';
        ret += computer_name;
        return ret;
    }
}
