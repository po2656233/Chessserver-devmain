#pragma once

//////////////////////////////////////////////////////////////////////////////////

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif

#ifndef WINVER
#define WINVER 0x0501
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS 0x0410
#endif

#ifndef _WIN32_IE
#define _WIN32_IE 0x0400
#endif

#define _ATL_ATTRIBUTES
#define _AFX_ALL_WARNINGS
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS

//////////////////////////////////////////////////////////////////////////////////

//MFC 文件
#include <AfxWin.h>
#include <AfxExt.h>
#include <AfxCmn.h>
#include <AfxDisp.h>

//////////////////////////////////////////////////////////////////////////////////
//链接代码

//系统链接
#pragma comment(lib,"VFW32.lib")
//	#pragma comment (lib,"WHImage.lib")
//	#pragma comment (lib,"FGuilib.lib")
//	#pragma comment (lib,"DownLoad.lib")
//	#pragma comment (lib,"ServiceCore.lib")
//	#pragma comment (lib,"SkinControl.lib")
//	#pragma comment (lib,"ShareControl.lib")
//	#pragma comment (lib,"FGuimfcFactory.lib")	
//	#pragma comment (lib,"PlatformData.lib")

//////////////////////////////////////////////////////////////////////////////////
