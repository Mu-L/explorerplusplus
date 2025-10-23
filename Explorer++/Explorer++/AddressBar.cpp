// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "AddressBar.h"
#include "AddressBarView.h"
#include "AsyncIconFetcher.h"
#include "BrowserWindow.h"
#include "NavigationHelper.h"
#include "RuntimeHelper.h"
#include "ShellBrowser/NavigationEvents.h"
#include "ShellBrowser/ShellBrowser.h"
#include "ShellBrowser/ShellBrowserEvents.h"
#include "Tab.h"
#include "TabEvents.h"
#include "../Helper/DragDropHelper.h"
#include "../Helper/Helper.h"
#include "../Helper/ShellHelper.h"
#include <glog/logging.h>

AddressBar *AddressBar::Create(AddressBarView *view, BrowserWindow *browser, TabEvents *tabEvents,
	ShellBrowserEvents *shellBrowserEvents, NavigationEvents *navigationEvents,
	const Runtime *runtime, std::shared_ptr<AsyncIconFetcher> iconFetcher)
{
	return new AddressBar(view, browser, tabEvents, shellBrowserEvents, navigationEvents, runtime,
		iconFetcher);
}

AddressBar::AddressBar(AddressBarView *view, BrowserWindow *browser, TabEvents *tabEvents,
	ShellBrowserEvents *shellBrowserEvents, NavigationEvents *navigationEvents,
	const Runtime *runtime, std::shared_ptr<AsyncIconFetcher> iconFetcher) :
	m_view(view),
	m_browser(browser),
	m_runtime(runtime),
	m_iconFetcher(iconFetcher),
	m_commandTarget(browser->GetCommandTargetManager(), this),
	m_weakPtrFactory(this)
{
	Initialize(tabEvents, shellBrowserEvents, navigationEvents);
}

void AddressBar::Initialize(TabEvents *tabEvents, ShellBrowserEvents *shellBrowserEvents,
	NavigationEvents *navigationEvents)
{
	m_view->SetDelegate(this);
	m_view->windowDestroyedSignal.AddObserver(
		std::bind_front(&AddressBar::OnWindowDestroyed, this));

	m_connections.push_back(tabEvents->AddSelectedObserver(
		std::bind_front(&AddressBar::OnTabSelected, this), TabEventScope::ForBrowser(*m_browser)));

	m_connections.push_back(shellBrowserEvents->AddDirectoryPropertiesChangedObserver(
		std::bind_front(&AddressBar::OnDirectoryPropertiesChanged, this),
		ShellBrowserEventScope::ForActiveShellBrowser(*m_browser)));

	m_connections.push_back(navigationEvents->AddCommittedObserver(
		std::bind_front(&AddressBar::OnNavigationCommitted, this),
		NavigationEventScope::ForActiveShellBrowser(*m_browser)));
}

AddressBarView *AddressBar::GetView() const
{
	return m_view;
}

bool AddressBar::OnKeyPressed(UINT key)
{
	switch (key)
	{
	case VK_RETURN:
		OnEnterPressed();
		return true;

	case VK_ESCAPE:
		OnEscapePressed();
		return true;
	}

	return false;
}

void AddressBar::OnEnterPressed()
{
	std::wstring path = m_view->GetText();

	const auto *shellBrowser = m_browser->GetActiveShellBrowser();
	std::wstring currentDirectory =
		GetDisplayNameWithFallback(shellBrowser->GetDirectory().Raw(), SHGDN_FORPARSING);

	// When entering a path in the address bar in Windows Explorer, environment variables will be
	// expanded. The behavior here is designed to match that.
	// Note that this does result in potential ambiguity. '%' is a valid character in a filename.
	// That means, for example, it's valid to have a file or folder called %windir%. In cases like
	// that, entering the text %windir% would be ambiguous - the path could refer either to the
	// file/folder or environment variable. Explorer treats it as an environment variable, which is
	// also the behavior here.
	// Additionally, it appears that Explorer doesn't normalize "." in paths (though ".." is
	// normalized). For example, entering "c:\windows\.\" results in an error. Whereas here, the
	// path is normalized before navigation, meaning entering "c:\windows\.\" will result in a
	// navigation to "c:\windows". That also means that entering the relative path ".\" works as
	// expected.
	auto absolutePath = TransformUserEnteredPathToAbsolutePathAndNormalize(path, currentDirectory,
		EnvVarsExpansion::Expand);

	if (!absolutePath)
	{
		// TODO: Should possibly display an error here (perhaps in the status bar).
		return;
	}

	/* TODO: Could keep text user has entered and only revert if navigation fails. */
	// Whether a file or folder is being opened, the address bar text should be reverted to the
	// original text. If the item being opened is a folder, the text will be updated once the
	// navigation commits.
	// Note that if the above call to TransformUserEnteredPathToAbsolutePathAndNormalize() fails,
	// the text won't be reverted. That gives the user the chance to update the text and try again.
	m_view->RevertText();

	m_browser->OpenItem(*absolutePath,
		DetermineOpenDisposition(false, IsKeyDown(VK_CONTROL), IsKeyDown(VK_SHIFT)));
	m_browser->FocusActiveTab();
}

void AddressBar::OnEscapePressed()
{
	if (m_view->IsTextModified())
	{
		m_view->RevertText();
		m_view->SelectAllText();
	}
	else
	{
		m_browser->FocusActiveTab();
	}
}

void AddressBar::OnBeginDrag()
{
	const auto *shellBrowser = m_browser->GetActiveShellBrowser();
	const auto &pidl = shellBrowser->GetDirectory();
	StartDragForShellItems({ pidl.Raw() }, DROPEFFECT_LINK);
}

void AddressBar::OnFocused()
{
	m_commandTarget.TargetFocused();
}

void AddressBar::OnTabSelected(const Tab &tab)
{
	UpdateTextAndIcon(tab.GetShellBrowser());
}

void AddressBar::OnNavigationCommitted(const NavigationRequest *request)
{
	UpdateTextAndIcon(request->GetShellBrowser());
}

void AddressBar::OnDirectoryPropertiesChanged(const ShellBrowser *shellBrowser)
{
	// Since the directory properties have changed, it's possible that the icon has changed.
	// Therefore, the updated icon should always be retrieved.
	UpdateTextAndIcon(shellBrowser, IconUpdateType::AlwaysFetch);
}

void AddressBar::UpdateTextAndIcon(const ShellBrowser *shellBrowser, IconUpdateType iconUpdateType)
{
	// Resetting this here ensures that any previous icon requests that are still ongoing will be
	// ignored once they complete.
	m_scopedStopSource = std::make_unique<ScopedStopSource>();

	const auto &pidl = shellBrowser->GetDirectory();

	auto cachedIconIndex = m_iconFetcher->MaybeGetCachedIconIndex(pidl.Raw());
	int iconIndex;

	if (cachedIconIndex)
	{
		iconIndex = *cachedIconIndex;
	}
	else
	{
		iconIndex = m_iconFetcher->GetDefaultIconIndex(pidl.Raw());
	}

	if (iconUpdateType == IconUpdateType::AlwaysFetch || !cachedIconIndex)
	{
		RetrieveUpdatedIcon(m_weakPtrFactory.GetWeakPtr(), pidl);
	}

	auto fullPathForDisplay = GetFolderPathForDisplayWithFallback(pidl.Raw());
	m_view->UpdateTextAndIcon(fullPathForDisplay, iconIndex);
}

concurrencpp::null_result AddressBar::RetrieveUpdatedIcon(WeakPtr<AddressBar> weakSelf,
	PidlAbsolute pidl)
{
	auto *runtime = weakSelf->m_runtime;
	auto iconFetcher = weakSelf->m_iconFetcher;
	auto stopToken = weakSelf->m_scopedStopSource->GetToken();

	auto iconInfo = co_await iconFetcher->GetIconIndexAsync(pidl.Raw(), stopToken);

	if (!iconInfo)
	{
		co_return;
	}

	co_await ResumeOnUiThread(runtime);

	if (stopToken.stop_requested() || !weakSelf)
	{
		co_return;
	}

	weakSelf->m_view->UpdateTextAndIcon(std::nullopt, iconInfo->iconIndex);
}

bool AddressBar::IsCommandEnabled(int command) const
{
	UNREFERENCED_PARAMETER(command);

	return false;
}

void AddressBar::ExecuteCommand(int command)
{
	UNREFERENCED_PARAMETER(command);
}

void AddressBar::OnWindowDestroyed()
{
	delete this;
}
