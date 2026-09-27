
// InputPointsDlg.h: 控制点坐标输入对话框
//

#pragma once

// 这个头文件里用到了 IDD_INPUTPOINTS、IDC_EDIT_X0 等资源 ID，
// 必须自己把 resource.h 拉进来，否则谁先包含它谁就报「未声明的标识符」。
#include "resource.h"
#include "CurveDef.h"

// ---------------------------------------------------------------------------
// 控制点坐标输入对话框。
//
// 表格形式：每个控制点一行，x / y / z 三个输入框。
// 点数可以在 CurveDef::MIN_COUNT ~ CurveDef::MAX_COUNT 之间改，
// 行数和对话框高度会跟着自动增减 —— 改点数不用关窗口，输入框失焦前就生效。
//
// 用法：
//     CInputPointsDlg dlg(this);
//     dlg.m_nCount = 视图当前的个数;
//     把视图当前的坐标拷进 dlg.m_pt;
//     if (dlg.DoModal() == IDOK)
//     {
//         从 dlg.m_nCount / dlg.m_pt 取回结果;
//     }
// ---------------------------------------------------------------------------
class CInputPointsDlg : public CDialog
{
	DECLARE_DYNAMIC(CInputPointsDlg)

public:
	// 输入 / 输出：控制点。调用前填上当前值，DoModal 返回 IDOK 后取回新值
	float m_pt[CurveDef::MAX_COUNT][3];
	int   m_nCount;

	CInputPointsDlg(CWnd* pParent = NULL);
	virtual ~CInputPointsDlg();

	enum { IDD = IDD_INPUTPOINTS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void OnOK();

	afx_msg void OnCountChange();     // “控制点个数”输入框内容变了
	afx_msg void OnResetDefault();    // 点“恢复默认”按钮
	DECLARE_MESSAGE_MAP()

private:
	// ---- 和资源文件 IDD_INPUTPOINTS 里的版面尺寸对应 ----
	enum
	{
		ROW_STEP_DLU = 13,   // 相邻两行的间距（对话框单位）
		FOOT_COUNT   = 6     // 底部要跟着上下挪的控件个数
	};

	BOOL  m_bReady;              // OnInitDialog 完成前不响应输入框的通知
	UINT  m_nFootId[FOOT_COUNT]; // 底部控件的 ID
	CRect m_rcFoot[FOOT_COUNT];  // 它们在“满 MAX_COUNT 行”时的位置（客户区坐标）
	int   m_nBaseHeight;         // 满 MAX_COUNT 行时对话框的高度（像素）
	int   m_nRowStepPx;          // 一行有多高（像素）

	int  CountFromEdit();        // 读“控制点个数”输入框
	void ShowRows(int n);        // 只显示前 n 行，其余隐藏
	void Relayout();             // 按当前点数缩放对话框、上移底部控件
	void FillEdits();            // 把 m_pt / m_nCount 写进各个输入框
	BOOL ReadEdits();            // 把输入框读回 m_pt，有非法输入就提示并返回 FALSE
};
