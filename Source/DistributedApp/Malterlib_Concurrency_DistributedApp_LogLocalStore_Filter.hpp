// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

namespace NMib::NConcurrency::NLogStore
{
	template <typename tf_CKey>
	bool fg_FilterLogKey
		(
			tf_CKey const &_Key
			, CDistributedAppLogReader_LogFilter const &_Filter
			, CFilterLogKeyContext &_Context
			, CDistributedAppLogReporter::CLogInfo const *_pLogInfo
			, NLogStoreLocalDatabase::CKnownHostValue const *_pKnownHostValue
		)
	{
		if (_Filter.m_HostID && _Key.m_HostID != *_Filter.m_HostID)
			return false;

		if (_Filter.m_Scope)
		{
			switch (_Filter.m_Scope->f_GetTypeID())
			{
			case CDistributedAppLogReporter::ELogScope_Unsupported:
				{
					return false;
				}
				break;
			case CDistributedAppLogReporter::ELogScope_Host:
				{
					if (!_Key.m_ApplicationName.f_IsEmpty())
						return false;
				}
				break;
			case CDistributedAppLogReporter::ELogScope_Application:
				{
					auto &Application = _Filter.m_Scope->f_Get<CDistributedAppLogReporter::ELogScope_Application>();
					if (NStr::fg_StrMatchWildcard(_Key.m_ApplicationName.f_GetStr(), Application.m_ApplicationName.f_GetStr()) != EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
						return false;
				}
				break;
			}
		}

		if (_Filter.m_Identifier && NStr::fg_StrMatchWildcard(_Key.m_Identifier.f_GetStr(), _Filter.m_Identifier->f_GetStr()) != EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
			return false;

		if
			(
				_Filter.m_IdentifierScope
				&& NStr::fg_StrMatchWildcard(_Key.m_IdentifierScope.f_GetStr(), _Filter.m_IdentifierScope->f_GetStr()) != EMatchWildcardResult_WholeStringMatchedAndPatternExhausted
			)
		{
			return false;
		}

		if (_Filter.m_Flags & CDistributedAppLogReader_LogFilter::ELogFlag_IgnoreRemoved)
		{
			if (_pKnownHostValue)
			{
				if (_pKnownHostValue->m_bRemoved)
					return false;
			}
			else if (!_Key.m_HostID.f_IsEmpty() && _Key.m_HostID != _Context.m_ThisHostID && _Context.m_pTransaction)
			{
				NLogStoreLocalDatabase::CKnownHostKey Key{.m_DbPrefix = _Context.m_Prefix, .m_HostID = _Key.m_HostID};
				NLogStoreLocalDatabase::CKnownHostValue Value;
				if (_Context.m_pTransaction->f_Get(Key, Value))
				{
					if (Value.m_bRemoved)
						return false;
				}
			}

			if (!_pLogInfo)
				_pLogInfo = _Context.f_GetLog(_Key.f_LogKey());

			if (_pLogInfo && _pLogInfo->m_bRemoved)
				return false;
		}

		return true;
	}

	template <typename tf_CKey>
	bool fg_FilterLogKey
		(
			tf_CKey const &_Key
			, TCVector<CDistributedAppLogReader_LogFilter> const &_Filters
			, CFilterLogKeyContext &_Context
			, CDistributedAppLogReporter::CLogInfo const *_pLogInfo
			, NLogStoreLocalDatabase::CKnownHostValue const *_pKnownHostValue
		)
	{
		bool bPassFilter = _Filters.f_IsEmpty();

		for (auto &Filter : _Filters)
		{
			if (!fg_FilterLogKey(_Key, Filter, _Context, _pLogInfo, _pKnownHostValue))
				continue;

			bPassFilter = true;
			break;
		}

		return bPassFilter;
	}
}
