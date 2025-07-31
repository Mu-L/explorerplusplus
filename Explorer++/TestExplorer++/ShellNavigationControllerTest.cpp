// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "../Explorer++/ShellBrowser/ShellNavigationController.h"
#include "BrowserWindowMock.h"
#include "NavigationRequestTestHelper.h"
#include "ShellBrowser/NavigationEvents.h"
#include "ShellBrowser/PreservedShellBrowser.h"
#include "ShellBrowserFake.h"
#include "ShellTestHelper.h"
#include "../Explorer++/ShellBrowser/HistoryEntry.h"
#include "../Explorer++/ShellBrowser/PreservedHistoryEntry.h"
#include "../Helper/ShellHelper.h"
#include <gtest/gtest.h>
#include <ShlObj.h>

using namespace testing;

class ShellNavigationControllerTest : public Test
{
protected:
	ShellNavigationControllerTest() : m_shellBrowser(&m_browser, &m_navigationEvents)
	{
	}

	ShellNavigationController *GetNavigationController() const
	{
		return m_shellBrowser.GetNavigationController();
	}

	NavigationEvents m_navigationEvents;
	BrowserWindowMock m_browser;
	ShellBrowserFake m_shellBrowser;
};

TEST_F(ShellNavigationControllerTest, RefreshInitialEntry)
{
	auto *navigationController = GetNavigationController();

	auto initialEntry = navigationController->GetCurrentEntry();
	auto initialEntryPidl = initialEntry->GetPidl();

	navigationController->Refresh();

	// Refreshing shouldn't result in a history entry being added.
	EXPECT_FALSE(navigationController->CanGoBack());
	EXPECT_FALSE(navigationController->CanGoForward());
	EXPECT_EQ(navigationController->GetNumHistoryEntries(), 1);

	auto updatedEntry = navigationController->GetCurrentEntry();
	ASSERT_NE(updatedEntry, nullptr);
	EXPECT_EQ(updatedEntry->GetInitialNavigationType(),
		HistoryEntry::InitialNavigationType::NonInitial);
	EXPECT_FALSE(updatedEntry->IsInitialEntry());
	EXPECT_EQ(updatedEntry->GetPidl(), initialEntryPidl);
}

TEST_F(ShellNavigationControllerTest, RefreshSubsequentEntry)
{
	auto *navigationController = GetNavigationController();

	PidlAbsolute pidl;
	m_shellBrowser.NavigateToPath(L"C:\\Fake", HistoryEntryType::AddEntry, &pidl);

	navigationController->Refresh();

	EXPECT_FALSE(navigationController->CanGoBack());
	EXPECT_FALSE(navigationController->CanGoForward());
	EXPECT_EQ(navigationController->GetNumHistoryEntries(), 1);

	auto updatedEntry = navigationController->GetCurrentEntry();
	ASSERT_NE(updatedEntry, nullptr);
	EXPECT_EQ(updatedEntry->GetPidl(), pidl);
}

TEST_F(ShellNavigationControllerTest, NavigateToSameFolder)
{
	m_shellBrowser.NavigateToPath(L"C:\\Fake");
	m_shellBrowser.NavigateToPath(L"C:\\Fake");

	auto *navigationController = GetNavigationController();

	// Navigating to the same location should be treated as an implicit refresh. No history entry
	// should be added.
	EXPECT_FALSE(navigationController->CanGoBack());
	EXPECT_FALSE(navigationController->CanGoForward());
	EXPECT_EQ(navigationController->GetNumHistoryEntries(), 1);
}

TEST_F(ShellNavigationControllerTest, BackForward)
{
	m_shellBrowser.NavigateToPath(L"C:\\Fake1");

	auto *navigationController = GetNavigationController();

	EXPECT_FALSE(navigationController->CanGoBack());
	EXPECT_FALSE(navigationController->CanGoForward());
	EXPECT_EQ(navigationController->GetNumHistoryEntries(), 1);

	m_shellBrowser.NavigateToPath(L"C:\\Fake2");

	EXPECT_TRUE(navigationController->CanGoBack());
	EXPECT_FALSE(navigationController->CanGoForward());
	EXPECT_EQ(navigationController->GetNumHistoryEntries(), 2);

	navigationController->GoBack();

	EXPECT_FALSE(navigationController->CanGoBack());
	EXPECT_TRUE(navigationController->CanGoForward());
	EXPECT_EQ(navigationController->GetNumHistoryEntries(), 2);
	EXPECT_EQ(navigationController->GetCurrentIndex(), 0);

	navigationController->GoForward();

	EXPECT_TRUE(navigationController->CanGoBack());
	EXPECT_FALSE(navigationController->CanGoForward());
	EXPECT_EQ(navigationController->GetNumHistoryEntries(), 2);
	EXPECT_EQ(navigationController->GetCurrentIndex(), 1);

	m_shellBrowser.NavigateToPath(L"C:\\Fake3");

	EXPECT_TRUE(navigationController->CanGoBack());
	EXPECT_FALSE(navigationController->CanGoForward());
	EXPECT_EQ(navigationController->GetNumHistoryEntries(), 3);

	// Go back to the first entry.
	navigationController->GoToOffset(-2);

	EXPECT_FALSE(navigationController->CanGoBack());
	EXPECT_TRUE(navigationController->CanGoForward());
	EXPECT_EQ(navigationController->GetNumHistoryEntries(), 3);

	m_shellBrowser.NavigateToPath(L"C:\\Fake4");

	// Performing a new navigation should have cleared the forward history.
	EXPECT_TRUE(navigationController->CanGoBack());
	EXPECT_FALSE(navigationController->CanGoForward());
	EXPECT_EQ(navigationController->GetNumHistoryEntries(), 2);
}

TEST_F(ShellNavigationControllerTest, BackForwardItemSelection)
{
	m_shellBrowser.NavigateToPath(L"C:\\Fake1");

	auto *navigationController = GetNavigationController();

	auto *currentEntry = navigationController->GetCurrentEntry();
	ASSERT_NE(currentEntry, nullptr);

	std::vector<PidlAbsolute> selectedItems1 = { CreateSimplePidlForTest(L"C:\\Fake1\\item1"),
		CreateSimplePidlForTest(L"C:\\Fake1\\item2") };
	currentEntry->SetSelectedItems(selectedItems1);
	EXPECT_EQ(currentEntry->GetSelectedItems(), selectedItems1);

	m_shellBrowser.NavigateToPath(L"C:\\Fake2");

	currentEntry = navigationController->GetCurrentEntry();
	ASSERT_NE(currentEntry, nullptr);

	std::vector<PidlAbsolute> selectedItems2 = { CreateSimplePidlForTest(L"C:\\Fake2\\item1"),
		CreateSimplePidlForTest(L"C:\\Fake2\\item2") };
	currentEntry->SetSelectedItems(selectedItems2);
	EXPECT_EQ(currentEntry->GetSelectedItems(), selectedItems2);

	navigationController->GoBack();

	currentEntry = navigationController->GetCurrentEntry();
	ASSERT_NE(currentEntry, nullptr);
	EXPECT_EQ(currentEntry->GetSelectedItems(), selectedItems1);

	navigationController->GoForward();

	currentEntry = navigationController->GetCurrentEntry();
	ASSERT_NE(currentEntry, nullptr);
	EXPECT_EQ(currentEntry->GetSelectedItems(), selectedItems2);
}

TEST_F(ShellNavigationControllerTest, RetrieveHistory)
{
	m_shellBrowser.NavigateToPath(L"C:\\Fake1");

	auto *navigationController = GetNavigationController();

	auto history = navigationController->GetBackHistory();
	EXPECT_TRUE(history.empty());

	history = navigationController->GetForwardHistory();
	EXPECT_TRUE(history.empty());

	m_shellBrowser.NavigateToPath(L"C:\\Fake2");

	history = navigationController->GetBackHistory();
	EXPECT_EQ(history.size(), 1U);

	history = navigationController->GetForwardHistory();
	EXPECT_TRUE(history.empty());

	m_shellBrowser.NavigateToPath(L"C:\\Fake3");

	history = navigationController->GetBackHistory();
	EXPECT_EQ(history.size(), 2U);

	history = navigationController->GetForwardHistory();
	EXPECT_TRUE(history.empty());

	navigationController->GoBack();

	history = navigationController->GetBackHistory();
	EXPECT_EQ(history.size(), 1U);

	history = navigationController->GetForwardHistory();
	EXPECT_EQ(history.size(), 1U);
}

TEST_F(ShellNavigationControllerTest, GoUp)
{
	auto *navigationController = GetNavigationController();

	PidlAbsolute pidlFolder = CreateSimplePidlForTest(L"C:\\Fake");
	auto navigateParamsFolder = NavigateParams::Normal(pidlFolder.Raw());
	navigationController->Navigate(navigateParamsFolder);

	EXPECT_TRUE(navigationController->CanGoUp());

	navigationController->GoUp();

	auto entry = navigationController->GetCurrentEntry();
	ASSERT_NE(entry, nullptr);

	PidlAbsolute pidlParent = CreateSimplePidlForTest(L"C:\\");
	EXPECT_EQ(entry->GetPidl(), pidlParent);

	// The desktop folder is the root of the shell namespace.
	PidlAbsolute pidlDesktop;
	HRESULT hr = SHGetKnownFolderIDList(FOLDERID_Desktop, KF_FLAG_DEFAULT, nullptr,
		PidlOutParam(pidlDesktop));
	ASSERT_HRESULT_SUCCEEDED(hr);

	auto navigateParamsDesktop = NavigateParams::Normal(pidlDesktop.Raw());
	navigationController->Navigate(navigateParamsDesktop);

	EXPECT_FALSE(navigationController->CanGoUp());

	// This should have no effect.
	navigationController->GoUp();

	entry = navigationController->GetCurrentEntry();
	ASSERT_NE(entry, nullptr);
	EXPECT_EQ(entry->GetPidl(), pidlDesktop);
}

TEST_F(ShellNavigationControllerTest, HistoryEntries)
{
	auto *navigationController = GetNavigationController();

	EXPECT_EQ(navigationController->GetCurrentIndex(), 0);

	// There should always be a current entry.
	auto entry = navigationController->GetCurrentEntry();
	ASSERT_NE(entry, nullptr);

	EXPECT_EQ(navigationController->GetIndexOfEntry(entry), 0);
	EXPECT_EQ(navigationController->GetEntryById(entry->GetId()), entry);

	PidlAbsolute pidl1 = CreateSimplePidlForTest(L"C:\\Fake1");
	auto navigateParams1 = NavigateParams::Normal(pidl1.Raw());
	navigationController->Navigate(navigateParams1);

	EXPECT_EQ(navigationController->GetCurrentIndex(), 0);

	entry = navigationController->GetCurrentEntry();
	ASSERT_NE(entry, nullptr);
	EXPECT_EQ(entry->GetPidl(), pidl1);

	EXPECT_EQ(navigationController->GetIndexOfEntry(entry), 0);
	EXPECT_EQ(navigationController->GetEntryById(entry->GetId()), entry);

	PidlAbsolute pidl2 = CreateSimplePidlForTest(L"C:\\Fake2");
	auto navigateParams2 = NavigateParams::Normal(pidl2.Raw());
	navigationController->Navigate(navigateParams2);

	entry = navigationController->GetCurrentEntry();
	ASSERT_NE(entry, nullptr);
	EXPECT_EQ(entry->GetPidl(), pidl2);

	EXPECT_EQ(navigationController->GetIndexOfEntry(entry), 1);
	EXPECT_EQ(navigationController->GetEntryById(entry->GetId()), entry);

	EXPECT_EQ(navigationController->GetCurrentIndex(), 1);
	EXPECT_EQ(navigationController->GetCurrentEntry(), navigationController->GetEntryAtIndex(1));

	entry = navigationController->GetEntryAtIndex(0);
	ASSERT_NE(entry, nullptr);
	EXPECT_EQ(entry->GetPidl(), pidl1);
}

TEST_F(ShellNavigationControllerTest, SetNavigationTargetMode)
{
	PidlAbsolute pidl1 = CreateSimplePidlForTest(L"C:\\Fake1");
	auto params = NavigateParams::Normal(pidl1.Raw());

	MockFunction<void(const NavigationRequest *request)> navigationStartedCallback;
	m_navigationEvents.AddStartedObserver(navigationStartedCallback.AsStdFunction(),
		NavigationEventScope::ForShellBrowser(m_shellBrowser));

	EXPECT_CALL(navigationStartedCallback, Call(NavigateParamsMatch(params)));

	// By default, all navigations should proceed in the current tab.
	EXPECT_CALL(m_browser, OpenItem(An<PCIDLIST_ABSOLUTE>(), _)).Times(0);

	auto *navigationController = GetNavigationController();
	EXPECT_EQ(navigationController->GetNavigationTargetMode(), NavigationTargetMode::Normal);

	navigationController->Navigate(params);

	navigationController->SetNavigationTargetMode(NavigationTargetMode::ForceNewTab);
	EXPECT_EQ(navigationController->GetNavigationTargetMode(), NavigationTargetMode::ForceNewTab);

	// The navigation is to the same directory, which is treated as an implicit refresh, so the
	// following fields are expected to be set.
	auto expectedParams = params;
	expectedParams.historyEntryType = HistoryEntryType::ReplaceCurrentEntry;
	expectedParams.overrideNavigationTargetMode = true;

	// Although the navigation mode has been set, the navigation is an implicit refresh and should
	// always proceed in the same tab.
	EXPECT_CALL(navigationStartedCallback, Call(NavigateParamsMatch(expectedParams)));
	EXPECT_CALL(m_browser, OpenItem(An<PCIDLIST_ABSOLUTE>(), _)).Times(0);

	navigationController->Navigate(params);

	PidlAbsolute pidl2 = CreateSimplePidlForTest(L"C:\\Fake2");
	params = NavigateParams::Normal(pidl2.Raw());

	// This is a navigation to a different directory, so the navigation mode above should now apply.
	EXPECT_CALL(navigationStartedCallback, Call(_)).Times(0);
	EXPECT_CALL(m_browser, OpenItem(params.pidl.Raw(), _));

	navigationController->Navigate(params);

	PidlAbsolute pidl3 = CreateSimplePidlForTest(L"C:\\Fake3");
	params = NavigateParams::Normal(pidl3.Raw());
	params.overrideNavigationTargetMode = true;

	// The navigation explicitly overrides the navigation mode, so this navigation should proceed in
	// the tab, even though a navigation mode was applied above.
	EXPECT_CALL(navigationStartedCallback, Call(NavigateParamsMatch(params)));
	EXPECT_CALL(m_browser, OpenItem(An<PCIDLIST_ABSOLUTE>(), _)).Times(0);

	navigationController->Navigate(params);
}

TEST_F(ShellNavigationControllerTest, SetNavigationTargetModeFirstNavigation)
{
	auto *navigationController = GetNavigationController();
	navigationController->SetNavigationTargetMode(NavigationTargetMode::ForceNewTab);

	PidlAbsolute pidl1 = CreateSimplePidlForTest(L"C:\\Fake1");
	auto params = NavigateParams::Normal(pidl1.Raw());

	MockFunction<void(const NavigationRequest *request)> navigationStartedCallback;
	m_navigationEvents.AddStartedObserver(navigationStartedCallback.AsStdFunction(),
		NavigationEventScope::ForShellBrowser(m_shellBrowser));

	// The first navigation in a tab should always take place within that tab, regardless of the
	// navigation mode in effect.
	EXPECT_CALL(navigationStartedCallback, Call(NavigateParamsMatch(params)));
	EXPECT_CALL(m_browser, OpenItem(An<PCIDLIST_ABSOLUTE>(), _)).Times(0);

	navigationController->Navigate(params);

	PidlAbsolute pidl2 = CreateSimplePidlForTest(L"C:\\Fake2");
	auto params2 = NavigateParams::Normal(pidl2.Raw());

	// Subsequent navigations should then open in a new tab when necessary.
	EXPECT_CALL(navigationStartedCallback, Call(NavigateParamsMatch(params2))).Times(0);
	EXPECT_CALL(m_browser, OpenItem(params2.pidl.Raw(), _));

	navigationController->Navigate(params2);
}

TEST_F(ShellNavigationControllerTest, HistoryEntryTypes)
{
	m_shellBrowser.NavigateToPath(L"C:\\Fake1", HistoryEntryType::AddEntry);

	PidlAbsolute pidl2;
	m_shellBrowser.NavigateToPath(L"C:\\Fake2", HistoryEntryType::ReplaceCurrentEntry, &pidl2);

	auto *navigationController = GetNavigationController();

	// The second navigation should have replaced the entry from the first navigation, so there
	// should only be a single entry.
	EXPECT_EQ(navigationController->GetNumHistoryEntries(), 1);
	EXPECT_EQ(navigationController->GetCurrentIndex(), 0);

	auto entry = navigationController->GetCurrentEntry();
	ASSERT_NE(entry, nullptr);
	EXPECT_EQ(entry->GetPidl(), pidl2);

	PidlAbsolute pidl3;
	m_shellBrowser.NavigateToPath(L"C:\\Fake3", HistoryEntryType::AddEntry, &pidl3);

	EXPECT_EQ(navigationController->GetNumHistoryEntries(), 2);
	EXPECT_EQ(navigationController->GetCurrentIndex(), 1);

	entry = navigationController->GetCurrentEntry();
	ASSERT_NE(entry, nullptr);
	EXPECT_EQ(entry->GetPidl(), pidl3);

	m_shellBrowser.NavigateToPath(L"C:\\Fake4", HistoryEntryType::None);

	EXPECT_EQ(navigationController->GetNumHistoryEntries(), 2);
	EXPECT_EQ(navigationController->GetCurrentIndex(), 1);

	// No entry should have been added, so the current entry should still be the same as it was
	// previously.
	entry = navigationController->GetCurrentEntry();
	ASSERT_NE(entry, nullptr);
	EXPECT_EQ(entry->GetPidl(), pidl3);
}

TEST_F(ShellNavigationControllerTest, ReplacePreviousHistoryEntry)
{
	m_shellBrowser.NavigateToPath(L"C:\\Fake1", HistoryEntryType::AddEntry);
	m_shellBrowser.NavigateToPath(L"C:\\Fake2", HistoryEntryType::AddEntry);

	auto *navigationController = GetNavigationController();

	auto *entry = navigationController->GetEntryAtIndex(0);
	ASSERT_NE(entry, nullptr);
	int originalEntryId = entry->GetId();

	auto params = NavigateParams::History(entry);
	params.historyEntryType = HistoryEntryType::ReplaceCurrentEntry;
	navigationController->Navigate(params);

	auto *updatedEntry = navigationController->GetEntryAtIndex(0);
	ASSERT_NE(updatedEntry, nullptr);

	// Navigating to the entry should have resulted in it being replaced, so the ID of the first
	// entry should have changed.
	EXPECT_NE(updatedEntry->GetId(), originalEntryId);
}

TEST_F(ShellNavigationControllerTest, HistoryEntryTypeFirstNavigation)
{
	PidlAbsolute pidl;
	m_shellBrowser.NavigateToPath(L"C:\\Fake", HistoryEntryType::None, &pidl);

	auto *navigationController = GetNavigationController();

	// The first navigation in a tab should always result in a history entry being added, regardless
	// of what's requested.
	EXPECT_EQ(navigationController->GetNumHistoryEntries(), 1);
	EXPECT_EQ(navigationController->GetCurrentIndex(), 0);

	auto entry = navigationController->GetCurrentEntry();
	ASSERT_NE(entry, nullptr);
	EXPECT_EQ(entry->GetPidl(), pidl);
}

TEST_F(ShellNavigationControllerTest, SlotOrdering)
{
	MockFunction<void(const NavigationRequest *request)> navigationCommittedCallback;
	ON_CALL(navigationCommittedCallback, Call(_))
		.WillByDefault(
			[this](const NavigationRequest *request)
			{
				UNREFERENCED_PARAMETER(request);

				auto *navigationController = GetNavigationController();

				// By the time this slot runs, the navigation controller should have already set up
				// the current entry. That is, the slot set up by the navigation controller should
				// always run before a slot like this.
				auto entry = navigationController->GetCurrentEntry();
				ASSERT_NE(entry, nullptr);
				EXPECT_EQ(entry->GetInitialNavigationType(),
					HistoryEntry::InitialNavigationType::NonInitial);
			});

	m_navigationEvents.AddCommittedObserver(navigationCommittedCallback.AsStdFunction(),
		NavigationEventScope::ForShellBrowser(m_shellBrowser), boost::signals2::at_front);
	EXPECT_CALL(navigationCommittedCallback, Call(_));

	m_shellBrowser.NavigateToPath(L"C:\\Fake");
}

TEST_F(ShellNavigationControllerTest, FirstNavigation)
{
	auto *navigationController = GetNavigationController();

	// There should always be an initial entry.
	EXPECT_EQ(navigationController->GetNumHistoryEntries(), 1);
	EXPECT_EQ(navigationController->GetCurrentIndex(), 0);

	auto entry = navigationController->GetCurrentEntry();
	ASSERT_NE(entry, nullptr);
	EXPECT_EQ(entry->GetInitialNavigationType(), HistoryEntry::InitialNavigationType::Initial);
	EXPECT_TRUE(entry->IsInitialEntry());
	int originalEntryId = entry->GetId();

	PidlAbsolute pidl;
	m_shellBrowser.NavigateToPath(L"C:\\Fake", HistoryEntryType::None, &pidl);

	// The navigation above should result in the initial entry being replaced. There should still
	// only be a single entry.
	auto updatedEntry = navigationController->GetCurrentEntry();
	ASSERT_NE(updatedEntry, nullptr);
	EXPECT_NE(updatedEntry->GetId(), originalEntryId);
	EXPECT_EQ(updatedEntry->GetInitialNavigationType(),
		HistoryEntry::InitialNavigationType::NonInitial);
	EXPECT_FALSE(updatedEntry->IsInitialEntry());
	EXPECT_EQ(updatedEntry->GetPidl(), pidl);
}

class ShellNavigationControllerPreservedTest : public Test
{
protected:
	std::unique_ptr<PreservedShellBrowser> BuildPreservedShellBrowser(int currentEntry)
	{
		std::vector<std::unique_ptr<PreservedHistoryEntry>> history;
		history.push_back(
			std::make_unique<PreservedHistoryEntry>(CreateSimplePidlForTest(L"C:\\Fake1")));
		history.push_back(
			std::make_unique<PreservedHistoryEntry>(CreateSimplePidlForTest(L"C:\\Fake2")));

		return std::make_unique<PreservedShellBrowser>(FolderSettings{}, FolderColumns{},
			std::move(history), currentEntry);
	}

	NavigationEvents m_navigationEvents;
	BrowserWindowMock m_browser;
};

TEST_F(ShellNavigationControllerPreservedTest, FirstIndexIsCurrent)
{
	auto preservedShellBrowser = BuildPreservedShellBrowser(0);
	auto shellBrowser =
		std::make_unique<ShellBrowserFake>(&m_browser, &m_navigationEvents, *preservedShellBrowser);
	auto *navigationController = shellBrowser->GetNavigationController();

	EXPECT_EQ(navigationController->GetCurrentIndex(), 0);
	EXPECT_FALSE(navigationController->CanGoBack());
	EXPECT_TRUE(navigationController->CanGoForward());
	EXPECT_EQ(navigationController->GetNumHistoryEntries(), 2);
}

TEST_F(ShellNavigationControllerPreservedTest, SecondIndexIsCurrent)
{
	auto preservedShellBrowser = BuildPreservedShellBrowser(1);
	auto shellBrowser =
		std::make_unique<ShellBrowserFake>(&m_browser, &m_navigationEvents, *preservedShellBrowser);
	auto *navigationController = shellBrowser->GetNavigationController();

	EXPECT_EQ(navigationController->GetCurrentIndex(), 1);
	EXPECT_TRUE(navigationController->CanGoBack());
	EXPECT_FALSE(navigationController->CanGoForward());
	EXPECT_EQ(navigationController->GetNumHistoryEntries(), 2);
}

TEST_F(ShellNavigationControllerPreservedTest, CheckEntries)
{
	auto preservedShellBrowser = BuildPreservedShellBrowser(0);
	auto shellBrowser =
		std::make_unique<ShellBrowserFake>(&m_browser, &m_navigationEvents, *preservedShellBrowser);
	auto *navigationController = shellBrowser->GetNavigationController();

	for (size_t i = 0; i < preservedShellBrowser->history.size(); i++)
	{
		auto entry = navigationController->GetEntryAtIndex(static_cast<int>(i));
		ASSERT_NE(entry, nullptr);
		EXPECT_EQ(entry->GetPidl(), preservedShellBrowser->history[i]->GetPidl());
	}
}
