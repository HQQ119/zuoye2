
//{{NO_DEPENDENCIES}}
// 生成的 Microsoft Visual C++ 包含文件。
// 由 hanqingqiong.rc 使用
//
#define IDD_ABOUTBOX				100
#define IDD_INPUTPOINTS				101
#define IDP_OLE_INIT_FAILED			100
#define IDR_MAINFRAME				128
#define IDR_hanqingqiongTYPE				130

// ---------------------------------------------------------------------------
// 曲线菜单命令
// ---------------------------------------------------------------------------
#define ID_CURVE_INPUT_POINTS		32771	// 弹出对话框输入控制点坐标
#define ID_CURVE_PICK_POINTS		32772	// 在视图里用鼠标依次点选控制点
#define ID_CURVE_RESET_POINTS		32773	// 恢复默认控制点

// ---------------------------------------------------------------------------
// IDD_INPUTPOINTS 对话框里的控件
//   第 i 行（i = 0 ~ 9）的控件 ID 由下面的基址加 i 得到，
//   这样代码里用一个 for 循环就能取到任意一行的三个输入框。
// ---------------------------------------------------------------------------
#define IDC_EDIT_X0					1001	// 第 i 行 x 输入框：IDC_EDIT_X0 + i（1001~1010）
#define IDC_EDIT_Y0					1011	// 第 i 行 y 输入框：IDC_EDIT_Y0 + i（1011~1020）
#define IDC_EDIT_Z0					1021	// 第 i 行 z 输入框：IDC_EDIT_Z0 + i（1021~1030）
#define IDC_STATIC_P0				1031	// 第 i 行序号标签：IDC_STATIC_P0 + i（1031~1040）

#define IDC_STATIC_HEAD_P			1044	// 表头“编号”
#define IDC_STATIC_HEAD_X			1045	// 表头“X”
#define IDC_STATIC_HEAD_Y			1046	// 表头“Y”
#define IDC_STATIC_HEAD_Z			1047	// 表头“Z”
#define IDC_EDIT_COUNT				1041	// 控制点个数
#define IDC_STATIC_COUNT_LABEL		1042	// “控制点个数”文字
#define IDC_STATIC_COUNT_HINT		1043	// “（可填 3 ~ 10）”提示
#define IDC_BTN_RESET				1048	// “恢复默认”按钮

// 新对象的下一组默认值
//
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE	310
#define _APS_NEXT_CONTROL_VALUE		1060
#define _APS_NEXT_SYMED_VALUE		310
#define _APS_NEXT_COMMAND_VALUE		32774
#endif
#endif
