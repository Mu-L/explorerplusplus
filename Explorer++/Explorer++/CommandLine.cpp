// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "CommandLine.h"
#include "Explorer++_internal.h"
#include "MainResource.h"
#include "ResourceHelper.h"
#include "../Helper/Macros.h"
#include "../Helper/ProcessHelper.h"
#include "../Helper/SetDefaultFileManager.h"
#include "../Helper/ShellHelper.h"
#include "../Helper/StringHelper.h"
#include "../ThirdParty/CLI11/CLI11.hpp"
#include <boost/filesystem.hpp>
#include <boost/log/core.hpp>

extern std::vector<std::wstring> g_commandLineDirectories;

struct CommandLineSettings
{
	bool clearRegistrySettings;
	bool enableLogging;
	bool enablePlugins;
	bool removeAsDefault;
	bool setAsDefault;
	std::string language;
	bool jumplistNewTab;
	std::vector<std::string> directories;
};

std::optional<CommandLine::ExitInfo> ProcessCommandLineSettings(const CommandLineSettings& commandLineSettings);
void OnClearRegistrySettings();
void OnRemoveAsDefault();
void OnSetAsDefault();
void OnJumplistNewTab();

std::optional<CommandLine::ExitInfo> CommandLine::ProcessCommandLine()
{
	CLI::App app("Explorer++");

	CommandLineSettings commandLineSettings;

	commandLineSettings.clearRegistrySettings = false;
	app.add_flag(
		"--clear-registry-settings",
		commandLineSettings.clearRegistrySettings,
		"Clear existing registry settings"
	);

	commandLineSettings.enableLogging = false;
	app.add_flag(
		"--enable-logging",
		commandLineSettings.enableLogging,
		"Enable logging"
	);

	commandLineSettings.enablePlugins = false;
	app.add_flag(
		"--enable-plugins",
		commandLineSettings.enablePlugins,
		"Enable the Lua plugin system"
	);

	commandLineSettings.removeAsDefault = false;
	auto removeAsDefaultOption = app.add_flag(
		"--remove-as-default",
		commandLineSettings.removeAsDefault,
		"Remove Explorer++ as the default file manager (requires administrator privileges)"
	);

	commandLineSettings.setAsDefault = false;
	auto setAsDefaultOption = app.add_flag(
		"--set-as-default",
		commandLineSettings.setAsDefault,
		"Set Explorer++ as the default file manager (requires administrator privileges)"
	);

	removeAsDefaultOption->excludes(setAsDefaultOption);
	setAsDefaultOption->excludes(removeAsDefaultOption);

	app.add_option(
		"--language",
		commandLineSettings.language,
		"Allows you to select your desired language. Should be a two-letter language code (e.g. FR, RU, etc)."
	);

	app.add_option(
		"directories",
		commandLineSettings.directories,
		"Directories to open"
	);

	// The options in this group are only used internally by the
	// application. They're not directly exposed to users.
	CLI::App *privateCommands = app.add_subcommand("private");
	privateCommands->group("");

	privateCommands->add_flag(
		wstrToStr(NExplorerplusplus::JUMPLIST_TASK_NEWTAB_ARGUMENT),
		commandLineSettings.jumplistNewTab
	);

	try
	{
		app.parse(__argc, __argv);
	}
	catch (const CLI::ParseError & e)
	{
		return ExitInfo{ app.exit(e) };
	}

	return ProcessCommandLineSettings(commandLineSettings);
}

std::optional<CommandLine::ExitInfo> ProcessCommandLineSettings(const CommandLineSettings& commandLineSettings)
{
	if (commandLineSettings.jumplistNewTab)
	{
		OnJumplistNewTab();
		return CommandLine::ExitInfo{ EXIT_SUCCESS };
	}

	if (commandLineSettings.clearRegistrySettings)
	{
		OnClearRegistrySettings();
	}

	if (commandLineSettings.enableLogging)
	{
		boost::log::core::get()->set_logging_enabled(true);
	}

	if (commandLineSettings.enablePlugins)
	{
		g_enablePlugins = true;
	}

	if (commandLineSettings.removeAsDefault)
	{
		OnRemoveAsDefault();
	}
	else if (commandLineSettings.setAsDefault)
	{
		OnSetAsDefault();
	}

	if (!commandLineSettings.language.empty())
	{
		g_bForceLanguageLoad = TRUE;

		StringCchCopy(g_szLang, SIZEOF_ARRAY(g_szLang), strToWstr(commandLineSettings.language).c_str());
	}

	TCHAR processImageName[MAX_PATH];
	GetProcessImageName(GetCurrentProcessId(), processImageName, SIZEOF_ARRAY(processImageName));

	boost::filesystem::path processDirectoryPath(processImageName);
	processDirectoryPath.remove_filename();

	for (const std::string& directory : commandLineSettings.directories)
	{
		TCHAR szParsingPath[MAX_PATH];
		DecodePath(strToWstr(directory).c_str(), processDirectoryPath.wstring().c_str(), szParsingPath, SIZEOF_ARRAY(szParsingPath));

		g_commandLineDirectories.emplace_back(szParsingPath);
	}

	return std::nullopt;
}

void OnClearRegistrySettings()
{
	LSTATUS lStatus;

	lStatus = SHDeleteKey(HKEY_CURRENT_USER, NExplorerplusplus::REG_MAIN_KEY);

	if (lStatus == ERROR_SUCCESS)
		MessageBox(nullptr, _T("Settings cleared successfully."), NExplorerplusplus::APP_NAME, MB_OK);
	else
		MessageBox(nullptr, _T("Settings could not be cleared."), NExplorerplusplus::APP_NAME, MB_ICONWARNING);
}

void OnRemoveAsDefault()
{
	LSTATUS res = NDefaultFileManager::RemoveAsDefaultFileManagerFileSystem(SHELL_DEFAULT_INTERNAL_COMMAND_NAME);

	/* Language hasn't been fully specified at this point, so
	can't load success/error message from language dll. Simply show
	a hardcoded success/error message. */
	if (res == ERROR_SUCCESS)
	{
		MessageBox(nullptr, _T("Explorer++ successfully removed as default file manager."),
			NExplorerplusplus::APP_NAME, MB_OK);
	}
	else
	{
		MessageBox(nullptr, _T("Could not remove Explorer++ as default file manager. Please \
ensure you have administrator privileges."), NExplorerplusplus::APP_NAME, MB_ICONWARNING | MB_OK);
	}
}

void OnSetAsDefault()
{
	std::wstring menuText = ResourceHelper::LoadString(GetModuleHandle(nullptr), IDS_OPEN_IN_EXPLORERPLUSPLUS);

	LSTATUS res = NDefaultFileManager::SetAsDefaultFileManagerFileSystem(
		SHELL_DEFAULT_INTERNAL_COMMAND_NAME, menuText.c_str());

	if (res == ERROR_SUCCESS)
	{
		MessageBox(nullptr, _T("Explorer++ successfully set as default file manager."),
			NExplorerplusplus::APP_NAME, MB_OK);
	}
	else
	{
		MessageBox(nullptr, _T("Could not set Explorer++ as default file manager. Please \
ensure you have administrator privileges."), NExplorerplusplus::APP_NAME, MB_ICONWARNING | MB_OK);
	}
}

void OnJumplistNewTab()
{
	/* This will be called when the user clicks the
	'New Tab' item on the tasks menu in Windows 7 and above.
	Find the already opened version of Explorer++,
	and tell it to open a new tab. */
	HANDLE hMutex;

	hMutex = CreateMutex(nullptr, TRUE, _T("Explorer++"));

	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		HWND hPrev;

		hPrev = FindWindow(NExplorerplusplus::CLASS_NAME, nullptr);

		if (hPrev != nullptr)
		{
			COPYDATASTRUCT cds;

			cds.cbData = 0;
			cds.lpData = nullptr;
			SendMessage(hPrev, WM_COPYDATA, NULL, (LPARAM)& cds);

			SetForegroundWindow(hPrev);
			ShowWindow(hPrev, SW_SHOW);
		}
	}

	if (hMutex != nullptr)
	{
		CloseHandle(hMutex);
	}
}