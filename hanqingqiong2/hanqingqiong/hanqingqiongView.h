
// hanqingqiongView.h: ChanqingqiongView 类的接口
//

#pragma once

#include "CurveDef.h"


class ChanqingqiongView : public CView
{
protected: // 仅从序列化创建
	ChanqingqiongView() noexcept;
	DECLARE_DYNCREATE(ChanqingqiongView)

	int m_GLPixelIndex;
	HGLRC m_hGLContext;
	int m_nWidth;          // 最近一次 OnSize 记录的客户区宽
	int m_nHeight;         // 最近一次 OnSize 记录的客户区高

	// ---------------- 曲线数据 ----------------
	// 控制点个数可以在 CurveDef::MIN_COUNT ~ MAX_COUNT 之间改，
	// 所以按最大值开数组，实际只用前 m_nCtrlCount 个。
	float m_ctrl[CurveDef::MAX_COUNT][3];
	int   m_nCtrlCount;

	// ---------------- 三维观察参数 ----------------
	float  m_fRotX;        // 绕 X 轴旋转角（上下转动），单位：度
	float  m_fRotY;        // 绕 Y 轴旋转角（左右转动），单位：度
	float  m_fCamDist;     // 相机到物体中心的距离，滚轮可缩放
	float  m_center[3];    // 旋转中心（控制点包围盒的中心）
	float  m_fFitDist;     // 刚好能把整条曲线塞进视野的相机距离，缩放范围以它为准
	float  m_fGridHalf;    // 参考网格的半边长，跟着曲线大小走
	BOOL   m_bDragging;    // 左键是否按下（是否处于拖拽旋转中）
	CPoint m_ptLastMouse;  // 拖拽时上一次的鼠标位置
	CPoint m_ptDown;       // 左键按下的位置，用来区分“单击”和“拖拽旋转”
	BOOL   m_bShowGrid;    // 是否显示 z=0 参考平面网格
	BOOL   m_bShowAxes;    // 是否显示坐标轴

	// ---------------- 鼠标点选控制点 ----------------
	BOOL   m_bPickMode;    // 是否处于拾取模式
	int    m_nPickIndex;   // 下一个要放下的控制点下标

// 特性
public:
	ChanqingqiongDoc* GetDocument() const;

// 操作
public:
	void EvalCurve(float t, float out[3]) const; // 算出参数 t 处的曲线点（de Casteljau）
	void DrawScene(void);                  // 绘制曲线（任意点数的三维 de Casteljau）
	void DrawGrid(void);                   // 绘制参考网格
	void DrawAxes(void);                   // 绘制三条坐标轴
	void DrawControlPolygon(void);         // 绘制控制多边形与控制点
	void SetupProjection(int cx, int cy);  // 按客户区尺寸建立透视投影
	void ResetView(void);                  // 复位观察角度

	void ResetCtrlPoints(void);            // 恢复默认控制点
	void FitViewToPoints(void);            // 按当前控制点重算旋转中心与合适的相机距离
	BOOL PickPointOnZPlane(CPoint pt, float zPlane, float out[3]);
	                                       // 屏幕像素 → 平面 z = zPlane 上的世界坐标
	void PlacePickPoint(CPoint pt);        // 把鼠标点的位置变成当前待放的控制点
	float ClampCamDist(float d) const;     // 把相机距离夹到允许范围内
	void SetHint(LPCTSTR pszText);         // 在状态栏显示一句话
	void UpdatePickHint(void);             // 更新点选进度提示

// 重写
public:
	virtual void OnDraw(CDC* pDC);  // 重写以绘制该视图
	virtual void OnInitialUpdate(); // 首次显示时在状态栏给出操作提示
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
	virtual BOOL OnPreparePrinting(CPrintInfo* pInfo);
	virtual void OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo);
	virtual void OnEndPrinting(CDC* pDC, CPrintInfo* pInfo);

	BOOL SetWindowPixelFormat(HDC hDC);
	BOOL CreateViewGLContext(HDC hDC);

// 实现
public:
	virtual ~ChanqingqiongView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// 生成的消息映射函数
protected:
	DECLARE_MESSAGE_MAP()
public:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);

	// ---------------- 鼠标 / 键盘 ----------------
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);

	// ---------------- 曲线菜单命令 ----------------
	afx_msg void OnInputPoints();                     // 弹对话框输入坐标
	afx_msg void OnPickPoints();                      // 切换鼠标点选模式
	afx_msg void OnUpdatePickPoints(CCmdUI* pCmdUI);  // 菜单打勾状态
	afx_msg void OnResetPoints();                     // 恢复默认曲线
};

#ifndef _DEBUG  // hanqingqiongView.cpp 中的调试版本
inline ChanqingqiongDoc* ChanqingqiongView::GetDocument() const
   { return reinterpret_cast<ChanqingqiongDoc*>(m_pDocument); }
#endif
