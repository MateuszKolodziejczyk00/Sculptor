#pragma once


#if USE_WIN32

namespace spt::platf
{

class Win32Window;

using PlatformWindow = Win32Window;


} // spt::plat

#endif // USE_WIN32
