// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "Explorer++.h"
#include "AddressBar.h"
#include "AddressBarView.h"
#include "App.h"
#include "Application.h"
#include "ApplicationEditorDialog.h"
#include "ApplicationToolbar.h"
#include "ApplicationToolbarView.h"
#include "Bookmarks/BookmarkHelper.h"
#include "Bookmarks/UI/BookmarksMainMenu.h"
#include "Bookmarks/UI/BookmarksToolbar.h"
#include "Bookmarks/UI/ManageBookmarksDialog.h"
#include "Bookmarks/UI/Views/BookmarksToolbarView.h"
#include "ClipboardOperations.h"
#include "Config.h"
#include "DisplayWindow/DisplayWindow.h"
#include "DrivesToolbar.h"
#include "DrivesToolbarView.h"
#include "Explorer++_internal.h"
#include "HolderWindow.h"
#include "MainMenuSubMenuView.h"
#include "MainRebarView.h"
#include "MainResource.h"
#include "MainToolbar.h"
#include "MainToolbarButtons.h"
#include "MenuRanges.h"
#include "ModelessDialogHelper.h"
#include "ShellBrowser/ShellBrowserImpl.h"
#include "ShellBrowser/SortModes.h"
#include "ShellBrowser/ViewModes.h"
#include "SortModeMenuMappings.h"
#include "StatusBar.h"
#include "TabContainer.h"
#include "TabRestorer.h"
#include "TabRestorerMenu.h"
#include "ViewsMenuBuilder.h"
#include "../Helper/Controls.h"
#include "../Helper/ListViewHelper.h"
#include "../Helper/ShellHelper.h"
#include "../Helper/WindowHelper.h"

static const int FOLDER_SIZE_LINE_INDEX = 1;

LRESULT Explorerplusplus::WindowProcedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_ACTIVATE:
		if (OnActivate(LOWORD(wParam), HIWORD(wParam)))
		{
			return 0;
		}
		break;

	case WM_INITMENU:
		OnInitMenu(reinterpret_cast<HMENU>(wParam));
		break;

	case WM_EXITMENULOOP:
		OnExitMenuLoop(wParam);
		break;

	case WM_INITMENUPOPUP:
		OnInitMenuPopup(reinterpret_cast<HMENU>(wParam));
		break;

	case WM_UNINITMENUPOPUP:
		OnUninitMenuPopup(reinterpret_cast<HMENU>(wParam));
		break;

	case WM_MENUSELECT:
		MenuItemSelected(reinterpret_cast<HMENU>(lParam), LOWORD(wParam), HIWORD(wParam));
		break;

	case WM_MBUTTONUP:
		OnMenuMiddleButtonUp({ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) },
			WI_IsFlagSet(wParam, MK_CONTROL), WI_IsFlagSet(wParam, MK_SHIFT));
		break;

	case WM_MENURBUTTONUP:
	{
		POINT pt;
		DWORD messagePos = GetMessagePos();
		POINTSTOPOINT(pt, MAKEPOINTS(messagePos));
		OnMenuRightButtonUp(reinterpret_cast<HMENU>(lParam), static_cast<int>(wParam), pt);
	}
	break;

	case WM_TIMER:
		if (wParam == LISTVIEW_ITEM_CHANGED_TIMER_ID)
		{
			Tab &selectedTab = GetActivePane()->GetTabContainer()->GetSelectedTab();

			UpdateDisplayWindow(selectedTab);
			m_mainToolbar->UpdateToolbarButtonStates();

			KillTimer(m_hContainer, LISTVIEW_ITEM_CHANGED_TIMER_ID);
		}
		break;

	case WM_USER_DISPLAYWINDOWRESIZED:
		OnDisplayWindowResized(wParam);
		break;

	case WM_APP_FOLDERSIZECOMPLETED:
	{
		DWFolderSizeCompletion *pDWFolderSizeCompletion = nullptr;
		TCHAR szSizeString[64];
		TCHAR szTotalSize[64];
		BOOL bValid = FALSE;

		pDWFolderSizeCompletion = (DWFolderSizeCompletion *) wParam;

		std::list<DWFolderSize>::iterator itr;

		/* First, make sure we should still display the
		results (we won't if the listview selection has
		changed, or this folder size was calculated for
		a tab other than the current one). */
		for (itr = m_DWFolderSizes.begin(); itr != m_DWFolderSizes.end(); itr++)
		{
			if (itr->uId == pDWFolderSizeCompletion->uId)
			{
				if (itr->iTabId == GetActivePane()->GetTabContainer()->GetSelectedTab().GetId())
				{
					bValid = itr->bValid;
				}

				m_DWFolderSizes.erase(itr);

				break;
			}
		}

		if (bValid)
		{
			auto displayFormat = m_config->globalFolderSettings.forceSize
				? m_config->globalFolderSettings.sizeDisplayFormat
				: +SizeDisplayFormat::None;
			auto folderSizeText =
				FormatSizeString(pDWFolderSizeCompletion->liFolderSize.QuadPart, displayFormat);

			LoadString(m_app->GetResourceInstance(), IDS_GENERAL_TOTALSIZE, szTotalSize,
				std::size(szTotalSize));

			StringCchPrintf(szSizeString, std::size(szSizeString), _T("%s: %s"), szTotalSize,
				folderSizeText.c_str());

			/* TODO: The line index should be stored in some other (variable) way. */
			DisplayWindow_SetLine(m_displayWindow->GetHWND(), FOLDER_SIZE_LINE_INDEX, szSizeString);
		}

		free(pDWFolderSizeCompletion);
	}
	break;

	case WM_NDW_RCLICK:
	{
		POINT pt;
		POINTSTOPOINT(pt, MAKEPOINTS(lParam));
		OnDisplayWindowRClick(&pt);
	}
	break;

	case WM_APPCOMMAND:
		OnAppCommand(GET_APPCOMMAND_LPARAM(lParam));
		break;

	case WM_COMMAND:
		return CommandHandler(hwnd, reinterpret_cast<HWND>(lParam), LOWORD(wParam), HIWORD(wParam));

	case WM_NOTIFY:
		return NotifyHandler(hwnd, msg, wParam, lParam);

	case WM_SIZE:
		OnSize(static_cast<UINT>(wParam));
		return 0;

	case WM_DPICHANGED:
		OnDpiChanged(reinterpret_cast<RECT *>(lParam));
		return 0;

	case WM_CTLCOLORSTATIC:
		if (auto res =
				OnCtlColorStatic(reinterpret_cast<HWND>(lParam), reinterpret_cast<HDC>(wParam)))
		{
			return *res;
		}
		break;

	// COM calls (such as IDropTarget::DragEnter) can result in a call being made to PeekMessage().
	// That method will then dispatch non-queued messages, with WM_CLOSE being one such message.
	// That's an issue, as it means if a WM_CLOSE message is in the message queue when a COM method
	// is called, the WM_CLOSE message could be processed, the main window destroyed and
	// Explorerplusplus instance deleted, all within the call to the COM method. Once the COM method
	// returns, the application isn't going to be in a valid state and will crash.
	// PeekMessage() won't, however, dispatch posted (i.e. queued) messages. So the message that's
	// posted here will only be processed in the normal message loop. If a COM modal loop is
	// running, the message won't be processed until that modal loop ends and the normal message
	// loop resumes.
	case WM_CLOSE:
		PostMessage(hwnd, WM_APP_CLOSE, 0, 0);
		return 0;

	case WM_APP_CLOSE:
		TryClose();
		break;

	case WM_ENDSESSION:
		if (wParam)
		{
			m_app->SessionEnding();
		}
		return 0;

	case WM_DESTROY:
		return OnDestroy();

	case WM_NCDESTROY:
		delete this;
		return 0;
	}

	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT Explorerplusplus::CommandHandler(HWND hwnd, HWND control, UINT id, UINT notificationCode)
{
	// Several toolbars will handle their own items.
	if (control
		&& ((m_drivesToolbar && control == m_drivesToolbar->GetView()->GetHWND())
			|| (m_applicationToolbar && control == m_applicationToolbar->GetView()->GetHWND())
			|| (m_bookmarksToolbar && control == m_bookmarksToolbar->GetView()->GetHWND())))
	{
		return 1;
	}

	if (control && notificationCode != 0)
	{
		return HandleControlNotification(hwnd, notificationCode);
	}
	else
	{
		return HandleMenuOrToolbarButtonOrAccelerator(hwnd, id, notificationCode);
	}
}

// It makes sense to handle menu items/toolbar buttons/accelerators together, since an individual
// command might be represented by all three of those.
LRESULT Explorerplusplus::HandleMenuOrToolbarButtonOrAccelerator(HWND hwnd, UINT id,
	UINT notificationCode)
{
	if (notificationCode == 0 && id >= MENU_BOOKMARK_START_ID && id < MENU_BOOKMARK_END_ID)
	{
		m_bookmarksMainMenu->OnMenuItemClicked(id);
		return 0;
	}
	else if (notificationCode == 0 && id >= MENU_PLUGIN_START_ID && id < MENU_PLUGIN_END_ID)
	{
		m_pluginMenuManager.OnMenuItemClicked(id);
		return 0;
	}
	else if (notificationCode == 1 && id >= ACCELERATOR_PLUGIN_START_ID
		&& id < ACCELERATOR_PLUGIN_END_ID)
	{
		m_pluginCommandManager.onAcceleratorPressed(id);
		return 0;
	}
	else if (notificationCode == 0 && MaybeHandleMainMenuItemSelection(id))
	{
		return 0;
	}
	else if (IsSortModeMenuItemId(id))
	{
		m_commandController.ExecuteCommand(id);
		return 0;
	}

	switch (id)
	{
	case MainToolbarButton::NewTab:
	case IDM_FILE_NEWTAB:
		OnNewTab();
		break;

	case MainToolbarButton::CloseTab:
	case IDM_FILE_CLOSETAB:
		m_commandController.ExecuteCommand(IDM_FILE_CLOSETAB);
		break;

	case IDM_FILE_NEW_WINDOW:
		CreateNewWindow();
		break;

	case IDM_FILE_CLONEWINDOW:
		OnCloneWindow();
		break;

	case IDM_FILE_SAVEDIRECTORYLISTING:
		m_commandController.ExecuteCommand(id);
		break;

	case MainToolbarButton::OpenCommandPrompt:
	case IDM_FILE_OPENCOMMANDPROMPT:
		m_commandController.ExecuteCommand(IDM_FILE_OPENCOMMANDPROMPT);
		break;

	case IDM_FILE_OPENCOMMANDPROMPTADMINISTRATOR:
		m_commandController.ExecuteCommand(IDM_FILE_OPENCOMMANDPROMPTADMINISTRATOR);
		break;

	case IDM_FILE_COPYFOLDERPATH:
	case IDM_FILE_COPYITEMPATH:
	case IDM_FILE_COPYUNIVERSALFILEPATHS:
		m_commandController.ExecuteCommand(id);
		break;

	case IDM_FILE_COPYCOLUMNTEXT:
		CopyColumnInfoToClipboard();
		break;

	case IDM_FILE_SETFILEATTRIBUTES:
		m_commandController.ExecuteCommand(id);
		break;

	case MainToolbarButton::Delete:
	case IDM_FILE_DELETE:
		m_commandController.ExecuteCommand(IDM_FILE_DELETE);
		break;

	case MainToolbarButton::DeletePermanently:
	case IDM_FILE_DELETEPERMANENTLY:
		m_commandController.ExecuteCommand(IDM_FILE_DELETEPERMANENTLY);
		break;

	case IDM_FILE_RENAME:
		m_commandController.ExecuteCommand(id);
		break;

	case MainToolbarButton::Properties:
	case IDM_FILE_PROPERTIES:
		m_commandController.ExecuteCommand(IDM_FILE_PROPERTIES);
		break;

	case IDM_FILE_EXIT:
		m_app->TryExit();
		break;

	case IDM_EDIT_UNDO:
		m_fileActionHandler.Undo();
		break;

	case MainToolbarButton::Cut:
	case IDM_EDIT_CUT:
		m_commandController.ExecuteCommand(IDM_EDIT_CUT);
		break;

	case MainToolbarButton::Copy:
	case IDM_EDIT_COPY:
		m_commandController.ExecuteCommand(IDM_EDIT_COPY);
		break;

	case MainToolbarButton::Paste:
	case IDM_EDIT_PASTE:
	case IDM_BACKGROUND_CONTEXT_MENU_PASTE:
		OnPaste();
		break;

	case IDM_EDIT_PASTESHORTCUT:
	case IDM_BACKGROUND_CONTEXT_MENU_PASTE_SHORTCUT:
		OnPasteShortcut();
		break;

	case IDM_EDIT_PASTEHARDLINK:
		GetActiveShellBrowserImpl()->PasteHardLinks();
		break;

	case IDM_EDIT_PASTE_SYMBOLIC_LINK:
		GetActiveShellBrowserImpl()->PasteSymLinks();
		break;

	case MainToolbarButton::MoveTo:
	case IDM_EDIT_MOVETOFOLDER:
		m_commandController.ExecuteCommand(IDM_EDIT_MOVETOFOLDER);
		break;

	case IDM_EDIT_COPYTOFOLDER:
	case MainToolbarButton::CopyTo:
		m_commandController.ExecuteCommand(IDM_EDIT_COPYTOFOLDER);
		break;

	case IDM_EDIT_SELECTALL:
	case IDM_EDIT_INVERTSELECTION:
	case IDM_EDIT_SELECTNONE:
		m_commandController.ExecuteCommand(id);
		break;

	case IDM_EDIT_SELECTALLOFSAMETYPE:
		HighlightSimilarFiles(m_hActiveListView);
		SetFocus(m_hActiveListView);
		break;

	case IDM_EDIT_WILDCARDSELECTION:
	case IDM_EDIT_WILDCARDDESELECT:
		m_commandController.ExecuteCommand(id);
		break;

	case IDM_EDIT_RESOLVELINK:
		OnResolveLink();
		break;

	case IDM_VIEW_DUAL_PANE:
		m_config->dualPane = !m_config->dualPane;
		break;

	case IDM_VIEW_STATUSBAR:
		m_commandController.ExecuteCommand(id);
		break;

	case MainToolbarButton::Folders:
	case IDM_VIEW_FOLDERS:
		m_commandController.ExecuteCommand(IDM_VIEW_FOLDERS);
		break;

	case IDM_VIEW_DISPLAYWINDOW:
		m_commandController.ExecuteCommand(id);
		break;

	case IDM_DISPLAYWINDOW_VERTICAL:
		m_config->displayWindowVertical = !m_config->displayWindowVertical;
		ApplyDisplayWindowPosition();
		UpdateLayout();
		break;

	case IDM_VIEW_TOOLBARS_ADDRESS_BAR:
	case IDM_VIEW_TOOLBARS_MAIN_TOOLBAR:
	case IDM_VIEW_TOOLBARS_BOOKMARKS_TOOLBAR:
	case IDM_VIEW_TOOLBARS_DRIVES_TOOLBAR:
	case IDM_VIEW_TOOLBARS_APPLICATION_TOOLBAR:
	case IDM_VIEW_TOOLBARS_LOCK_TOOLBARS:
	case IDM_VIEW_TOOLBARS_CUSTOMIZE:
	case IDM_VIEW_DECREASE_TEXT_SIZE:
	case IDM_VIEW_INCREASE_TEXT_SIZE:
	case IDA_RESET_TEXT_SIZE:
	case IDM_VIEW_EXTRALARGEICONS:
	case IDM_VIEW_LARGEICONS:
	case IDM_VIEW_ICONS:
	case IDM_VIEW_SMALLICONS:
	case IDM_VIEW_LIST:
	case IDM_VIEW_DETAILS:
	case IDM_VIEW_EXTRALARGETHUMBNAILS:
	case IDM_VIEW_LARGETHUMBNAILS:
	case IDM_VIEW_THUMBNAILS:
	case IDM_VIEW_TILES:
	case IDM_VIEW_CHANGEDISPLAYCOLOURS:
	case IDM_FILTER_FILTERRESULTS:
	case IDM_FILTER_ENABLE_FILTER:
		m_commandController.ExecuteCommand(id);
		break;

	case IDM_GROUPBY_NAME:
		OnGroupBy(SortMode::Name);
		break;

	case IDM_GROUPBY_SIZE:
		OnGroupBy(SortMode::Size);
		break;

	case IDM_GROUPBY_TYPE:
		OnGroupBy(SortMode::Type);
		break;

	case IDM_GROUPBY_DATEMODIFIED:
		OnGroupBy(SortMode::DateModified);
		break;

	case IDM_GROUPBY_TOTALSIZE:
		OnGroupBy(SortMode::TotalSize);
		break;

	case IDM_GROUPBY_FREESPACE:
		OnGroupBy(SortMode::FreeSpace);
		break;

	case IDM_GROUPBY_DATEDELETED:
		OnGroupBy(SortMode::DateDeleted);
		break;

	case IDM_GROUPBY_ORIGINALLOCATION:
		OnGroupBy(SortMode::OriginalLocation);
		break;

	case IDM_GROUPBY_ATTRIBUTES:
		OnGroupBy(SortMode::Attributes);
		break;

	case IDM_GROUPBY_REALSIZE:
		OnGroupBy(SortMode::RealSize);
		break;

	case IDM_GROUPBY_SHORTNAME:
		OnGroupBy(SortMode::ShortName);
		break;

	case IDM_GROUPBY_OWNER:
		OnGroupBy(SortMode::Owner);
		break;

	case IDM_GROUPBY_PRODUCTNAME:
		OnGroupBy(SortMode::ProductName);
		break;

	case IDM_GROUPBY_COMPANY:
		OnGroupBy(SortMode::Company);
		break;

	case IDM_GROUPBY_DESCRIPTION:
		OnGroupBy(SortMode::Description);
		break;

	case IDM_GROUPBY_FILEVERSION:
		OnGroupBy(SortMode::FileVersion);
		break;

	case IDM_GROUPBY_PRODUCTVERSION:
		OnGroupBy(SortMode::ProductVersion);
		break;

	case IDM_GROUPBY_SHORTCUTTO:
		OnGroupBy(SortMode::ShortcutTo);
		break;

	case IDM_GROUPBY_HARDLINKS:
		OnGroupBy(SortMode::HardLinks);
		break;

	case IDM_GROUPBY_EXTENSION:
		OnGroupBy(SortMode::Extension);
		break;

	case IDM_GROUPBY_CREATED:
		OnGroupBy(SortMode::Created);
		break;

	case IDM_GROUPBY_ACCESSED:
		OnGroupBy(SortMode::Accessed);
		break;

	case IDM_GROUPBY_TITLE:
		OnGroupBy(SortMode::Title);
		break;

	case IDM_GROUPBY_SUBJECT:
		OnGroupBy(SortMode::Subject);
		break;

	case IDM_GROUPBY_AUTHOR:
		OnGroupBy(SortMode::Authors);
		break;

	case IDM_GROUPBY_KEYWORDS:
		OnGroupBy(SortMode::Keywords);
		break;

	case IDM_GROUPBY_COMMENTS:
		OnGroupBy(SortMode::Comments);
		break;

	case IDM_GROUPBY_CAMERAMODEL:
		OnGroupBy(SortMode::CameraModel);
		break;

	case IDM_GROUPBY_DATETAKEN:
		OnGroupBy(SortMode::DateTaken);
		break;

	case IDM_GROUPBY_WIDTH:
		OnGroupBy(SortMode::Width);
		break;

	case IDM_GROUPBY_HEIGHT:
		OnGroupBy(SortMode::Height);
		break;

	case IDM_GROUPBY_VIRTUALCOMMENTS:
		OnGroupBy(SortMode::VirtualComments);
		break;

	case IDM_GROUPBY_FILESYSTEM:
		OnGroupBy(SortMode::FileSystem);
		break;

	case IDM_GROUPBY_NUMPRINTERDOCUMENTS:
		OnGroupBy(SortMode::NumPrinterDocuments);
		break;

	case IDM_GROUPBY_PRINTERSTATUS:
		OnGroupBy(SortMode::PrinterStatus);
		break;

	case IDM_GROUPBY_PRINTERCOMMENTS:
		OnGroupBy(SortMode::PrinterComments);
		break;

	case IDM_GROUPBY_PRINTERLOCATION:
		OnGroupBy(SortMode::PrinterLocation);
		break;

	case IDM_GROUPBY_NETWORKADAPTER_STATUS:
		OnGroupBy(SortMode::NetworkAdapterStatus);
		break;

	case IDM_GROUPBY_MEDIA_BITRATE:
		OnGroupBy(SortMode::MediaBitrate);
		break;

	case IDM_GROUPBY_MEDIA_COPYRIGHT:
		OnGroupBy(SortMode::MediaCopyright);
		break;

	case IDM_GROUPBY_MEDIA_DURATION:
		OnGroupBy(SortMode::MediaDuration);
		break;

	case IDM_GROUPBY_MEDIA_PROTECTED:
		OnGroupBy(SortMode::MediaProtected);
		break;

	case IDM_GROUPBY_MEDIA_RATING:
		OnGroupBy(SortMode::MediaRating);
		break;

	case IDM_GROUPBY_MEDIA_ALBUM_ARTIST:
		OnGroupBy(SortMode::MediaAlbumArtist);
		break;

	case IDM_GROUPBY_MEDIA_ALBUM:
		OnGroupBy(SortMode::MediaAlbum);
		break;

	case IDM_GROUPBY_MEDIA_BEATS_PER_MINUTE:
		OnGroupBy(SortMode::MediaBeatsPerMinute);
		break;

	case IDM_GROUPBY_MEDIA_COMPOSER:
		OnGroupBy(SortMode::MediaComposer);
		break;

	case IDM_GROUPBY_MEDIA_CONDUCTOR:
		OnGroupBy(SortMode::MediaConductor);
		break;

	case IDM_GROUPBY_MEDIA_DIRECTOR:
		OnGroupBy(SortMode::MediaDirector);
		break;

	case IDM_GROUPBY_MEDIA_GENRE:
		OnGroupBy(SortMode::MediaGenre);
		break;

	case IDM_GROUPBY_MEDIA_LANGUAGE:
		OnGroupBy(SortMode::MediaLanguage);
		break;

	case IDM_GROUPBY_MEDIA_BROADCAST_DATE:
		OnGroupBy(SortMode::MediaBroadcastDate);
		break;

	case IDM_GROUPBY_MEDIA_CHANNEL:
		OnGroupBy(SortMode::MediaChannel);
		break;

	case IDM_GROUPBY_MEDIA_STATION_NAME:
		OnGroupBy(SortMode::MediaStationName);
		break;

	case IDM_GROUPBY_MEDIA_MOOD:
		OnGroupBy(SortMode::MediaMood);
		break;

	case IDM_GROUPBY_MEDIA_PARENTAL_RATING:
		OnGroupBy(SortMode::MediaParentalRating);
		break;

	case IDM_GROUPBY_MEDIA_PARENTAL_RATING_REASON:
		OnGroupBy(SortMode::MediaParentalRatingReason);
		break;

	case IDM_GROUPBY_MEDIA_PERIOD:
		OnGroupBy(SortMode::MediaPeriod);
		break;

	case IDM_GROUPBY_MEDIA_PRODUCER:
		OnGroupBy(SortMode::MediaProducer);
		break;

	case IDM_GROUPBY_MEDIA_PUBLISHER:
		OnGroupBy(SortMode::MediaPublisher);
		break;

	case IDM_GROUPBY_MEDIA_WRITER:
		OnGroupBy(SortMode::MediaWriter);
		break;

	case IDM_GROUPBY_MEDIA_YEAR:
		OnGroupBy(SortMode::MediaYear);
		break;

	case IDM_GROUP_BY_NONE:
		OnGroupByNone();
		break;

	case IDM_SORT_ASCENDING:
	case IDM_SORT_DESCENDING:
		m_commandController.ExecuteCommand(id);
		break;

	case IDM_GROUP_SORT_ASCENDING:
		OnGroupSortDirectionSelected(SortDirection::Ascending);
		break;

	case IDM_GROUP_SORT_DESCENDING:
		OnGroupSortDirectionSelected(SortDirection::Descending);
		break;

	case IDM_VIEW_AUTOARRANGE:
		m_commandController.ExecuteCommand(id);
		break;

	case IDM_VIEW_SHOWHIDDENFILES:
		OnShowHiddenFiles();
		break;

	case MainToolbarButton::Refresh:
	case IDM_VIEW_REFRESH:
	case IDM_BACKGROUND_CONTEXT_MENU_REFRESH:
		m_commandController.ExecuteCommand(IDM_VIEW_REFRESH);
		break;

	case IDM_SORTBY_MORE:
	case IDM_VIEW_SELECTCOLUMNS:
		OnSelectColumns();
		break;

	case IDM_VIEW_AUTOSIZECOLUMNS:
		m_commandController.ExecuteCommand(id);
		break;

	case IDM_VIEW_SAVECOLUMNLAYOUTASDEFAULT:
	{
		/* Dump the columns from the current tab, and save
		them as the default columns for the appropriate folder
		type.. */
		const auto &currentColumns = m_pActiveShellBrowser->GetCurrentColumnSet();
		auto pidl = m_pActiveShellBrowser->GetDirectoryIdl();

		unique_pidl_absolute pidlDrives;
		SHGetFolderLocation(nullptr, CSIDL_DRIVES, nullptr, 0, wil::out_param(pidlDrives));

		unique_pidl_absolute pidlControls;
		SHGetFolderLocation(nullptr, CSIDL_CONTROLS, nullptr, 0, wil::out_param(pidlControls));

		unique_pidl_absolute pidlBitBucket;
		SHGetFolderLocation(nullptr, CSIDL_BITBUCKET, nullptr, 0, wil::out_param(pidlBitBucket));

		unique_pidl_absolute pidlPrinters;
		SHGetFolderLocation(nullptr, CSIDL_PRINTERS, nullptr, 0, wil::out_param(pidlPrinters));

		unique_pidl_absolute pidlConnections;
		SHGetFolderLocation(nullptr, CSIDL_CONNECTIONS, nullptr, 0,
			wil::out_param(pidlConnections));

		unique_pidl_absolute pidlNetwork;
		SHGetFolderLocation(nullptr, CSIDL_NETWORK, nullptr, 0, wil::out_param(pidlNetwork));

		IShellFolder *pShellFolder;
		SHGetDesktopFolder(&pShellFolder);

		if (pShellFolder->CompareIDs(SHCIDS_CANONICALONLY, pidl.get(), pidlDrives.get()) == 0)
		{
			m_config->globalFolderSettings.folderColumns.myComputerColumns = currentColumns;
		}
		else if (pShellFolder->CompareIDs(SHCIDS_CANONICALONLY, pidl.get(), pidlControls.get())
			== 0)
		{
			m_config->globalFolderSettings.folderColumns.controlPanelColumns = currentColumns;
		}
		else if (pShellFolder->CompareIDs(SHCIDS_CANONICALONLY, pidl.get(), pidlBitBucket.get())
			== 0)
		{
			m_config->globalFolderSettings.folderColumns.recycleBinColumns = currentColumns;
		}
		else if (pShellFolder->CompareIDs(SHCIDS_CANONICALONLY, pidl.get(), pidlPrinters.get())
			== 0)
		{
			m_config->globalFolderSettings.folderColumns.printersColumns = currentColumns;
		}
		else if (pShellFolder->CompareIDs(SHCIDS_CANONICALONLY, pidl.get(), pidlConnections.get())
			== 0)
		{
			m_config->globalFolderSettings.folderColumns.networkConnectionsColumns = currentColumns;
		}
		else if (pShellFolder->CompareIDs(SHCIDS_CANONICALONLY, pidl.get(), pidlNetwork.get()) == 0)
		{
			m_config->globalFolderSettings.folderColumns.myNetworkPlacesColumns = currentColumns;
		}
		else
		{
			m_config->globalFolderSettings.folderColumns.realFolderColumns = currentColumns;
		}

		pShellFolder->Release();
	}
	break;

	case MainToolbarButton::NewFolder:
	case IDM_ACTIONS_NEWFOLDER:
		m_commandController.ExecuteCommand(IDM_ACTIONS_NEWFOLDER);
		break;

	case MainToolbarButton::SplitFile:
	case IDM_ACTIONS_SPLITFILE:
		m_commandController.ExecuteCommand(IDM_ACTIONS_SPLITFILE);
		break;

	case MainToolbarButton::MergeFiles:
	case IDM_ACTIONS_MERGEFILES:
		m_commandController.ExecuteCommand(IDM_ACTIONS_MERGEFILES);
		break;

	case IDM_ACTIONS_DESTROYFILES:
		OnDestroyFiles();
		break;

	case MainToolbarButton::Back:
	case IDM_GO_BACK:
		m_commandController.ExecuteCommand(IDM_GO_BACK, DetermineOpenDisposition(false));
		break;

	case MainToolbarButton::Forward:
	case IDM_GO_FORWARD:
		m_commandController.ExecuteCommand(IDM_GO_FORWARD, DetermineOpenDisposition(false));
		break;

	case MainToolbarButton::Up:
	case IDM_GO_UP:
		m_commandController.ExecuteCommand(IDM_GO_UP, DetermineOpenDisposition(false));
		break;

	case IDM_GO_QUICK_ACCESS:
	case IDM_GO_COMPUTER:
	case IDM_GO_DOCUMENTS:
	case IDM_GO_DOWNLOADS:
	case IDM_GO_MUSIC:
	case IDM_GO_PICTURES:
	case IDM_GO_VIDEOS:
	case IDM_GO_DESKTOP:
	case IDM_GO_RECYCLE_BIN:
	case IDM_GO_CONTROL_PANEL:
	case IDM_GO_PRINTERS:
	case IDM_GO_NETWORK:
	case IDM_GO_WSL_DISTRIBUTIONS:
		m_commandController.ExecuteCommand(id, DetermineOpenDisposition(false));
		break;

	case MainToolbarButton::AddBookmark:
	case IDM_BOOKMARKS_BOOKMARKTHISTAB:
		BookmarkHelper::AddBookmarkItem(m_app->GetBookmarkTree(), BookmarkItem::Type::Bookmark,
			nullptr, std::nullopt, hwnd, this, m_app->GetAcceleratorManager(),
			m_app->GetResourceLoader(), m_app->GetPlatformContext());
		break;

	case IDM_BOOKMARKS_BOOKMARK_ALL_TABS:
		BookmarkHelper::BookmarkAllTabs(m_app->GetBookmarkTree(), m_app->GetResourceLoader(), hwnd,
			this, m_app->GetAcceleratorManager(), m_app->GetPlatformContext());
		break;

	case MainToolbarButton::Bookmarks:
	case IDM_BOOKMARKS_MANAGEBOOKMARKS:
		CreateOrSwitchToModelessDialog(m_app->GetModelessDialogList(), L"ManageBookmarksDialog",
			[this, hwnd]
			{
				return ManageBookmarksDialog::Create(m_app->GetResourceLoader(),
					m_app->GetResourceInstance(), hwnd, this, m_config,
					m_app->GetAcceleratorManager(), &m_iconFetcher, m_app->GetBookmarkTree(),
					m_app->GetPlatformContext());
			});
		break;

	case MainToolbarButton::Search:
	case IDM_TOOLS_SEARCH:
		OnSearch();
		break;

	case IDM_TOOLS_CUSTOMIZECOLORS:
		OnCustomizeColors();
		break;

	case IDM_TOOLS_RUNSCRIPT:
		OnRunScript();
		break;

	case IDM_TOOLS_OPTIONS:
		OnShowOptions();
		break;

	case IDM_WINDOW_SEARCH_TABS:
		OnSearchTabs();
		break;

	case IDM_HELP_ONLINE_DOCUMENTATION:
	case IDM_HELP_CHECKFORUPDATES:
	case IDM_HELP_ABOUT:
	case IDA_SELECT_PREVIOUS_TAB:
	case IDA_SELECT_NEXT_TAB:
		m_commandController.ExecuteCommand(id);
		break;

	case IDA_ADDRESSBAR:
		SetFocus(m_addressBar->GetView()->GetHWND());
		break;

	case IDA_COMBODROPDOWN:
		SetFocus(m_addressBar->GetView()->GetHWND());
		SendMessage(m_addressBar->GetView()->GetHWND(), CB_SHOWDROPDOWN, TRUE, 0);
		break;

	case IDA_PREVIOUSWINDOW:
		OnFocusNextWindow(FocusChangeDirection::Previous);
		break;

	case IDA_NEXTWINDOW:
		OnFocusNextWindow(FocusChangeDirection::Next);
		break;

	case IDA_DUPLICATE_TAB:
	case IDA_HOME:
	case IDA_SELECT_TAB_1:
	case IDA_SELECT_TAB_2:
	case IDA_SELECT_TAB_3:
	case IDA_SELECT_TAB_4:
	case IDA_SELECT_TAB_5:
	case IDA_SELECT_TAB_6:
	case IDA_SELECT_TAB_7:
	case IDA_SELECT_TAB_8:
	case IDA_SELECT_LAST_TAB:
		m_commandController.ExecuteCommand(id);
		break;

	case IDA_RESTORE_LAST_TAB:
		m_app->GetTabRestorer()->RestoreLastTab();
		break;

	case MainToolbarButton::Views:
		OnToolbarViews();
		break;

		/* Display window menus. */
	case IDM_DW_HIDEDISPLAYWINDOW:
		m_config->showDisplayWindow = false;
		break;
	}

	return 1;
}

LRESULT Explorerplusplus::HandleControlNotification(HWND hwnd, UINT notificationCode)
{
	UNREFERENCED_PARAMETER(hwnd);

	switch (notificationCode)
	{
	case CBN_DROPDOWN:
		AddPathsToComboBoxEx(m_addressBar->GetView()->GetHWND(),
			m_pActiveShellBrowser->GetDirectoryPath().c_str());
		break;
	}

	return 1;
}

/*
 * WM_NOTIFY handler for the main window.
 */
LRESULT CALLBACK Explorerplusplus::NotifyHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	auto *nmhdr = reinterpret_cast<NMHDR *>(lParam);

	switch (nmhdr->code)
	{
	case LVN_KEYDOWN:
		return OnListViewKeyDown(lParam);

	case TBN_ENDADJUST:
		if (GetLifecycleState() == LifecycleState::Main)
		{
			OnRebarToolbarSizeUpdated(reinterpret_cast<NMHDR *>(lParam)->hwndFrom);
		}
		break;

	case RBN_HEIGHTCHANGE:
		// This message can be dispatched within the middle of an existing layout operation (if the
		// height of the rebar is updated). To avoid making re-entrant layout calls, the layout
		// update will be scheduled, instead of being immediately invoked.
		ScheduleUpdateLayout(m_weakPtrFactory.GetWeakPtr(), m_app->GetRuntime());
		break;

	case RBN_CHEVRONPUSHED:
	{
		NMREBARCHEVRON *pnmrc = nullptr;
		HWND hToolbar = nullptr;
		HMENU hMenu;
		MENUITEMINFO mii;
		TCHAR szText[512];
		TBBUTTON tbButton;
		RECT rcToolbar;
		RECT rcButton;
		RECT rcIntersect;
		LRESULT lResult;
		BOOL bIntersect;
		int iMenu = 0;
		int nButtons;
		int i = 0;

		pnmrc = (NMREBARCHEVRON *) lParam;

		POINT ptMenu;
		ptMenu.x = pnmrc->rc.left;
		ptMenu.y = pnmrc->rc.bottom;
		ClientToScreen(m_mainRebarView->GetHWND(), &ptMenu);

		if (pnmrc->wID == REBAR_BAND_ID_BOOKMARKS_TOOLBAR)
		{
			m_bookmarksToolbar->ShowOverflowMenu(ptMenu);
			return 0;
		}

		hMenu = CreatePopupMenu();

		switch (pnmrc->wID)
		{
		case REBAR_BAND_ID_MAIN_TOOLBAR:
			hToolbar = m_mainToolbar->GetHWND();
			break;

		case REBAR_BAND_ID_DRIVES_TOOLBAR:
			hToolbar = m_drivesToolbar->GetView()->GetHWND();
			break;

		case REBAR_BAND_ID_APPLICATIONS_TOOLBAR:
			hToolbar = m_applicationToolbar->GetView()->GetHWND();
			break;
		}

		nButtons = (int) SendMessage(hToolbar, TB_BUTTONCOUNT, 0, 0);

		GetClientRect(hToolbar, &rcToolbar);

		for (i = 0; i < nButtons; i++)
		{
			lResult = SendMessage(hToolbar, TB_GETITEMRECT, i, (LPARAM) &rcButton);

			if (lResult)
			{
				bIntersect = IntersectRect(&rcIntersect, &rcToolbar, &rcButton);

				if (!bIntersect || rcButton.right > rcToolbar.right)
				{
					SendMessage(hToolbar, TB_GETBUTTON, i, (LPARAM) &tbButton);

					if (tbButton.fsStyle & BTNS_SEP)
					{
						mii.cbSize = sizeof(mii);
						mii.fMask = MIIM_FTYPE;
						mii.fType = MFT_SEPARATOR;
						InsertMenuItem(hMenu, i, TRUE, &mii);
					}
					else
					{
						if (IS_INTRESOURCE(tbButton.iString))
						{
							SendMessage(hToolbar, TB_GETSTRING,
								MAKEWPARAM(std::size(szText), tbButton.iString), (LPARAM) szText);
						}
						else
						{
							StringCchCopy(szText, std::size(szText), (LPCWSTR) tbButton.iString);
						}

						HMENU hSubMenu = nullptr;
						UINT fMask;

						fMask = MIIM_ID | MIIM_STRING;
						hSubMenu = nullptr;

						switch (pnmrc->wID)
						{
						case REBAR_BAND_ID_MAIN_TOOLBAR:
						{
							switch (tbButton.idCommand)
							{
							case MainToolbarButton::Back:
								hSubMenu = CreateRebarHistoryMenu(TRUE);
								fMask |= MIIM_SUBMENU;
								break;

							case MainToolbarButton::Forward:
								hSubMenu = CreateRebarHistoryMenu(FALSE);
								fMask |= MIIM_SUBMENU;
								break;

							case MainToolbarButton::Views:
							{
								ViewsMenuBuilder viewsMenuBuilder(m_app->GetResourceLoader());
								auto viewsMenu = viewsMenuBuilder.BuildMenu(this);

								// The submenu will be destroyed when the parent menu is
								// destroyed.
								hSubMenu = viewsMenu.release();

								fMask |= MIIM_SUBMENU;
							}
							break;
							}
						}
						break;
						}

						mii.cbSize = sizeof(mii);
						mii.fMask = fMask;
						mii.wID = tbButton.idCommand;
						mii.hSubMenu = hSubMenu;
						mii.dwTypeData = szText;
						InsertMenuItem(hMenu, iMenu, TRUE, &mii);

						/* TODO: Update the image
						for this menu item. */
					}
					iMenu++;
				}
			}
		}

		UINT uFlags = TPM_LEFTALIGN | TPM_RETURNCMD;
		int iCmd;

		iCmd = TrackPopupMenu(hMenu, uFlags, ptMenu.x, ptMenu.y, 0, m_mainRebarView->GetHWND(),
			nullptr);

		if (iCmd != 0)
		{
			/* We'll handle the back and forward buttons
			in place, and send the rest of the messages
			back to the main window. */
			if ((iCmd >= ID_REBAR_MENU_BACK_START && iCmd <= ID_REBAR_MENU_BACK_END)
				|| (iCmd >= ID_REBAR_MENU_FORWARD_START && iCmd <= ID_REBAR_MENU_FORWARD_END))
			{
				if (iCmd >= ID_REBAR_MENU_BACK_START && iCmd <= ID_REBAR_MENU_BACK_END)
				{
					iCmd = -(iCmd - ID_REBAR_MENU_BACK_START);
				}
				else
				{
					iCmd = iCmd - ID_REBAR_MENU_FORWARD_START;
				}

				OnGoToOffset(iCmd);
			}
			else
			{
				SendMessage(m_hContainer, WM_COMMAND, MAKEWPARAM(iCmd, 0), 0);
			}
		}

		DestroyMenu(hMenu);
	}
	break;
	}

	return DefSubclassProc(hwnd, msg, wParam, lParam);
}
