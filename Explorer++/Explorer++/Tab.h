// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include "../Helper/Macros.h"
#include <boost/signals2.hpp>
#include <memory>

class CoreInterface;
class FileActionHandler;
struct FolderColumns;
struct FolderSettings;
struct PreservedTab;
class ShellBrowser;
class TabNavigationInterface;

class Tab
{
public:
	enum class PropertyType
	{
		Name,
		LockState
	};

	enum class LockState
	{
		// The tab isn't locked; it can be navigated freely and closed.
		NotLocked,

		// The tab is locked. It can be navigated freely, but not closed.
		Locked,

		// Both the tab and address are locked. The tab can't be navigated or closed. All
		// navigations will proceed in a new tab.
		AddressLocked
	};

	typedef boost::signals2::signal<void(const Tab &tab, PropertyType propertyType)>
		TabUpdatedSignal;

	Tab(CoreInterface *coreInterface, TabNavigationInterface *tabNavigation,
		FileActionHandler *fileActionHandler, const FolderSettings *folderSettings,
		const FolderColumns *initialColumns);
	Tab(const PreservedTab &preservedTab, CoreInterface *coreInterface,
		TabNavigationInterface *tabNavigation, FileActionHandler *fileActionHandler);

	int GetId() const;

	ShellBrowser *GetShellBrowser() const;
	std::weak_ptr<ShellBrowser> GetShellBrowserWeak() const;

	std::wstring GetName() const;
	bool GetUseCustomName() const;
	void SetCustomName(const std::wstring &name);
	void ClearCustomName();

	LockState GetLockState() const;
	void SetLockState(LockState lockState);

	boost::signals2::connection AddTabUpdatedObserver(const TabUpdatedSignal::slot_type &observer);

	/* Although each tab manages its
	own columns, it does not know
	about any column defaults.
	Therefore, it makes more sense
	for this setting to remain here. */
	// BOOL	bUsingDefaultColumns;

private:
	DISALLOW_COPY_AND_ASSIGN(Tab);

	static int idCounter;
	const int m_id;

	std::shared_ptr<ShellBrowser> m_shellBrowser;

	bool m_useCustomName;
	std::wstring m_customName;
	LockState m_lockState;

	TabUpdatedSignal m_tabUpdatedSignal;
};
