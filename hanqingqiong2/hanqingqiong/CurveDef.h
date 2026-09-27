
// CurveDef.h: 贝塞尔曲线控制点的公共定义
//
// 视图和坐标输入对话框都包含这个头文件，
// 免得“最多几个控制点”“默认曲线是哪几个点”在好几处各写一遍、改的时候漏掉一处。

#pragma once

namespace CurveDef
{
	// 控制点个数的允许范围。视图里的坐标数组就按 MAX_COUNT 开。
	const int MIN_COUNT = 3;
	const int MAX_COUNT = 10;

	// 默认曲线：程序启动时用它，“恢复默认曲线”也用它。
	const int DEFAULT_COUNT = 4;
	static const float DEFAULT_POINTS[DEFAULT_COUNT][3] =
	{
		{ 0.0f, 0.0f,  0.0f },   // P0
		{ 1.0f, 3.0f,  2.0f },   // P1  往 +z 方向鼓出来
		{ 2.0f, 1.0f, -2.0f },   // P2  往 -z 方向凹进去
		{ 3.0f, 3.0f,  0.0f }    // P3
	};
}
