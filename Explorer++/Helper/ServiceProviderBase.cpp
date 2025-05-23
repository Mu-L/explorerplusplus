// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "ServiceProviderBase.h"

void ServiceProviderBase::RegisterService(REFGUID guidService, winrt::com_ptr<IUnknown> service)
{
	m_services[guidService] = service;
}

HRESULT ServiceProviderBase::QueryService(REFGUID guidService, REFIID riid, void **ppv)
{
	auto itr = m_services.find(guidService);

	if (itr == m_services.end())
	{
		return E_NOINTERFACE;
	}

	return itr->second->QueryInterface(riid, ppv);
}
