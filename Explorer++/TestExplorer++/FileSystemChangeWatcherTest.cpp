// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "FileSystemChangeWatcher.h"
#include "ExecutorWrapper.h"
#include "MessageLoop.h"
#include "ScopedTestDir.h"
#include "ShellTestHelper.h"
#include "UIThreadExecutor.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <fstream>

using namespace std::chrono_literals;
using namespace testing;

class FileSystemChangeWatcherTest : public Test
{
protected:
	FileSystemChangeWatcherTest() : m_uiThreadExecutor(std::make_shared<UIThreadExecutor>())
	{
	}

	void SetUp() override
	{
		HRESULT hr = SHParseDisplayName(m_scopedTestDir.GetPath().c_str(), nullptr,
			PidlOutParam(m_scopedTestDirPidl), 0, nullptr);
		ASSERT_HRESULT_SUCCEEDED(hr);
	}

	std::unique_ptr<FileSystemChangeWatcher> CreateWatcher(
		FileSystemChangeWatcher::Event eventToObserve)
	{
		std::unique_ptr<FileSystemChangeWatcher> watcher;
		CreateWatcher(watcher, eventToObserve);
		return watcher;
	}

	void WaitForNotifications()
	{
		m_messageLoop.RunWithTimeout(TIMEOUT_DURATION, m_uiThreadExecutor.Get());
	}

	static constexpr std::chrono::duration TIMEOUT_DURATION = 5s;

	ScopedTestDir m_scopedTestDir;
	PidlAbsolute m_scopedTestDirPidl;
	ExecutorWrapper<UIThreadExecutor> m_uiThreadExecutor;
	MockFunction<void(FileSystemChangeWatcher::Event event, const PidlAbsolute &simplePidl1,
		const PidlAbsolute &simplePidl2)>
		m_callback;
	MessageLoop m_messageLoop;

private:
	void CreateWatcher(std::unique_ptr<FileSystemChangeWatcher> &watcher,
		FileSystemChangeWatcher::Event eventToObserve)
	{
		watcher = FileSystemChangeWatcher::MaybeCreate(m_scopedTestDirPidl,
			wil::FolderChangeEvents::All, m_uiThreadExecutor.Get(),
			[this, eventToObserve](FileSystemChangeWatcher::Event event,
				const PidlAbsolute &simplePidl1, const PidlAbsolute &simplePidl2)
			{
				if (event == eventToObserve)
				{
					m_callback.Call(event, simplePidl1, simplePidl2);
					m_messageLoop.Stop();
				}
			});
		ASSERT_NE(watcher, nullptr);
	}
};

TEST_F(FileSystemChangeWatcherTest, AddItem)
{
	auto watcher = CreateWatcher(FileSystemChangeWatcher::Event::Added);

	auto newItemPath = m_scopedTestDir.GetPath() / L"new-item";
	auto newItemPidl = CreateSimplePidlForTest(newItemPath, nullptr, ShellItemType::Folder);

	EXPECT_CALL(m_callback,
		Call(FileSystemChangeWatcher::Event::Added, newItemPidl, PidlAbsolute{}));
	std::filesystem::create_directory(newItemPath);

	WaitForNotifications();
}

TEST_F(FileSystemChangeWatcherTest, ModifyItem)
{
	auto watcher = CreateWatcher(FileSystemChangeWatcher::Event::Modified);

	auto itemPath = m_scopedTestDir.GetPath() / L"item";
	auto itemPidl = CreateSimplePidlForTest(itemPath, nullptr, ShellItemType::File);

	EXPECT_CALL(m_callback,
		Call(FileSystemChangeWatcher::Event::Modified, itemPidl, PidlAbsolute{}));

	{
		std::ofstream stream(itemPath);
		stream << "data";
	}

	WaitForNotifications();
}

TEST_F(FileSystemChangeWatcherTest, RenameItem)
{
	auto watcher = CreateWatcher(FileSystemChangeWatcher::Event::Renamed);

	auto originalItemPath = m_scopedTestDir.GetPath() / L"original-item";
	auto originalItemPidl =
		CreateSimplePidlForTest(originalItemPath, nullptr, ShellItemType::Folder);

	auto updatedItemPath = m_scopedTestDir.GetPath() / L"updated-item";
	auto updatedItemPidl = CreateSimplePidlForTest(updatedItemPath, nullptr, ShellItemType::Folder);

	EXPECT_CALL(m_callback,
		Call(FileSystemChangeWatcher::Event::Renamed, originalItemPidl, updatedItemPidl));

	std::filesystem::create_directory(originalItemPath);
	std::filesystem::rename(originalItemPath, updatedItemPath);

	WaitForNotifications();
}

TEST_F(FileSystemChangeWatcherTest, RemoveItem)
{
	auto watcher = CreateWatcher(FileSystemChangeWatcher::Event::Removed);

	auto itemPath = m_scopedTestDir.GetPath() / L"item";
	auto itemPidl = CreateSimplePidlForTest(itemPath, nullptr, ShellItemType::Folder);

	EXPECT_CALL(m_callback,
		Call(FileSystemChangeWatcher::Event::Removed, itemPidl, PidlAbsolute{}));

	std::filesystem::create_directory(itemPath);
	std::filesystem::remove(itemPath);

	WaitForNotifications();
}

TEST_F(FileSystemChangeWatcherTest, ResumeAfterDestruction)
{
	auto watcher = CreateWatcher(FileSystemChangeWatcher::Event::Added);

	auto itemPath = m_scopedTestDir.GetPath() / L"item";
	std::filesystem::create_directory(itemPath);

	// To transfer back to the UI thread, the FileSystemChangeWatcher instance will queue a task to
	// UIThreadExecutor. That ultimately involves posting a message. This will wait (with a timeout)
	// until that message has been posted. It's important to do this before the
	// FileSystemChangeWatcher instance is destroyed, otherwise the instance could be destroyed too
	// early (before any change notifications have been processed).
	std::chrono::milliseconds timeoutInMs = TIMEOUT_DURATION;
	auto res = MsgWaitForMultipleObjectsEx(0, nullptr, static_cast<DWORD>(timeoutInMs.count()),
		QS_ALLINPUT, 0);
	ASSERT_NE(res, static_cast<DWORD>(WAIT_TIMEOUT));

	watcher.reset();

	// This should result in the queued task being processed. This should be a safe operation, even
	// though the FileSystemChangeWatcher instance has been destroyed.
	m_messageLoop.RunUntilIdle();
}
