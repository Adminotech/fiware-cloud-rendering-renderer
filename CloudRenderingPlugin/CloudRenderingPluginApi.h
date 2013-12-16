
// For conditions of distribution and use, see copyright notice in LICENSE

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
