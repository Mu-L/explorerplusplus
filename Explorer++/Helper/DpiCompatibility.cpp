// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "DpiCompatibility.h"
#include "Helper.h"
#include <wil/win32_helpers.h>

DpiCompatibility &DpiCompatibility::GetInstance()
{
	static DpiCompatibility dpiCompatibility;
	return dpiCompatibility;
}

DpiCompatibility::DpiCompatibility() :
	m_SystemParametersInfoForDpi(nullptr),
	m_GetSystemMetricsForDpi(nullptr),
	m_GetDpiForWindow(nullptr)
{
	m_user32 = LoadSystemLibrary(L"user32.dll");

	if (m_user32)
	{
		m_SystemParametersInfoForDpi = std::bit_cast<SystemParametersInfoForDpiType>(
			GetProcAddress(m_user32.get(), "SystemParametersInfoForDpi"));
		m_GetSystemMetricsForDpi = std::bit_cast<GetSystemMetricsForDpiType>(
			GetProcAddress(m_user32.get(), "GetSystemMetricsForDpi"));
		m_GetDpiForWindow =
			std::bit_cast<GetDpiForWindowType>(GetProcAddress(m_user32.get(), "GetDpiForWindow"));
	}
}

BOOL DpiCompatibility::SystemParametersInfoForDpi(UINT uiAction, UINT uiParam, PVOID pvParam,
	UINT fWinIni, UINT dpi)
{
	if (m_SystemParametersInfoForDpi)
	{
		return m_SystemParametersInfoForDpi(uiAction, uiParam, pvParam, fWinIni, dpi);
	}

	return SystemParametersInfo(uiAction, uiParam, pvParam, fWinIni);
}

int DpiCompatibility::GetSystemMetricsForDpi(int nIndex, UINT dpi)
{
	if (m_GetSystemMetricsForDpi)
	{
		return m_GetSystemMetricsForDpi(nIndex, dpi);
	}

	return GetSystemMetrics(nIndex);
}

UINT DpiCompatibility::GetDpiForWindow(HWND hwnd)
{
	if (m_GetDpiForWindow)
	{
		return m_GetDpiForWindow(hwnd);
	}

	auto hdc = wil::GetDC(hwnd);

	if (hdc)
	{
		return GetDeviceCaps(hdc.get(), LOGPIXELSX);
	}

	return USER_DEFAULT_SCREEN_DPI;
}

int DpiCompatibility::ScaleValue(HWND hwnd, int value)
{
	return MulDiv(value, GetDpiForWindow(hwnd), USER_DEFAULT_SCREEN_DPI);
}

int DpiCompatibility::PointsToPixelsForDefaultDpi(int pt)
{
	return PointsToPixelsForDpi(pt, USER_DEFAULT_SCREEN_DPI);
}

int DpiCompatibility::PixelsToPointsForDefaultDpi(int px)
{
	return PixelsToPointsForDpi(px, USER_DEFAULT_SCREEN_DPI);
}

// One point (the unit for font size) is equal to 1/72 of an inch (see
// https://learn.microsoft.com/en-us/windows/win32/learnwin32/dpi-and-device-independent-pixels).
// The number of pixels that correspond to an inch then depends on the DPI of the display. So, this
// function converts points to pixels, taking into account the DPI of the display the specified
// window is on.
int DpiCompatibility::PointsToPixels(HWND hwnd, int pt)
{
	return PointsToPixelsForDpi(pt, GetDpiForWindow(hwnd));
}

int DpiCompatibility::PixelsToPoints(HWND hwnd, int px)
{
	return PixelsToPointsForDpi(px, GetDpiForWindow(hwnd));
}

int DpiCompatibility::PointsToPixelsForDpi(int pt, UINT dpi)
{
	return MulDiv(pt, dpi, 72);
}

int DpiCompatibility::PixelsToPointsForDpi(int px, UINT dpi)
{
	return MulDiv(px, 72, dpi);
}
