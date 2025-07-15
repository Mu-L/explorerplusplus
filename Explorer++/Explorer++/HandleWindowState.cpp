// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "Explorer++.h"
#include "App.h"
#include "Config.h"
#include "FeatureList.h"
#include "MainResource.h"
#include "ShellBrowser/ShellBrowserImpl.h"
#include "ShellBrowser/ShellNavigationController.h"
#include "ShellBrowser/ViewModes.h"
#include "ShellTreeView/ShellTreeView.h"
#include "SortMenuBuilder.h"
#include "TabContainer.h"
#include "../Helper/MenuHelper.h"

void Explorerplusplus::UpdateWindowStates(const Tab &tab)
{
	UpdateDisplayWindow(tab);
}

/*
 * Set the state of the items in the main
 * program menu.
 */
void Explorerplusplus::SetProgramMenuItemStates(HMENU hProgramMenu)
{
	const Tab &tab = GetActivePane()->GetTabContainer()->GetSelectedTab();

	ViewMode viewMode = tab.GetShellBrowserImpl()->GetViewMode();

	int numSelected = tab.GetShellBrowserImpl()->GetNumSelected();
	bool anySelected = (numSelected > 0);

	MenuHelper::EnableItem(hProgramMenu, IDM_FILE_COPYITEMPATH,
		m_commandController.IsCommandEnabled(IDM_FILE_COPYITEMPATH));
	MenuHelper::EnableItem(hProgramMenu, IDM_FILE_COPYUNIVERSALFILEPATHS,
		m_commandController.IsCommandEnabled(IDM_FILE_COPYUNIVERSALFILEPATHS));
	MenuHelper::EnableItem(hProgramMenu, IDM_FILE_SETFILEATTRIBUTES, AnyItemsSelected());
	MenuHelper::EnableItem(hProgramMenu, IDM_FILE_OPENCOMMANDPROMPT,
		m_commandController.IsCommandEnabled(IDM_FILE_OPENCOMMANDPROMPT));
	MenuHelper::EnableItem(hProgramMenu, IDM_FILE_OPENCOMMANDPROMPTADMINISTRATOR,
		m_commandController.IsCommandEnabled(IDM_FILE_OPENCOMMANDPROMPTADMINISTRATOR));
	MenuHelper::EnableItem(hProgramMenu, IDM_FILE_SAVEDIRECTORYLISTING,
		m_commandController.IsCommandEnabled(IDM_FILE_SAVEDIRECTORYLISTING));
	MenuHelper::EnableItem(hProgramMenu, IDM_FILE_COPYCOLUMNTEXT,
		anySelected && (viewMode == +ViewMode::Details));

	MenuHelper::EnableItem(hProgramMenu, IDM_FILE_RENAME,
		m_commandController.IsCommandEnabled(IDM_FILE_RENAME));
	MenuHelper::EnableItem(hProgramMenu, IDM_FILE_DELETE,
		m_commandController.IsCommandEnabled(IDM_FILE_DELETE));
	MenuHelper::EnableItem(hProgramMenu, IDM_FILE_DELETEPERMANENTLY,
		m_commandController.IsCommandEnabled(IDM_FILE_DELETEPERMANENTLY));
	MenuHelper::EnableItem(hProgramMenu, IDM_FILE_PROPERTIES,
		m_commandController.IsCommandEnabled(IDM_FILE_PROPERTIES));

	MenuHelper::EnableItem(hProgramMenu, IDM_EDIT_UNDO, m_fileActionHandler.CanUndo());
	MenuHelper::EnableItem(hProgramMenu, IDM_EDIT_PASTE, CanPaste(PasteType::Normal));
	MenuHelper::EnableItem(hProgramMenu, IDM_EDIT_PASTESHORTCUT, CanPaste(PasteType::Shortcut));
	MenuHelper::EnableItem(hProgramMenu, IDM_EDIT_PASTEHARDLINK, CanPasteLink());
	MenuHelper::EnableItem(hProgramMenu, IDM_EDIT_PASTE_SYMBOLIC_LINK, CanPasteLink());

	/* The following menu items are only enabled when one
	or more files are selected (they represent file
	actions, cut/copy, etc). */
	MenuHelper::EnableItem(hProgramMenu, IDM_EDIT_CUT,
		m_commandController.IsCommandEnabled(IDM_EDIT_CUT));
	MenuHelper::EnableItem(hProgramMenu, IDM_EDIT_COPY,
		m_commandController.IsCommandEnabled(IDM_EDIT_COPY));
	MenuHelper::EnableItem(hProgramMenu, IDM_EDIT_MOVETOFOLDER,
		m_commandController.IsCommandEnabled(IDM_EDIT_MOVETOFOLDER));
	MenuHelper::EnableItem(hProgramMenu, IDM_EDIT_COPYTOFOLDER,
		m_commandController.IsCommandEnabled(IDM_EDIT_COPYTOFOLDER));
	MenuHelper::EnableItem(hProgramMenu, IDM_EDIT_WILDCARDDESELECT,
		m_commandController.IsCommandEnabled(IDM_EDIT_WILDCARDDESELECT));
	MenuHelper::EnableItem(hProgramMenu, IDM_EDIT_SELECTNONE,
		m_commandController.IsCommandEnabled(IDM_EDIT_SELECTNONE));
	MenuHelper::EnableItem(hProgramMenu, IDM_EDIT_RESOLVELINK, anySelected);

	if (m_app->GetFeatureList()->IsEnabled(Feature::DualPane))
	{
		MenuHelper::CheckItem(hProgramMenu, IDM_VIEW_DUAL_PANE, m_config->dualPane);
	}

	MenuHelper::CheckItem(hProgramMenu, IDM_VIEW_STATUSBAR, m_config->showStatusBar.get());
	MenuHelper::CheckItem(hProgramMenu, IDM_VIEW_FOLDERS, m_config->showFolders.get());
	MenuHelper::CheckItem(hProgramMenu, IDM_VIEW_DISPLAYWINDOW, m_config->showDisplayWindow.get());
	MenuHelper::CheckItem(hProgramMenu, IDM_VIEW_TOOLBARS_ADDRESS_BAR,
		m_config->showAddressBar.get());
	MenuHelper::CheckItem(hProgramMenu, IDM_VIEW_TOOLBARS_MAIN_TOOLBAR,
		m_config->showMainToolbar.get());
	MenuHelper::CheckItem(hProgramMenu, IDM_VIEW_TOOLBARS_BOOKMARKS_TOOLBAR,
		m_config->showBookmarksToolbar.get());
	MenuHelper::CheckItem(hProgramMenu, IDM_VIEW_TOOLBARS_DRIVES_TOOLBAR,
		m_config->showDrivesToolbar.get());
	MenuHelper::CheckItem(hProgramMenu, IDM_VIEW_TOOLBARS_APPLICATION_TOOLBAR,
		m_config->showApplicationToolbar.get());
	MenuHelper::CheckItem(hProgramMenu, IDM_VIEW_TOOLBARS_LOCK_TOOLBARS,
		m_config->lockToolbars.get());

	MenuHelper::EnableItem(hProgramMenu, IDM_VIEW_DECREASE_TEXT_SIZE,
		m_commandController.IsCommandEnabled(IDM_VIEW_DECREASE_TEXT_SIZE));
	MenuHelper::EnableItem(hProgramMenu, IDM_VIEW_INCREASE_TEXT_SIZE,
		m_commandController.IsCommandEnabled(IDM_VIEW_INCREASE_TEXT_SIZE));

	MenuHelper::CheckItem(hProgramMenu, IDM_VIEW_SHOWHIDDENFILES,
		tab.GetShellBrowserImpl()->GetShowHidden());
	MenuHelper::CheckItem(hProgramMenu, IDM_FILTER_ENABLE_FILTER,
		tab.GetShellBrowserImpl()->IsFilterEnabled());

	MenuHelper::EnableItem(hProgramMenu, IDM_ACTIONS_NEWFOLDER,
		m_commandController.IsCommandEnabled(IDM_ACTIONS_NEWFOLDER));
	MenuHelper::EnableItem(hProgramMenu, IDM_ACTIONS_SPLITFILE,
		m_commandController.IsCommandEnabled(IDM_ACTIONS_SPLITFILE));
	MenuHelper::EnableItem(hProgramMenu, IDM_ACTIONS_MERGEFILES,
		m_commandController.IsCommandEnabled(IDM_ACTIONS_MERGEFILES));
	MenuHelper::EnableItem(hProgramMenu, IDM_ACTIONS_DESTROYFILES, anySelected);

	UINT itemToCheck = GetViewModeMenuId(viewMode);
	CheckMenuRadioItem(hProgramMenu, IDM_VIEW_EXTRALARGEICONS, IDM_VIEW_TILES, itemToCheck,
		MF_BYCOMMAND);

	MenuHelper::EnableItem(hProgramMenu, IDM_GO_BACK,
		m_commandController.IsCommandEnabled(IDM_GO_BACK));
	MenuHelper::EnableItem(hProgramMenu, IDM_GO_FORWARD,
		m_commandController.IsCommandEnabled(IDM_GO_FORWARD));
	MenuHelper::EnableItem(hProgramMenu, IDM_GO_UP,
		m_commandController.IsCommandEnabled(IDM_GO_UP));

	MenuHelper::EnableItem(hProgramMenu, IDM_VIEW_AUTOSIZECOLUMNS,
		m_commandController.IsCommandEnabled(IDM_VIEW_AUTOSIZECOLUMNS));

	if (viewMode == +ViewMode::Details)
	{
		/* Disable auto arrange menu item. */
		MenuHelper::EnableItem(hProgramMenu, IDM_VIEW_AUTOARRANGE, FALSE);
		MenuHelper::CheckItem(hProgramMenu, IDM_VIEW_AUTOARRANGE, FALSE);

		MenuHelper::EnableItem(hProgramMenu, IDM_VIEW_GROUPBY, TRUE);
	}
	else if (viewMode == +ViewMode::List)
	{
		/* Disable group menu item. */
		MenuHelper::EnableItem(hProgramMenu, IDM_VIEW_GROUPBY, FALSE);

		/* Disable auto arrange menu item. */
		MenuHelper::EnableItem(hProgramMenu, IDM_VIEW_AUTOARRANGE, FALSE);
		MenuHelper::CheckItem(hProgramMenu, IDM_VIEW_AUTOARRANGE, FALSE);
	}
	else
	{
		MenuHelper::EnableItem(hProgramMenu, IDM_VIEW_GROUPBY, TRUE);

		MenuHelper::EnableItem(hProgramMenu, IDM_VIEW_AUTOARRANGE, TRUE);
		MenuHelper::CheckItem(hProgramMenu, IDM_VIEW_AUTOARRANGE,
			tab.GetShellBrowser()->IsAutoArrangeEnabled());
	}

	SortMenuBuilder sortMenuBuilder(m_app->GetResourceLoader());
	auto [sortByMenu, groupByMenu] = sortMenuBuilder.BuildMenus(tab);

	MenuHelper::AttachSubMenu(hProgramMenu, std::move(sortByMenu), IDM_VIEW_SORTBY, FALSE);
	MenuHelper::AttachSubMenu(hProgramMenu, std::move(groupByMenu), IDM_VIEW_GROUPBY, FALSE);
}
