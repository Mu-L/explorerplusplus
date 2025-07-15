// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include "BrowserWindow.h"
#include "MainRebarStorage.h"
#include "TabStorage.h"
#include "WindowStorage.h"
#include <gmock/gmock.h>

class BrowserWindowMock : public BrowserWindow
{
public:
	// BrowserWindow
	MOCK_METHOD(HWND, GetHWND, (), (const, override));
	MOCK_METHOD(BrowserCommandController *, GetCommandController, (), (override));
	MOCK_METHOD(BrowserPane *, GetActivePane, (), (const, override));
	MOCK_METHOD(TabContainer *, GetActiveTabContainer, (), (override));
	MOCK_METHOD(const TabContainer *, GetActiveTabContainer, (), (const, override));
	MOCK_METHOD(void, FocusActiveTab, (), (override));
	MOCK_METHOD(Tab *, CreateTabFromPreservedTab, (const PreservedTab *tab), (override));
	MOCK_METHOD(void, OpenDefaultItem, (OpenFolderDisposition openFolderDisposition), (override));
	MOCK_METHOD(void, OpenItem,
		(const std::wstring &itemPath, OpenFolderDisposition openFolderDisposition), (override));
	MOCK_METHOD(void, OpenItem,
		(PCIDLIST_ABSOLUTE pidlItem, OpenFolderDisposition openFolderDisposition), (override));
	MOCK_METHOD(ShellBrowser *, GetActiveShellBrowser, (), (override));
	MOCK_METHOD(const ShellBrowser *, GetActiveShellBrowser, (), (const, override));
	MOCK_METHOD(void, StartMainToolbarCustomization, (), (override));
	MOCK_METHOD(std::optional<std::wstring>, RequestMenuHelpText, (HMENU menu, UINT id),
		(const, override));
	MOCK_METHOD(WindowStorageData, GetStorageData, (), (const, override));
	MOCK_METHOD(bool, IsActive, (), (const, override));
	MOCK_METHOD(void, Activate, (), (override));
	MOCK_METHOD(void, TryClose, (), (override));
	MOCK_METHOD(void, Close, (), (override));

	// MenuHelpTextHost
	MOCK_METHOD(boost::signals2::connection, AddMenuHelpTextRequestObserver,
		(const MenuHelpTextRequestSignal::slot_type &observer), (override));
};
