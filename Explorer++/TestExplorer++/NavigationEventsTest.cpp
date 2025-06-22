// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "pch.h"
#include "BrowserTestBase.h"
#include "BrowserWindowFake.h"
#include "NavigationRequestDelegateMock.h"
#include "ShellEnumeratorFake.h"
#include "ShellTestHelper.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace testing;

class NavigationEventsTest : public BrowserTestBase
{
protected:
	using NavigationCallBackMock = StrictMock<MockFunction<void(const NavigationRequest *request)>>;

	NavigationEventsTest() :
		m_browser1(AddBrowser()),
		m_tab1(m_browser1->AddTab(L"c:\\")),
		m_tab2(m_browser1->AddTab(L"c:\\")),
		m_browser2(AddBrowser()),
		m_tab3(m_browser2->AddTab(L"c:\\")),
		m_shellEnumerator(std::make_shared<ShellEnumeratorFake>()),
		m_manualExecutorBackground(std::make_shared<concurrencpp::manual_executor>()),
		m_manualExecutorCurrent(std::make_shared<concurrencpp::manual_executor>()),
		m_request1(MakeNavigationRequest(m_tab1, L"d:\\")),
		m_request2(MakeNavigationRequest(m_tab2, L"e:\\")),
		m_request3(MakeNavigationRequest(m_tab3, L"f:\\"))
	{
	}

	NavigationRequest MakeNavigationRequest(const Tab *tab, const std::wstring &path)
	{
		auto pidl = CreateSimplePidlForTest(path);
		auto navigateParams = NavigateParams::Normal(pidl.Raw());
		return NavigationRequest{ tab->GetShellBrowser(), &m_navigationEvents, &m_requestDelegate,
			m_shellEnumerator, m_manualExecutorBackground, m_manualExecutorCurrent, navigateParams,
			m_stopSource.get_token() };
	}

	BrowserWindowFake *const m_browser1;
	Tab *const m_tab1;
	Tab *const m_tab2;

	BrowserWindowFake *const m_browser2;
	Tab *const m_tab3;

	NiceMock<NavigationRequestDelegateMock> m_requestDelegate;
	const std::shared_ptr<ShellEnumeratorFake> m_shellEnumerator;
	const std::shared_ptr<concurrencpp::manual_executor> m_manualExecutorBackground;
	const std::shared_ptr<concurrencpp::manual_executor> m_manualExecutorCurrent;
	std::stop_source m_stopSource;

	NavigationRequest m_request1;
	NavigationRequest m_request2;
	NavigationRequest m_request3;

	NavigationCallBackMock m_startedCallback;
	NavigationCallBackMock m_willCommitCallback;
	NavigationCallBackMock m_committedCallback;
	NavigationCallBackMock m_failedCallback;
	NavigationCallBackMock m_cancelledCallback;

	StrictMock<MockFunction<void(const ShellBrowser *shellBrowser)>> m_stoppedCallback;
};

TEST_F(NavigationEventsTest, Signals)
{
	InSequence seq;

	m_navigationEvents.AddStartedObserver(m_startedCallback.AsStdFunction(),
		NavigationEventScope::Global());
	EXPECT_CALL(m_startedCallback, Call(&m_request1));
	EXPECT_CALL(m_startedCallback, Call(&m_request2));
	EXPECT_CALL(m_startedCallback, Call(&m_request3));

	m_navigationEvents.AddWillCommitObserver(m_willCommitCallback.AsStdFunction(),
		NavigationEventScope::Global());
	EXPECT_CALL(m_willCommitCallback, Call(&m_request1));
	EXPECT_CALL(m_willCommitCallback, Call(&m_request2));
	EXPECT_CALL(m_willCommitCallback, Call(&m_request3));

	m_navigationEvents.AddCommittedObserver(m_committedCallback.AsStdFunction(),
		NavigationEventScope::Global());
	EXPECT_CALL(m_committedCallback, Call(&m_request1));
	EXPECT_CALL(m_committedCallback, Call(&m_request2));
	EXPECT_CALL(m_committedCallback, Call(&m_request3));

	m_navigationEvents.AddFailedObserver(m_failedCallback.AsStdFunction(),
		NavigationEventScope::Global());
	EXPECT_CALL(m_failedCallback, Call(&m_request1));
	EXPECT_CALL(m_failedCallback, Call(&m_request2));
	EXPECT_CALL(m_failedCallback, Call(&m_request3));

	m_navigationEvents.AddCancelledObserver(m_cancelledCallback.AsStdFunction(),
		NavigationEventScope::Global());
	EXPECT_CALL(m_cancelledCallback, Call(&m_request1));
	EXPECT_CALL(m_cancelledCallback, Call(&m_request2));
	EXPECT_CALL(m_cancelledCallback, Call(&m_request3));

	m_navigationEvents.AddStoppedObserver(m_stoppedCallback.AsStdFunction(),
		NavigationEventScope::Global());
	EXPECT_CALL(m_stoppedCallback, Call(m_tab1->GetShellBrowser()));
	EXPECT_CALL(m_stoppedCallback, Call(m_tab2->GetShellBrowser()));
	EXPECT_CALL(m_stoppedCallback, Call(m_tab3->GetShellBrowser()));

	// The NavigationEvents class simply broadcasts events, it doesn't rely on the NavigationRequest
	// instance being in any particular state. So, it's fine to broadcast these events, without ever
	// actually starting any of the navigations.
	m_navigationEvents.NotifyStarted(&m_request1);
	m_navigationEvents.NotifyStarted(&m_request2);
	m_navigationEvents.NotifyStarted(&m_request3);

	m_navigationEvents.NotifyWillCommit(&m_request1);
	m_navigationEvents.NotifyWillCommit(&m_request2);
	m_navigationEvents.NotifyWillCommit(&m_request3);

	m_navigationEvents.NotifyCommitted(&m_request1);
	m_navigationEvents.NotifyCommitted(&m_request2);
	m_navigationEvents.NotifyCommitted(&m_request3);

	m_navigationEvents.NotifyFailed(&m_request1);
	m_navigationEvents.NotifyFailed(&m_request2);
	m_navigationEvents.NotifyFailed(&m_request3);

	m_navigationEvents.NotifyCancelled(&m_request1);
	m_navigationEvents.NotifyCancelled(&m_request2);
	m_navigationEvents.NotifyCancelled(&m_request3);

	m_navigationEvents.NotifyStopped(m_tab1->GetShellBrowser());
	m_navigationEvents.NotifyStopped(m_tab2->GetShellBrowser());
	m_navigationEvents.NotifyStopped(m_tab3->GetShellBrowser());
}

TEST_F(NavigationEventsTest, SignalsFilteredByBrowser)
{
	InSequence seq;

	m_navigationEvents.AddStartedObserver(m_startedCallback.AsStdFunction(),
		NavigationEventScope::ForBrowser(*m_browser1));
	EXPECT_CALL(m_startedCallback, Call(&m_request1));
	EXPECT_CALL(m_startedCallback, Call(&m_request2));

	m_navigationEvents.AddStoppedObserver(m_stoppedCallback.AsStdFunction(),
		NavigationEventScope::ForBrowser(*m_browser2));
	EXPECT_CALL(m_stoppedCallback, Call(m_tab3->GetShellBrowser()));

	m_navigationEvents.NotifyStarted(&m_request1);
	m_navigationEvents.NotifyStarted(&m_request2);
	m_navigationEvents.NotifyStarted(&m_request3);

	m_navigationEvents.NotifyStopped(m_tab1->GetShellBrowser());
	m_navigationEvents.NotifyStopped(m_tab2->GetShellBrowser());
	m_navigationEvents.NotifyStopped(m_tab3->GetShellBrowser());
}

TEST_F(NavigationEventsTest, SignalsFilteredByShellBrowser)
{
	InSequence seq;

	m_navigationEvents.AddStartedObserver(m_startedCallback.AsStdFunction(),
		NavigationEventScope::ForShellBrowser(*m_tab1->GetShellBrowser()));
	EXPECT_CALL(m_startedCallback, Call(&m_request1));

	m_navigationEvents.AddStoppedObserver(m_stoppedCallback.AsStdFunction(),
		NavigationEventScope::ForShellBrowser(*m_tab2->GetShellBrowser()));
	EXPECT_CALL(m_stoppedCallback, Call(m_tab2->GetShellBrowser()));

	m_navigationEvents.NotifyStarted(&m_request1);
	m_navigationEvents.NotifyStarted(&m_request2);
	m_navigationEvents.NotifyStarted(&m_request3);

	m_navigationEvents.NotifyStopped(m_tab1->GetShellBrowser());
	m_navigationEvents.NotifyStopped(m_tab2->GetShellBrowser());
	m_navigationEvents.NotifyStopped(m_tab3->GetShellBrowser());
}

TEST_F(NavigationEventsTest, SignalsFilteredByActiveShellBrowser)
{
	InSequence seq;

	m_browser1->GetActiveTabContainer()->SelectTabAtIndex(1);
	m_browser2->GetActiveTabContainer()->SelectTabAtIndex(0);

	m_navigationEvents.AddStartedObserver(m_startedCallback.AsStdFunction(),
		NavigationEventScope::ForActiveShellBrowser(*m_browser1));
	EXPECT_CALL(m_startedCallback, Call(&m_request2));

	m_navigationEvents.AddStoppedObserver(m_stoppedCallback.AsStdFunction(),
		NavigationEventScope::ForActiveShellBrowser(*m_browser2));
	EXPECT_CALL(m_stoppedCallback, Call(m_tab3->GetShellBrowser()));

	m_navigationEvents.NotifyStarted(&m_request1);
	m_navigationEvents.NotifyStarted(&m_request2);
	m_navigationEvents.NotifyStarted(&m_request3);

	m_navigationEvents.NotifyStopped(m_tab1->GetShellBrowser());
	m_navigationEvents.NotifyStopped(m_tab2->GetShellBrowser());
	m_navigationEvents.NotifyStopped(m_tab3->GetShellBrowser());
}
