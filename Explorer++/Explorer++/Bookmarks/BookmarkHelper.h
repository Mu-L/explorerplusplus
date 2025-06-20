// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include "Bookmarks/BookmarkItem.h"
#include "NavigationHelper.h"
#include <optional>

class AcceleratorManager;
class BookmarkTree;
class BrowserWindow;
class ClipboardStore;
class CoreInterface;
class ResourceLoader;
class TabContainer;

using RawBookmarkItems = std::vector<BookmarkItem *>;

namespace BookmarkHelper
{

enum class ColumnType
{
	Default = 0,
	Name = 1,
	Location = 2,
	DateCreated = 3,
	DateModified = 4
};

bool IsFolder(const std::unique_ptr<BookmarkItem> &bookmarkItem);
bool IsBookmark(const std::unique_ptr<BookmarkItem> &bookmarkItem);

int CALLBACK Sort(ColumnType columnType, const BookmarkItem *firstItem,
	const BookmarkItem *secondItem);

void BookmarkAllTabs(BookmarkTree *bookmarkTree, const ResourceLoader *resourceLoader,
	HWND parentWindow, BrowserWindow *browser, CoreInterface *coreInterface,
	const AcceleratorManager *acceleratorManager);
BookmarkItem *AddBookmarkItem(BookmarkTree *bookmarkTree, BookmarkItem::Type type,
	BookmarkItem *defaultParentSelection, std::optional<size_t> suggestedIndex, HWND parentWindow,
	BrowserWindow *browser, const AcceleratorManager *acceleratorManager,
	const ResourceLoader *resourceLoader,
	std::optional<std::wstring> customDialogTitle = std::nullopt);
void EditBookmarkItem(BookmarkItem *bookmarkItem, BookmarkTree *bookmarkTree,
	const AcceleratorManager *acceleratorManager, const ResourceLoader *resourceLoader,
	HWND parentWindow);
void OpenBookmarkItemWithDisposition(const BookmarkItem *bookmarkItem,
	OpenFolderDisposition disposition, BrowserWindow *browser);

bool CopyBookmarkItems(ClipboardStore *clipboardStore, BookmarkTree *bookmarkTree,
	const RawBookmarkItems &bookmarkItems, bool cut);
void PasteBookmarkItems(ClipboardStore *clipboardStore, BookmarkTree *bookmarkTree,
	BookmarkItem *parentFolder, size_t index);

BookmarkItem *GetBookmarkItemById(BookmarkTree *bookmarkTree, std::wstring_view guid);

bool IsAncestor(const BookmarkItem *bookmarkItem, const BookmarkItem *possibleAncestor);

}
