// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include "../Helper/ClipboardHelper.h"
#include "../Helper/SignalHelper.h"
#include <boost/signals2.hpp>

using MainMenuPreShowSignal = boost::signals2::signal<void(HMENU mainMenu)>;
using MainMenuItemRightClickedSignal =
	boost::signals2::signal<bool(HMENU menu, int index, const POINT &pt),
		FirstSuccessfulRequestCombiner<bool>>;
using MainMenuItemMiddleClickedSignal =
	boost::signals2::signal<bool(const POINT &pt, bool isCtrlKeyDown, bool isShiftKeyDown),
		FirstSuccessfulRequestCombiner<bool>>;

class ShellBrowserImpl;
class TabContainer;

// Deprecated interface between Explorerplusplus and some of the other components (such as the
// dialogs and toolbars).
class CoreInterface
{
public:
	virtual ~CoreInterface() = default;

	virtual HWND GetMainWindow() const = 0;

	virtual ShellBrowserImpl *GetActiveShellBrowserImpl() const = 0;

	virtual TabContainer *GetTabContainer() const = 0;

	virtual BOOL CanPaste(PasteType pasteType) const = 0;

	virtual boost::signals2::connection AddMainMenuPreShowObserver(
		const MainMenuPreShowSignal::slot_type &observer) = 0;
	virtual boost::signals2::connection AddMainMenuItemMiddleClickedObserver(
		const MainMenuItemMiddleClickedSignal::slot_type &observer) = 0;
	virtual boost::signals2::connection AddMainMenuItemRightClickedObserver(
		const MainMenuItemRightClickedSignal::slot_type &observer) = 0;
};
