
// InputPointsDlg.cpp: 控制点坐标输入对话框的实现
//

#include "pch.h"
#include "framework.h"
#include "resource.h"
#include "CurveDef.h"
#include "InputPointsDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


IMPLEMENT_DYNAMIC(CInputPointsDlg, CDialog)

BEGIN_MESSAGE_MAP(CInputPointsDlg, CDialog)
	ON_EN_CHANGE(IDC_EDIT_COUNT, &CInputPointsDlg::OnCountChange)
	ON_BN_CLICKED(IDC_BTN_RESET, &CInputPointsDlg::OnResetDefault)
END_MESSAGE_MAP()


CInputPointsDlg::CInputPointsDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CInputPointsDlg::IDD, pParent)
{
	// 调用者忘了填的话，至少是一份合法的默认曲线
	m_nCount = CurveDef::DEFAULT_COUNT;
	for (int i = 0; i < CurveDef::MAX_COUNT; i++)
	{
		for (int k = 0; k < 3; k++)
		{
			m_pt[i][k] = (i < CurveDef::DEFAULT_COUNT) ? CurveDef::DEFAULT_POINTS[i][k] : 0.0f;
		}
	}

	m_bReady = FALSE;
	m_nBaseHeight = 0;
	m_nRowStepPx = 1;
	for (int i = 0; i < FOOT_COUNT; i++)
	{
		m_nFootId[i] = 0;
		m_rcFoot[i] = CRect(0, 0, 0, 0);
	}
}

CInputPointsDlg::~CInputPointsDlg()
{
}

void CInputPointsDlg::DoDataExchange(CDataExchange* pDX)
{
	// 这里不用 DDX，坐标是循环取控件值，走 FillEdits / ReadEdits 更直接
	CDialog::DoDataExchange(pDX);
}

BOOL CInputPointsDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// ---------------- 1. 记下底部控件在“满 MAX_COUNT 行”时的位置 ----------------
	// 以后每次重排都从这个基线算，反复加行减行也不会越挪越偏
	static const UINT footId[FOOT_COUNT] =
	{
		IDC_STATIC_COUNT_LABEL, IDC_EDIT_COUNT, IDC_STATIC_COUNT_HINT,
		IDOK, IDCANCEL, IDC_BTN_RESET
	};

	for (int i = 0; i < FOOT_COUNT; i++)
	{
		m_nFootId[i] = footId[i];

		CRect rc(0, 0, 0, 0);
		CWnd* pWnd = GetDlgItem(footId[i]);
		if (pWnd != NULL)
		{
			pWnd->GetWindowRect(&rc);
			ScreenToClient(&rc);
		}
		m_rcFoot[i] = rc;
	}

	// ---------------- 2. 把版面尺寸从对话框单位换算成像素 ----------------
	// 资源文件里一行占 ROW_STEP_DLU 个对话框单位，这里问系统它等于多少像素
	CRect rcStep(0, 0, 0, ROW_STEP_DLU);
	MapDialogRect(&rcStep);
	m_nRowStepPx = rcStep.Height();
	if (m_nRowStepPx < 1)
	{
		m_nRowStepPx = 1;
	}

	CRect rcDlg;
	GetWindowRect(&rcDlg);
	m_nBaseHeight = rcDlg.Height();

	// ---------------- 3. 其它初始化 ----------------
	CEdit* pEditCount = (CEdit*)GetDlgItem(IDC_EDIT_COUNT);
	if (pEditCount != NULL)
	{
		pEditCount->SetLimitText(2);        // 最多两位数，装得下 10
	}

	if (m_nCount < CurveDef::MIN_COUNT || m_nCount > CurveDef::MAX_COUNT)
	{
		m_nCount = CurveDef::DEFAULT_COUNT;
	}

	m_bReady = FALSE;                       // 摆版面期间先屏蔽 EN_CHANGE

	// ---------------- 4. 按当前点数摆一次 ----------------
	FillEdits();
	ShowRows(m_nCount);
	Relayout();

	CenterWindow(GetParent());

	m_bReady = TRUE;                        // 从这一刻起才响应用户的输入
	return TRUE;                            // 除非把焦点设给某个控件，否则返回 TRUE
}

// 点“确定”：先把输入框读一遍，有错就不关窗口
void CInputPointsDlg::OnOK()
{
	if (!ReadEdits())
	{
		return;
	}
	CDialog::OnOK();
}

// “控制点个数”变了：悄悄调整行数，非法值先不吭声（等按确定时再统一报错）
void CInputPointsDlg::OnCountChange()
{
	if (!m_bReady)
	{
		return;
	}

	int n = CountFromEdit();
	if (n < CurveDef::MIN_COUNT || n > CurveDef::MAX_COUNT || n == m_nCount)
	{
		return;                             // 还在输入途中，或者压根没变
	}

	m_nCount = n;
	ShowRows(m_nCount);
	Relayout();
}

// “恢复默认”：把默认曲线的四个点填回去
void CInputPointsDlg::OnResetDefault()
{
	for (int i = 0; i < CurveDef::MAX_COUNT; i++)
	{
		for (int k = 0; k < 3; k++)
		{
			m_pt[i][k] = (i < CurveDef::DEFAULT_COUNT) ? CurveDef::DEFAULT_POINTS[i][k] : 0.0f;
		}
	}
	m_nCount = CurveDef::DEFAULT_COUNT;

	FillEdits();
	ShowRows(m_nCount);
	Relayout();
}

// ---------------------------------------------------------------------------
// 读“控制点个数”输入框
// ---------------------------------------------------------------------------
int CInputPointsDlg::CountFromEdit()
{
	CString s;
	GetDlgItemText(IDC_EDIT_COUNT, s);
	s.Trim();
	return _ttoi(s);                        // 空串会得到 0，落在非法区间里
}

// ---------------------------------------------------------------------------
// 只显示前 n 行
// ---------------------------------------------------------------------------
void CInputPointsDlg::ShowRows(int n)
{
	for (int i = 0; i < CurveDef::MAX_COUNT; i++)
	{
		BOOL bShow = (i < n) ? TRUE : FALSE;

		const int rowId[4] =
		{
			IDC_STATIC_P0 + i, IDC_EDIT_X0 + i, IDC_EDIT_Y0 + i, IDC_EDIT_Z0 + i
		};

		for (int k = 0; k < 4; k++)
		{
			CWnd* pWnd = GetDlgItem(rowId[k]);
			if (pWnd != NULL)
			{
				pWnd->ShowWindow(bShow ? SW_SHOW : SW_HIDE);
				pWnd->EnableWindow(bShow);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// 按当前点数把对话框“变矮”：底部控件整体上移，窗口高度同步缩小
// 位置一律从基线重算，所以是幂等的，来回改点数不会漂移
// ---------------------------------------------------------------------------
void CInputPointsDlg::Relayout()
{
	int dy = (CurveDef::MAX_COUNT - m_nCount) * m_nRowStepPx;

	// 底部控件上移 dy 像素
	for (int i = 0; i < FOOT_COUNT; i++)
	{
		CWnd* pWnd = GetDlgItem(m_nFootId[i]);
		if (pWnd != NULL)
		{
			pWnd->SetWindowPos(NULL,
				m_rcFoot[i].left, m_rcFoot[i].top - dy,
				0, 0,
				SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
		}
	}

	// 窗口本身也缩掉同样多，这样下边距看起来没变
	CRect rcDlg;
	GetWindowRect(&rcDlg);
	int nHeight = m_nBaseHeight - dy;
	if (nHeight < 1)
	{
		nHeight = 1;
	}
	SetWindowPos(NULL, 0, 0, rcDlg.Width(), nHeight,
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// ---------------------------------------------------------------------------
// 把 m_pt / m_nCount 写进输入框
// ---------------------------------------------------------------------------
void CInputPointsDlg::FillEdits()
{
	CString s;

	for (int i = 0; i < CurveDef::MAX_COUNT; i++)
	{
		s.Format(_T("%g"), m_pt[i][0]);
		SetDlgItemText(IDC_EDIT_X0 + i, s);
		s.Format(_T("%g"), m_pt[i][1]);
		SetDlgItemText(IDC_EDIT_Y0 + i, s);
		s.Format(_T("%g"), m_pt[i][2]);
		SetDlgItemText(IDC_EDIT_Z0 + i, s);
	}

	s.Format(_T("%d"), m_nCount);
	SetDlgItemText(IDC_EDIT_COUNT, s);
}

// ---------------------------------------------------------------------------
// 把输入框读回 m_pt；碰到非法输入就弹一下提示并返回 FALSE
// ---------------------------------------------------------------------------
BOOL CInputPointsDlg::ReadEdits()
{
	int n = CountFromEdit();
	if (n < CurveDef::MIN_COUNT || n > CurveDef::MAX_COUNT)
	{
		CString msg;
		msg.Format(_T("控制点个数必须是 %d ~ %d 之间的整数。"),
			CurveDef::MIN_COUNT, CurveDef::MAX_COUNT);
		MessageBox(msg, _T("输入有误"), MB_ICONWARNING | MB_OK);
		GetDlgItem(IDC_EDIT_COUNT)->SetFocus();
		return FALSE;
	}
	m_nCount = n;

	CString s;
	double v;

	for (int i = 0; i < m_nCount; i++)
	{
		for (int k = 0; k < 3; k++)
		{
			int nId = (k == 0) ? (IDC_EDIT_X0 + i)
				: (k == 1) ? (IDC_EDIT_Y0 + i)
				: (IDC_EDIT_Z0 + i);

			GetDlgItemText(nId, s);
			s.Trim();

			if (s.IsEmpty() || _stscanf_s(s, _T("%lf"), &v) != 1)
			{
				LPCTSTR pszAxis = (k == 0) ? _T("x") : (k == 1) ? _T("y") : _T("z");

				CString msg;
				msg.Format(_T("第 %d 个控制点 P%d 的 %s 坐标不是合法数字，请检查。"),
					i + 1, i, pszAxis);
				MessageBox(msg, _T("输入有误"), MB_ICONWARNING | MB_OK);
				GetDlgItem(nId)->SetFocus();
				return FALSE;
			}

			m_pt[i][k] = (float)v;
		}
	}

	return TRUE;
}
