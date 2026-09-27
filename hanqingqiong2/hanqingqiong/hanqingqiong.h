
// hanqingqiong.h: hanqingqiong 应用程序的主头文件
//
#pragma once

#ifndef __AFXWIN_H__
	#error "在包含此文件之前包含 'pch.h' 以生成 PCH"
#endif

#include "resource.h"       // 主符号


// ChanqingqiongApp:
// 有关此类的实现，请参阅 hanqingqiong.cpp
//

class ChanqingqiongApp : public CWinApp
{
public:
	ChanqingqiongApp() noexcept;


// 重写
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

// 实现
	afx_msg void OnAppAbout();
	DECLARE_MESSAGE_MAP()
};

extern ChanqingqiongApp theApp;
