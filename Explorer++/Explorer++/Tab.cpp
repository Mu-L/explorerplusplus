// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "Tab.h"
#include "Config.h"
#include "CoreInterface.h"
#include "PreservedTab.h"
#include "ShellBrowser/FolderSettings.h"
#include "ShellBrowser/ShellBrowserImpl.h"
#include "ShellBrowser/ShellNavigationController.h"
#include "TabStorage.h"
#include <wil/resource.h>

int Tab::idCounter = 1;

Tab::Tab(std::shared_ptr<ShellBrowserImpl> shellBrowser) :
	m_id(idCounter++),
	m_shellBrowser(shellBrowser),
	m_useCustomName(false),
	m_lockState(LockState::NotLocked)
{
	// The provided ShellBrowser instance may be null in tests.
	if (m_shellBrowser)
	{
		m_shellBrowser->SetID(m_id);
	}
}

Tab::Tab(const PreservedTab &preservedTab, std::shared_ptr<ShellBrowserImpl> shellBrowser) :
	m_id(idCounter++),
	m_shellBrowser(shellBrowser),
	m_useCustomName(preservedTab.useCustomName),
	m_customName(preservedTab.customName),
	m_lockState(preservedTab.lockState)
{
	if (m_shellBrowser)
	{
		m_shellBrowser->SetID(m_id);
	}
}

int Tab::GetId() const
{
	return m_id;
}

ShellBrowserImpl *Tab::GetShellBrowser() const
{
	return m_shellBrowser.get();
}

std::weak_ptr<ShellBrowserImpl> Tab::GetShellBrowserWeak() const
{
	return std::weak_ptr<ShellBrowserImpl>(m_shellBrowser);
}

// If a custom name has been set, that will be returned. Otherwise, the
// display name of the current directory will be returned.
std::wstring Tab::GetName() const
{
	if (m_useCustomName)
	{
		return m_customName;
	}

	if (!m_shellBrowser)
	{
		return {};
	}

	auto pidlDirectory = m_shellBrowser->GetDirectoryIdl();

	std::wstring name;
	HRESULT hr = GetDisplayName(pidlDirectory.get(), SHGDN_INFOLDER, name);

	if (FAILED(hr))
	{
		return L"(Unknown)";
	}

	return name;
}

bool Tab::GetUseCustomName() const
{
	return m_useCustomName;
}

void Tab::SetCustomName(const std::wstring &name)
{
	if (name.empty())
	{
		return;
	}

	m_useCustomName = true;
	m_customName = name;

	m_tabUpdatedSignal(*this, PropertyType::Name);
}

void Tab::ClearCustomName()
{
	m_useCustomName = false;
	m_customName.erase();

	m_tabUpdatedSignal(*this, PropertyType::Name);
}

Tab::LockState Tab::GetLockState() const
{
	return m_lockState;
}

void Tab::SetLockState(LockState lockState)
{
	if (lockState == m_lockState)
	{
		return;
	}

	m_lockState = lockState;

	if (m_shellBrowser)
	{
		NavigationMode navigationMode = (lockState == LockState::AddressLocked)
			? NavigationMode::ForceNewTab
			: NavigationMode::Normal;
		m_shellBrowser->GetNavigationController()->SetNavigationMode(navigationMode);
	}

	m_tabUpdatedSignal(*this, PropertyType::LockState);
}

boost::signals2::connection Tab::AddTabUpdatedObserver(const TabUpdatedSignal::slot_type &observer)
{
	return m_tabUpdatedSignal.connect(observer);
}

TabStorageData Tab::GetStorageData() const
{
	// The ShellBrowser instance can be null in tests, in which case, this method shouldn't be
	// called.
	CHECK(m_shellBrowser);

	TabStorageData storageData;
	storageData.pidl = m_shellBrowser->GetDirectoryIdl().get();
	storageData.directory = m_shellBrowser->GetDirectory();
	storageData.folderSettings = m_shellBrowser->GetFolderSettings();
	storageData.columns = m_shellBrowser->ExportAllColumns();

	TabSettings tabSettings;

	if (m_useCustomName)
	{
		tabSettings.name = m_customName;
	}

	tabSettings.lockState = m_lockState;

	storageData.tabSettings = tabSettings;

	return storageData;
}
