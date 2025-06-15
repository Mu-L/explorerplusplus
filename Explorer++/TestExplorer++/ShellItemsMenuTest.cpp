// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "ShellItemsMenu.h"
#include "AcceleratorManager.h"
#include "BrowserWindowMock.h"
#include "MenuViewFake.h"
#include "ShellIconLoaderFake.h"
#include "ShellTestHelper.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace testing;

namespace
{

std::wstring GetPathForItem(const std::wstring &name)
{
	return L"C:\\" + name;
}

std::wstring GetNameForItem(size_t index)
{
	return L"Fake" + std::to_wstring(index);
}

std::vector<PidlAbsolute> BuildPidlCollection(int size)
{
	std::vector<PidlAbsolute> pidls;

	for (int i = 0; i < size; i++)
	{
		auto path = GetPathForItem(GetNameForItem(i));
		pidls.push_back(CreateSimplePidlForTest(path));
	}

	return pidls;
}

}

class ShellItemsMenuTest : public Test
{
protected:
	std::unique_ptr<ShellItemsMenu> BuildMenu(MenuView *menuView,
		const std::vector<PidlAbsolute> &pidls)
	{
		return std::make_unique<ShellItemsMenu>(menuView, &m_acceleratorManager, pidls,
			&m_browserWindow, &m_shellIconLoader);
	}

	std::unique_ptr<ShellItemsMenu> BuildMenu(MenuView *menuView,
		const std::vector<PidlAbsolute> &pidls, UINT menuStartId, UINT menuEndId)
	{
		return std::make_unique<ShellItemsMenu>(menuView, &m_acceleratorManager, pidls,
			&m_browserWindow, &m_shellIconLoader, menuStartId, menuEndId);
	}

	void CheckItemDetails(const MenuViewFake *menuView, const std::vector<PidlAbsolute> &pidls)
	{
		ASSERT_EQ(static_cast<size_t>(menuView->GetItemCount()), pidls.size());

		for (size_t i = 0; i < pidls.size(); i++)
		{
			auto id = menuView->GetItemId(static_cast<int>(i));
			auto name = GetNameForItem(i);
			EXPECT_EQ(menuView->GetItemText(id), name);
			EXPECT_NE(menuView->GetItemBitmap(id), nullptr);
			EXPECT_EQ(menuView->GetItemHelpText(id), GetPathForItem(name));
		}
	}

	void CheckIdRange(UINT startId, UINT endId, UINT expectedStartId, UINT expectedEndId)
	{
		MenuViewFake menuView;
		auto pidls = BuildPidlCollection(1);
		auto menu = BuildMenu(&menuView, pidls, startId, endId);
		EXPECT_EQ(menu->GetIdRange(), MenuBase::IdRange(expectedStartId, expectedEndId));
	}

	AcceleratorManager m_acceleratorManager;
	BrowserWindowMock m_browserWindow;
	ShellIconLoaderFake m_shellIconLoader;
};

TEST_F(ShellItemsMenuTest, CheckItems)
{
	MenuViewFake menuView;
	menuView.OnMenuWillShowForDpi(USER_DEFAULT_SCREEN_DPI);
	auto pidls = BuildPidlCollection(3);
	auto menu = BuildMenu(&menuView, pidls);

	CheckItemDetails(&menuView, pidls);
}

TEST_F(ShellItemsMenuTest, MaxItems)
{
	MenuViewFake menuView;
	menuView.OnMenuWillShowForDpi(USER_DEFAULT_SCREEN_DPI);
	auto pidls = BuildPidlCollection(3);
	auto menu = BuildMenu(&menuView, pidls, 1, 2);

	// The menu only has a single ID it can assign from the provided range of [1,2). So, although 3
	// items were passed in, only the first item should be added to the menu.
	EXPECT_EQ(menuView.GetItemCount(), 1);
	EXPECT_EQ(menuView.GetItemText(menuView.GetItemId(0)), GetNameForItem(0));
}

TEST_F(ShellItemsMenuTest, GetIdRange)
{
	CheckIdRange(20, 100, 20, 100);

	// 0 isn't a valid start ID, so the final ID range should start from 1.
	CheckIdRange(0, 46, 1, 46);

	// 0 isn't a valid end ID either, so the end ID should be set to the start ID.
	CheckIdRange(11, 0, 11, 11);

	CheckIdRange(0, 0, 1, 1);

	// The end ID should always be greater or equal to the start ID.
	CheckIdRange(200, 148, 200, 200);
}

TEST_F(ShellItemsMenuTest, RebuildMenu)
{
	MenuViewFake menuView;
	menuView.OnMenuWillShowForDpi(USER_DEFAULT_SCREEN_DPI);
	auto pidls = BuildPidlCollection(3);
	auto menu = BuildMenu(&menuView, pidls);

	auto updatedPidls = BuildPidlCollection(5);
	menu->RebuildMenu(updatedPidls);

	CheckItemDetails(&menuView, updatedPidls);
}

class ShellItemsMenuSelectionTest : public Test
{
protected:
	enum class SelectionType
	{
		Click,
		MiddleClick
	};

	ShellItemsMenuSelectionTest() :
		m_pidls(BuildPidlCollection(3)),
		m_menu(&m_menuView, &m_acceleratorManager, m_pidls, &m_browserWindow, &m_shellIconLoader)
	{
	}

	void TestSelection(SelectionType selectionType)
	{
		for (size_t i = 0; i < m_pidls.size(); i++)
		{
			TestSelectionForItem(i, selectionType);
		}
	}

private:
	void TestSelectionForItem(size_t index, SelectionType selectionType)
	{
		auto disposition = (selectionType == SelectionType::Click)
			? OpenFolderDisposition::CurrentTab
			: OpenFolderDisposition::NewTabDefault;

		EXPECT_CALL(m_browserWindow,
			OpenItem(TypedEq<PCIDLIST_ABSOLUTE>(m_pidls[index]), disposition));

		auto id = m_menuView.GetItemId(static_cast<int>(index));

		if (selectionType == SelectionType::Click)
		{
			m_menuView.SelectItem(id, false, false);
		}
		else
		{
			m_menuView.MiddleClickItem(id, false, false);
		}
	}

	MenuViewFake m_menuView;
	AcceleratorManager m_acceleratorManager;
	std::vector<PidlAbsolute> m_pidls;
	BrowserWindowMock m_browserWindow;
	ShellIconLoaderFake m_shellIconLoader;
	ShellItemsMenu m_menu;
};

TEST_F(ShellItemsMenuSelectionTest, Selection)
{
	TestSelection(SelectionType::Click);
	TestSelection(SelectionType::MiddleClick);
}
