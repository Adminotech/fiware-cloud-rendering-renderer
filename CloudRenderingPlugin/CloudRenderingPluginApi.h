/**
    @author Admino Technologies Oy

    Copyright 2013 Admino Technologies Oy. All rights reserved.
    See LICENCE for conditions of distribution and use.

    @file   
    @brief   */

#pragma once

#if defined (_WINDOWS)
#if defined(CloudRenderingPlugin_EXPORTS) 
#define CLOUDRENDERING_API __declspec(dllexport)
#else
#define CLOUDRENDERING_API __declspec(dllimport) 
#endif
#else
#define CLOUDRENDERING_API
#endif
