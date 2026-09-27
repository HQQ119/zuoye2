
// hanqingqiongView.cpp: ChanqingqiongView 类的实现
//

#include "pch.h"
#include "framework.h"
#include <math.h>
// SHARED_HANDLERS 可以在实现预览、缩略图和搜索筛选器句柄的
// ATL 项目中进行定义，并允许与该项目共享文档代码。
#ifndef SHARED_HANDLERS
#include "hanqingqiong.h"
#endif

#include "hanqingqiongDoc.h"
#include "hanqingqiongView.h"

// 这两个要放在 hanqingqiong.h 之后：resource.h 是由 hanqingqiong.h 引出来的，
// 而 InputPointsDlg.h 里用到了 resource.h 中的 IDD_INPUTPOINTS。
#include "CurveDef.h"
#include "InputPointsDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// ===========================================================================
//                               可调参数
// ===========================================================================

static const double kPI = 3.14159265358979323846;

static const int kSegCount = 50;        // 画曲线时离散成多少段（越大越光滑）
static const int kFitSamples = 64;      // 估算“曲线有多大”时在曲线上采多少点

// ---- 观察手感 -------------------------------------------------------------
static const float kRotStep      = 0.5f;   // 鼠标每移动 1 像素转多少度
static const float kZoomRate     = 0.88f;  // 滚轮每滚一格的缩放倍率（乘性，手感更匀）
static const float kFitMargin    = 1.02f;  // 相机距离相对“刚好装下”的余量
static const float kZoomInRatio  = 0.55f;  // 最近能推到合适距离的多少倍
static const float kZoomOutRatio = 4.00f;  // 最远能拉到合适距离的多少倍
static const int   kClickSlop    = 4;      // 位移不超过这么多像素就算“单击”而不是“拖拽”
static const float kDefaultRotX  = 18.0f;  // 启动时的俯仰角
static const float kDefaultRotY  = -24.0f; // 启动时的偏转角

// ---- 投影参数 -------------------------------------------------------------
static const float kFovY = 45.0f;          // 垂直视场角


// ChanqingqiongView

IMPLEMENT_DYNCREATE(ChanqingqiongView, CView)

BEGIN_MESSAGE_MAP(ChanqingqiongView, CView)
	// 标准打印命令
	ON_COMMAND(ID_FILE_PRINT, &CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_DIRECT, &CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_PREVIEW, &CView::OnFilePrintPreview)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	// 三维旋转用的鼠标 / 键盘消息
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_RBUTTONDOWN()
	ON_WM_MOUSEWHEEL()
	ON_WM_KEYDOWN()
	ON_WM_SETCURSOR()
	// 曲线菜单命令
	ON_COMMAND(ID_CURVE_INPUT_POINTS, &ChanqingqiongView::OnInputPoints)
	ON_COMMAND(ID_CURVE_PICK_POINTS, &ChanqingqiongView::OnPickPoints)
	ON_COMMAND(ID_CURVE_RESET_POINTS, &ChanqingqiongView::OnResetPoints)
	ON_UPDATE_COMMAND_UI(ID_CURVE_PICK_POINTS, &ChanqingqiongView::OnUpdatePickPoints)
END_MESSAGE_MAP()

// ChanqingqiongView 构造/析构

ChanqingqiongView::ChanqingqiongView() noexcept
{
	// TODO: 在此处添加构造代码
	this->m_GLPixelIndex = 0;
	this->m_hGLContext = NULL;
	m_nWidth = 0;
	m_nHeight = 0;

	// 曲线：先摆上默认那四个点
	m_nCtrlCount = 0;
	ResetCtrlPoints();

	// 观察参数
	m_fRotX = kDefaultRotX;
	m_fRotY = kDefaultRotY;
	m_fCamDist = 10.0f;
	m_fFitDist = 10.0f;
	m_fGridHalf = 4.0f;
	m_center[0] = 0.0f;
	m_center[1] = 0.0f;
	m_center[2] = 0.0f;
	m_bDragging = FALSE;
	m_ptLastMouse = CPoint(0, 0);
	m_ptDown = CPoint(0, 0);
	m_bShowGrid = TRUE;
	m_bShowAxes = TRUE;

	m_bPickMode = FALSE;
	m_nPickIndex = 0;

	// 按默认曲线算出旋转中心和合适的相机距离
	FitViewToPoints();
}

ChanqingqiongView::~ChanqingqiongView()
{
}

BOOL ChanqingqiongView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: 在此处通过修改
	//  CREATESTRUCT cs 来修改窗口类或样式
	cs.style |= (WS_CLIPCHILDREN | WS_CLIPSIBLINGS);

	return CView::PreCreateWindow(cs);
}

// ===========================================================================
//                            数据：控制点
// ===========================================================================

// 恢复默认控制点
void ChanqingqiongView::ResetCtrlPoints(void)
{
	for (int i = 0; i < CurveDef::MAX_COUNT; i++)
	{
		for (int k = 0; k < 3; k++)
		{
			m_ctrl[i][k] = (i < CurveDef::DEFAULT_COUNT) ? CurveDef::DEFAULT_POINTS[i][k] : 0.0f;
		}
	}
	m_nCtrlCount = CurveDef::DEFAULT_COUNT;

	m_bPickMode = FALSE;
	m_nPickIndex = 0;
}

// ---------------------------------------------------------------------------
// 按当前控制点重算：旋转中心、刚好能装下的相机距离、参考网格尺寸
// 用户输入坐标之后曲线可能跑到别处去了，就是靠这里把视野重新对准的
// ---------------------------------------------------------------------------
void ChanqingqiongView::FitViewToPoints(void)
{
	if (m_nCtrlCount < 1)
	{
		return;
	}

	// 1) 用控制点包围盒的中心当旋转中心
	float lo[3], hi[3];
	for (int k = 0; k < 3; k++)
	{
		lo[k] = m_ctrl[0][k];
		hi[k] = m_ctrl[0][k];
	}
	for (int i = 1; i < m_nCtrlCount; i++)
	{
		for (int k = 0; k < 3; k++)
		{
			if (m_ctrl[i][k] < lo[k]) lo[k] = m_ctrl[i][k];
			if (m_ctrl[i][k] > hi[k]) hi[k] = m_ctrl[i][k];
		}
	}
	for (int k = 0; k < 3; k++)
	{
		m_center[k] = 0.5f * (lo[k] + hi[k]);
	}

	// 2) 在曲线上采样，量出它到旋转中心的最大距离，当作包围球半径。
	//    注意是量“曲线”而不是量控制点：控制点的凸包比曲线胖一圈，
	//    拿控制点算会让相机退得太远，曲线在屏幕上显得很小。
	float r = 0.0f;
	float p[3];
	for (int i = 0; i <= kFitSamples; i++)
	{
		EvalCurve((float)i / (float)kFitSamples, p);

		float dx = p[0] - m_center[0];
		float dy = p[1] - m_center[1];
		float dz = p[2] - m_center[2];
		float d = sqrtf(dx * dx + dy * dy + dz * dz);
		if (d > r)
		{
			r = d;
		}
	}
	if (r < 0.01f)
	{
		r = 0.01f;
	}

	// 3) 半径 r 的包围球要整个落进垂直视野，需要 D ≥ r / sin(fovY / 2)。
	//    按这个距离摆相机，曲线转到任何角度都刚好在画面里，不会被切掉。
	float sn = (float)sin(kPI * (double)kFovY / 360.0);
	m_fFitDist = r / sn * kFitMargin;
	if (m_fFitDist < 0.05f)
	{
		m_fFitDist = 0.05f;
	}

	// 4) 参考网格取一个“整”的尺寸（1、2、4、8 …）而且格数固定，
	//    这样不管曲线多大多小，网格看起来密度都差不多
	float half = 1.0f;
	while (half < r * 1.3f)
	{
		half *= 2.0f;
	}
	while (half > r * 5.0f && half > 0.02f)
	{
		half *= 0.5f;
	}
	m_fGridHalf = half;

	// 5) 换了曲线就重新对准，相机距离回到“正好装下”的位置
	m_fCamDist = m_fFitDist;
}

// 把相机距离夹到允许范围内。范围跟着曲线大小走，所以填很大的坐标也不会看不见
float ChanqingqiongView::ClampCamDist(float d) const
{
	float lo = m_fFitDist * kZoomInRatio;
	float hi = m_fFitDist * kZoomOutRatio;
	if (d < lo) d = lo;
	if (d > hi) d = hi;
	return d;
}

// ===========================================================================
//                                绘图
// ===========================================================================

// ---------------------------------------------------------------------------
// 用 de Casteljau（割角）递推算出参数 t 处的曲线点
// 每算一层，就把相邻控制点连成的线段按 t 割一刀、取割点；
// 割到最后只剩一个点，那个点就在曲线上。三维版，点数任意。
// ---------------------------------------------------------------------------
void ChanqingqiongView::EvalCurve(float t, float out[3]) const
{
	int n = m_nCtrlCount;
	if (n < 1)
	{
		out[0] = 0.0f;
		out[1] = 0.0f;
		out[2] = 0.0f;
		return;
	}

	// 先把控制点拷进临时数组，免得把原始控制点改坏了
	float temp[CurveDef::MAX_COUNT][3];
	for (int i = 0; i < n; i++)
	{
		temp[i][0] = m_ctrl[i][0];
		temp[i][1] = m_ctrl[i][1];
		temp[i][2] = m_ctrl[i][2];
	}

	////////////////////////////////////////// 割角
	for (int s = 1; s < n; s++)
	{
		for (int i = 0; i < n - s; i++)
		{
			temp[i][0] = (1 - t) * temp[i][0] + t * temp[i + 1][0];
			temp[i][1] = (1 - t) * temp[i][1] + t * temp[i + 1][1];
			temp[i][2] = (1 - t) * temp[i][2] + t * temp[i + 1][2];
		}
	}

	out[0] = temp[0][0];
	out[1] = temp[0][1];
	out[2] = temp[0][2];
}

// ---------------------------------------------------------------------------
// 把曲线离散成 kSegCount 段连起来画
// ---------------------------------------------------------------------------
void ChanqingqiongView::DrawScene(void)
{
	float p[3];

	glColor3f(1.0f, 0.0f, 0.0f);
	glLineWidth(3.0f);
	glBegin(GL_LINE_STRIP);
	for (int loop = 0; loop <= kSegCount; loop++)
	{
		EvalCurve((float)loop / (float)kSegCount, p);
		glVertex3f(p[0], p[1], p[2]);
	}
	glEnd();
}

// ---------------------------------------------------------------------------
// 画控制多边形和各个控制点
// 这两样东西是判断“曲线在空间里怎么弯”的最好参照物
// ---------------------------------------------------------------------------
void ChanqingqiongView::DrawControlPolygon(void)
{
	glDisable(GL_LINE_SMOOTH);      // 直线不用抗锯齿，画得利落些

	// 控制多边形：灰色细线
	glColor3f(0.55f, 0.55f, 0.60f);
	glLineWidth(1.0f);
	glBegin(GL_LINE_STRIP);
	for (int i = 0; i < m_nCtrlCount; i++)
	{
		glVertex3f(m_ctrl[i][0], m_ctrl[i][1], m_ctrl[i][2]);
	}
	glEnd();

	// 控制点：蓝色小方块。
	// 拾取模式下把“下一个等着放的那个点”画成橙色并且放大，一眼就知道该点哪儿了。
	for (int i = 0; i < m_nCtrlCount; i++)
	{
		BOOL bNext = (m_bPickMode && i == m_nPickIndex) ? TRUE : FALSE;

		glPointSize(bNext ? 12.0f : 7.0f);
		if (bNext)
		{
			glColor3f(1.00f, 0.55f, 0.00f);
		}
		else
		{
			glColor3f(0.10f, 0.30f, 0.85f);
		}

		glBegin(GL_POINTS);
		glVertex3f(m_ctrl[i][0], m_ctrl[i][1], m_ctrl[i][2]);
		glEnd();
	}

	glEnable(GL_LINE_SMOOTH);
}

// ---------------------------------------------------------------------------
// 画三条坐标轴：X 红、Y 绿、Z 蓝
// ---------------------------------------------------------------------------
void ChanqingqiongView::DrawAxes(void)
{
	if (!m_bShowAxes)
	{
		return;
	}

	float len = m_fGridHalf;

	glDisable(GL_LINE_SMOOTH);
	glLineWidth(1.5f);
	glBegin(GL_LINES);
		// X 轴
		glColor3f(0.85f, 0.20f, 0.20f);
		glVertex3f(0.0f, 0.0f, 0.0f);
		glVertex3f(len, 0.0f, 0.0f);
		// Y 轴
		glColor3f(0.15f, 0.65f, 0.20f);
		glVertex3f(0.0f, 0.0f, 0.0f);
		glVertex3f(0.0f, len, 0.0f);
		// Z 轴
		glColor3f(0.20f, 0.35f, 0.90f);
		glVertex3f(0.0f, 0.0f, 0.0f);
		glVertex3f(0.0f, 0.0f, len);
	glEnd();
	glEnable(GL_LINE_SMOOTH);
}

// ---------------------------------------------------------------------------
// 在 z = 0 平面上铺一层浅灰参考网格，位置跟着曲线走、大小自适应
// 它正好是原来那条二维曲线所在的平面，曲线是浮在网格上面还是扎到下面，
// 一眼就能看出来 —— 这是三维旋转时最重要的深度参照
// ---------------------------------------------------------------------------
void ChanqingqiongView::DrawGrid(void)
{
	if (!m_bShowGrid)
	{
		return;
	}

	const int kCells = 4;                       // 每边 4 格，共 9 条线
	float half = m_fGridHalf;
	float step = half / (float)kCells;
	float gx = m_center[0];
	float gy = m_center[1];

	glDisable(GL_LINE_SMOOTH);
	glLineWidth(1.0f);
	glColor3f(0.86f, 0.86f, 0.89f);
	glBegin(GL_LINES);
	for (int i = -kCells; i <= kCells; i++)
	{
		float v = step * (float)i;

		// 平行于 y 轴的线
		glVertex3f(gx + v, gy - half, 0.0f);
		glVertex3f(gx + v, gy + half, 0.0f);
		// 平行于 x 轴的线
		glVertex3f(gx - half, gy + v, 0.0f);
		glVertex3f(gx + half, gy + v, 0.0f);
	}
	glEnd();
	glEnable(GL_LINE_SMOOTH);
}

void ChanqingqiongView::OnDraw(CDC* pDC)
{
	ChanqingqiongDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		return;

	// TODO: 在此处为本机数据添加绘制代码
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// ------------------------- 三维观察变换 -------------------------
	// 这几步的顺序不能颠倒：先把旋转中心挪到原点 → 再旋转 → 最后把相机往后退。
	// 换算成矩阵就是 M = T(0,0,-距离) · Rx · Ry · T(-中心)，
	// 所以曲线是绕自己的中心转，怎么转都不会跑出视野。
	glTranslatef(0.0f, 0.0f, -m_fCamDist);            // 相机后退，距离由滚轮控制
	glRotatef(m_fRotX, 1.0f, 0.0f, 0.0f);             // 绕 X 轴转：鼠标上下拖
	glRotatef(m_fRotY, 0.0f, 1.0f, 0.0f);             // 绕 Y 轴转：鼠标左右拖
	glTranslatef(-m_center[0], -m_center[1], -m_center[2]);

	glEnable(GL_DEPTH_TEST);

	// 先画参照物，再画曲线，层次看起来更清楚
	DrawGrid();
	DrawAxes();
	DrawControlPolygon();
	DrawScene();

	SwapBuffers(pDC->m_hDC);
}

// ---------------------------------------------------------------------------
// 按客户区尺寸建立透视投影
// 透视比平行投影多一层“近大远小”，旋转时立体感强得多。
// 这里用 glFrustum 手工搭，效果等同于 gluPerspective(kFovY, aspect, near, far)；
// 近远平面跟着曲线大小走，所以坐标填得再大再小都不会被切掉。
// ---------------------------------------------------------------------------
void ChanqingqiongView::SetupProjection(int cx, int cy)
{
	if (m_hGLContext == NULL)   // OpenGL 上下文还没建好，先什么都不做
	{
		return;
	}

	if (cx < 1) cx = 1;         // 防止出现宽或高为 0 的退化投影
	if (cy < 1) cy = 1;

	glViewport(0, 0, cx, cy);

	double aspect = (double)cx / (double)cy;

	double nearZ = (double)m_fFitDist * 0.02;
	if (nearZ < 0.05)
	{
		nearZ = 0.05;
	}
	double farZ = (double)m_fFitDist * 12.0 + 10.0;

	double top = nearZ * tan(kPI * (double)kFovY / 360.0);
	double right = top * aspect;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-right, right, -top, top, nearZ, farZ);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

// ---------------------------------------------------------------------------
// 复位观察角度：回到程序刚启动时那个略微俯视、略微侧转的角度
// ---------------------------------------------------------------------------
void ChanqingqiongView::ResetView(void)
{
	m_fRotX = kDefaultRotX;
	m_fRotY = kDefaultRotY;
	m_fCamDist = m_fFitDist;
	Invalidate();
}

// ---------------------------------------------------------------------------
// 在状态栏显示一句话
// ---------------------------------------------------------------------------
void ChanqingqiongView::SetHint(LPCTSTR pszText)
{
	CFrameWnd* pFrame = GetParentFrame();
	if (pFrame != NULL)
	{
		pFrame->SetMessageText(pszText);
	}
}

// 更新点选进度提示
void ChanqingqiongView::UpdatePickHint(void)
{
	if (m_bPickMode)
	{
		CString s;
		s.Format(_T("点选控制点：请点第 %d 个（P%d），还剩 %d 个 ｜ 单击放点、拖拽仍是转视角、ESC 退出"),
			m_nPickIndex + 1, m_nPickIndex, m_nCtrlCount - m_nPickIndex);
		SetHint(s);
	}
	else
	{
		SetHint(_T("点选完成。"));
	}
}

// ===========================================================================
//                          屏幕像素 → 世界坐标
//
// 正投影那几行是（见 OnDraw）：
//     M = T(0,0,-D) · Rx(rotX) · Ry(rotY) · T(-center)
// 这里把它倒过来用：屏幕点先还原成眼坐标里的一条射线，再用 M 的逆变换送回世界坐标，
// 最后和水平面 z = zPlane 求交，交点就是鼠标指着的那个位置。
//
// 倒推过程（θx = rotX，θy = rotY，D = m_fCamDist）：
//     e = s · dir                          眼坐标下的射线
//     w = e + (0,0,D)
//     u = Rx(-θx) · w
//     p = center + Ry(-θy) · u
// 令 p.z = zPlane，解出 s 即可。
// ===========================================================================
BOOL ChanqingqiongView::PickPointOnZPlane(CPoint pt, float zPlane, float out[3])
{
	if (m_nWidth < 1 || m_nHeight < 1)
	{
		return FALSE;
	}

	double k = tan(kPI * (double)kFovY / 360.0);            // tan(fovY / 2)
	double aspect = (double)m_nWidth / (double)m_nHeight;

	// 1) 屏幕像素 → 归一化设备坐标。
	//    注意 y 要翻过来：视口原点在左下角，而窗口原点在左上角。
	double xn = 2.0 * (double)pt.x / (double)m_nWidth - 1.0;
	double yn = 1.0 - 2.0 * (double)pt.y / (double)m_nHeight;

	// 2) NDC → 眼坐标下的一条射线方向（深度先取 1，方向可任意缩放）
	double dx = xn * k * aspect;
	double dy = yn * k;
	double dz = -1.0;

	double tx = (double)m_fRotX * kPI / 180.0;
	double ty = (double)m_fRotY * kPI / 180.0;
	double cx = cos(tx), sx = sin(tx);
	double cy = cos(ty), sy = sin(ty);

	// 3) 平面 z = zPlane 在眼坐标里的法向是 (sinθy, -sinθx·cosθy, cosθx·cosθy)。
	//    射线和它平行时（也就是这个平面几乎侧对着屏幕）没有唯一交点，只能放弃。
	double denom = dx * sy - dy * sx * cy + dz * cx * cy;
	if (fabs(denom) < 1e-6)
	{
		return FALSE;
	}

	double D = (double)m_fCamDist;
	double s = ((double)zPlane - (double)m_center[2] - D * cx * cy) / denom;

	// s 就是射线上那个点到相机的距离，太近或太远都不可信
	if (s < 0.05 || s > D * 60.0)
	{
		return FALSE;
	}

	// 4) 沿射线走 s，再一步步逆变换回世界坐标
	double wx = s * dx;
	double wy = s * dy;
	double wz = s * dz + D;                 // 补上相机后退的那一段

	double ux = wx;
	double uy = wy * cx + wz * sx;          // Rx(-θx)
	double uz = -wy * sx + wz * cx;

	out[0] = (float)((double)m_center[0] + ux * cy - uz * sy);   // Ry(-θy)
	out[1] = (float)((double)m_center[1] + uy);
	out[2] = (float)((double)m_center[2] + ux * sy + uz * cy);

	return TRUE;
}

// ---------------------------------------------------------------------------
// 把鼠标点的位置变成“当前待放的那个控制点”
// 鼠标在屏幕上只能定 x、y 两个自由度，所以点的 z 保持不变：
// 落在“该点当前 z 所在的水平面”上，这样它正好出现在鼠标指针下面。
// 想调 z 请用「曲线 / 输入控制点坐标」对话框。
// ---------------------------------------------------------------------------
void ChanqingqiongView::PlacePickPoint(CPoint pt)
{
	if (m_nPickIndex >= m_nCtrlCount)
	{
		return;
	}

	int i = m_nPickIndex;
	float zPlane = m_ctrl[i][2];

	float p[3];
	if (!PickPointOnZPlane(pt, zPlane, p))
	{
		SetHint(_T("这个视角下参考平面几乎侧对着屏幕，算不出落点，请先转一下视角再点。"));
		return;
	}

	m_ctrl[i][0] = p[0];
	m_ctrl[i][1] = p[1];
	m_ctrl[i][2] = zPlane;

	m_nPickIndex++;
	if (m_nPickIndex >= m_nCtrlCount)
	{
		m_bPickMode = FALSE;        // 点满了自动退出拾取模式
	}

	UpdatePickHint();
	Invalidate();
}

// ChanqingqiongView 打印

BOOL ChanqingqiongView::OnPreparePrinting(CPrintInfo* pInfo)
{
	// 默认准备
	return DoPreparePrinting(pInfo);
}

void ChanqingqiongView::OnBeginPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	// TODO: 添加额外的打印前进行的初始化过程
}

void ChanqingqiongView::OnEndPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	// TODO: 添加打印后进行的清理过程
}


// ChanqingqiongView 诊断

#ifdef _DEBUG
void ChanqingqiongView::AssertValid() const
{
	CView::AssertValid();
}

void ChanqingqiongView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

ChanqingqiongDoc* ChanqingqiongView::GetDocument() const // 非调试版本是内联的
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(ChanqingqiongDoc)));
	return (ChanqingqiongDoc*)m_pDocument;
}
#endif //_DEBUG


// ChanqingqiongView 消息处理程序
BOOL ChanqingqiongView::SetWindowPixelFormat(HDC hDC)
{
	PIXELFORMATDESCRIPTOR  pixelDesc = { sizeof(PIXELFORMATDESCRIPTOR), 1,
																			   PFD_DRAW_TO_WINDOW |
																			   PFD_SUPPORT_OPENGL |
																			   PFD_DOUBLEBUFFER,
																			   PFD_TYPE_RGBA,   24,  0,0,0,0,0,0,  0,  0,  0,
																			   0,0,0,0,  32,   0,  0, 0,  0,  0,0,0 };
	m_GLPixelIndex = ChoosePixelFormat(hDC, &pixelDesc);

	if (m_GLPixelIndex == 0)
	{
		return FALSE;
	}

	if (SetPixelFormat(hDC, m_GLPixelIndex, &pixelDesc) == FALSE)
	{
		return FALSE;
	}

	return TRUE;
}

BOOL ChanqingqiongView::CreateViewGLContext(HDC hDC)
{
	m_hGLContext = wglCreateContext(hDC);
	if (m_hGLContext == NULL) //创建失败
	{
		return FALSE;
	}

	if (wglMakeCurrent(hDC, m_hGLContext) == FALSE)
	{//选为当前RC失败
		return FALSE;
	}
	return TRUE;
}

int ChanqingqiongView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;

	// TODO:  在此添加您专用的创建代码
	HWND hWnd = this->GetSafeHwnd();
	HDC hDC = ::GetDC(hWnd);

	if (this->SetWindowPixelFormat(hDC) == FALSE)
	{
		return 0;
	}

	if (this->CreateViewGLContext(hDC) == FALSE)
	{
		return 0;
	}

	glewInit();
	glDrawBuffer(GL_BACK);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClearDepth(1.0);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	// 线条抗锯齿，曲线看着更顺滑
	glEnable(GL_LINE_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// 按当前客户区尺寸先建一次投影，
	// 免得窗口刚出现时用的是默认投影，曲线显示不正常
	CRect rcClient;
	GetClientRect(&rcClient);
	m_nWidth = rcClient.Width();
	m_nHeight = rcClient.Height();
	SetupProjection(m_nWidth, m_nHeight);

	return 0;
}

// 首次显示时在状态栏写一句操作提示
void ChanqingqiongView::OnInitialUpdate()
{
	CView::OnInitialUpdate();

	SetHint(_T("左键拖拽：旋转 ｜ 右键：复位视角 ｜ 滚轮：缩放 ｜ 键：I 输入坐标  P 鼠标点选  D 默认曲线  R 复位视角  G 网格  A 坐标轴"));
}

void ChanqingqiongView::OnDestroy()
{
	CView::OnDestroy();

	// TODO: 在此处添加消息处理程序代码

	if (wglGetCurrentContext() != NULL)
	{
		wglMakeCurrent(NULL, NULL);
	}

	if (this->m_hGLContext != NULL)
	{
		wglDeleteContext(this->m_hGLContext);
		this->m_hGLContext = NULL;
	}
}

void ChanqingqiongView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);

	// TODO: 在此处添加消息处理程序代码
	m_nWidth = cx;
	m_nHeight = cy;

	SetupProjection(cx, cy);    // 窗口大小一变，投影矩阵要跟着重算
	Invalidate();
}

BOOL ChanqingqiongView::OnEraseBkgnd(CDC* pDC)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值

	//return CView::OnEraseBkgnd(pDC);
	return true;
}


// ===========================================================================
//                     鼠标 / 键盘：旋转、缩放、点选
// ===========================================================================

// 按下左键 → 进入拖拽状态。
// 拾取模式下不在这里放点：要等松开时看看到底是“单击”还是“拖拽转视角”。
void ChanqingqiongView::OnLButtonDown(UINT nFlags, CPoint point)
{
	SetFocus();                 // 让视图拿到键盘焦点，各种快捷键才响应
	SetCapture();               // 捕获鼠标，拖到窗口外面也照样收到消息
	m_bDragging = TRUE;
	m_ptLastMouse = point;
	m_ptDown = point;

	CView::OnLButtonDown(nFlags, point);
}

// 松开左键 → 结束拖拽；拾取模式下如果几乎没动过，就当成一次“单击”来放点
void ChanqingqiongView::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_bDragging)
	{
		m_bDragging = FALSE;
		ReleaseCapture();

		int mvx = point.x - m_ptDown.x;
		int mvy = point.y - m_ptDown.y;
		if (mvx < 0) mvx = -mvx;
		if (mvy < 0) mvy = -mvy;

		// 位移很小 → 是单击，不是拖拽旋转
		if (m_bPickMode && mvx + mvy <= kClickSlop)
		{
			PlacePickPoint(m_ptDown);
		}
	}

	CView::OnLButtonUp(nFlags, point);
}

// 拖拽过程中：把鼠标的位移换算成旋转角度
void ChanqingqiongView::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_bDragging)
	{
		// 鼠标左右移动 → 绕 Y 轴转（转盘效果）
		// 鼠标上下移动 → 绕 X 轴转（俯视 / 仰视）
		int dx = point.x - m_ptLastMouse.x;
		int dy = point.y - m_ptLastMouse.y;

		m_fRotY += (float)dx * kRotStep;
		m_fRotX += (float)dy * kRotStep;

		// 绕 Y 轴可以随便转；绕 X 轴限制在 -90°~90°，免得画面上下颠倒看不明白
		if (m_fRotY > 180.0f)  m_fRotY -= 360.0f;
		if (m_fRotY < -180.0f) m_fRotY += 360.0f;
		if (m_fRotX > 90.0f)   m_fRotX = 90.0f;
		if (m_fRotX < -90.0f)  m_fRotX = -90.0f;

		m_ptLastMouse = point;
		Invalidate();           // 请求重绘，曲线就跟着鼠标转起来了
	}

	CView::OnMouseMove(nFlags, point);
}

// 右键 → 复位视角
void ChanqingqiongView::OnRButtonDown(UINT nFlags, CPoint point)
{
	ResetView();

	CView::OnRButtonDown(nFlags, point);
}

// 滚轮 → 拉近拉远（缩放）
BOOL ChanqingqiongView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	// 向前滚（zDelta > 0）靠近物体，看着变大；向后滚则远离。
	// 用乘性倍率而不是加减，快慢手感才均匀。
	float steps = (float)zDelta / (float)WHEEL_DELTA;
	float ratio = m_fCamDist / m_fFitDist;
	ratio *= powf(kZoomRate, steps);

	m_fCamDist = ClampCamDist(ratio * m_fFitDist);

	Invalidate();
	return TRUE;
}

// 拾取模式下把鼠标换成十字光标，提示现在可以点选
BOOL ChanqingqiongView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	if (m_bPickMode)
	{
		::SetCursor(::LoadCursor(NULL, IDC_CROSS));
		return TRUE;
	}

	return CView::OnSetCursor(pWnd, nHitTest, message);
}

// 键盘：R 复位视角、G 网格、A 坐标轴、I 输入坐标、P 点选、D 默认曲线、方向键微调
void ChanqingqiongView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	switch (nChar)
	{
	case 'R':                                   // 复位视角
		ResetView();
		break;
	case 'G':                                   // 显示 / 隐藏参考网格
		m_bShowGrid = !m_bShowGrid;
		Invalidate();
		break;
	case 'A':                                   // 显示 / 隐藏坐标轴
		m_bShowAxes = !m_bShowAxes;
		Invalidate();
		break;
	case 'I':                                   // 打开坐标输入对话框
		OnInputPoints();
		break;
	case 'P':                                   // 切换鼠标点选模式
		OnPickPoints();
		break;
	case 'D':                                   // 恢复默认曲线
		OnResetPoints();
		break;
	case VK_ESCAPE:                             // 退出点选模式（已经点下去的点保留）
		if (m_bPickMode)
		{
			m_bPickMode = FALSE;
			SetHint(_T("已退出点选模式。"));
			Invalidate();
		}
		break;
	case VK_LEFT:                               // 方向键小幅旋转，便于精确调整
		m_fRotY -= 5.0f;
		Invalidate();
		break;
	case VK_RIGHT:
		m_fRotY += 5.0f;
		Invalidate();
		break;
	case VK_UP:
		m_fRotX -= 5.0f;
		if (m_fRotX < -90.0f) m_fRotX = -90.0f;
		Invalidate();
		break;
	case VK_DOWN:
		m_fRotX += 5.0f;
		if (m_fRotX > 90.0f) m_fRotX = 90.0f;
		Invalidate();
		break;
	default:
		break;
	}

	CView::OnKeyDown(nChar, nRepCnt, nFlags);
}


// ===========================================================================
//                       曲线菜单命令：自由输入控制点
// ===========================================================================

// 「曲线 / 输入控制点坐标」：弹出表格对话框，逐点填 x、y、z
void ChanqingqiongView::OnInputPoints(void)
{
	CInputPointsDlg dlg(this);

	// 把当前曲线塞进对话框
	dlg.m_nCount = m_nCtrlCount;
	for (int i = 0; i < CurveDef::MAX_COUNT; i++)
	{
		for (int k = 0; k < 3; k++)
		{
			dlg.m_pt[i][k] = m_ctrl[i][k];
		}
	}

	if (dlg.DoModal() != IDOK)
	{
		return;
	}

	// 取回用户填的曲线
	m_nCtrlCount = dlg.m_nCount;
	for (int i = 0; i < m_nCtrlCount; i++)
	{
		for (int k = 0; k < 3; k++)
		{
			m_ctrl[i][k] = dlg.m_pt[i][k];
		}
	}

	m_bPickMode = FALSE;
	m_nPickIndex = 0;

	// 新坐标可能和原来差得很远，把视野重新对准它
	FitViewToPoints();
	SetupProjection(m_nWidth, m_nHeight);

	SetHint(_T("已按输入的坐标更新曲线。"));
	Invalidate();
}

// 「曲线 / 鼠标点选控制点」：进入 / 退出拾取模式
void ChanqingqiongView::OnPickPoints(void)
{
	m_bPickMode = !m_bPickMode;

	if (m_bPickMode)
	{
		m_nPickIndex = 0;       // 从 P0 开始依次点
		UpdatePickHint();
	}
	else
	{
		SetHint(_T("已退出点选模式。"));
	}

	Invalidate();
}

// 处于拾取模式时让菜单项显示打勾
void ChanqingqiongView::OnUpdatePickPoints(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(m_bPickMode ? 1 : 0);
}

// 「曲线 / 恢复默认曲线」
void ChanqingqiongView::OnResetPoints(void)
{
	ResetCtrlPoints();

	FitViewToPoints();
	SetupProjection(m_nWidth, m_nHeight);

	SetHint(_T("已恢复默认曲线。"));
	Invalidate();
}
