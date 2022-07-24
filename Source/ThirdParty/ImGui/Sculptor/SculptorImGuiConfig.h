#pragma once


#ifdef IMGUI_PLATFORM_WINDOWS
	#ifdef IMGUI_BUILD_DLL
		#define IMGUI_API __declspec(dllexport)
	#else
		#define IMGUI_API __declspec(dllimport)
	#endif
#else
	#error Sculptor only supports Windows
#endif // IMGUI_PLATFORM_WINDOWS


#include "Eigen/Geometry"

#define IM_VEC2_CLASS_EXTRA							\
	ImVec2(const Eigen::Vector2f& rhs)				\
		: x(rhs.x())								\
		, y(rhs.y())								\
	{ }												\
													\
	operator Eigen::Vector2f() const				\
	{												\
		return Eigen::Vector2f(x, y);				\
	}
	

#define IM_VEC4_CLASS_EXTRA							\
	ImVec4(const Eigen::Vector4f& rhs)				\
		: x(rhs.x())								\
		, y(rhs.y())								\
		, z(rhs.z())								\
		, w(rhs.w())								\
	{ }												\
													\
	operator Eigen::Vector4f() const				\
	{												\
		return Eigen::Vector4f(x, y, z, w);			\
	}
