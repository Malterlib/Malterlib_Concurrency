// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

namespace NMib::NConcurrency::NLogStore
{
	template <typename tf_CKey>
	bool fg_FilterLogKey
		(
			tf_CKey const &_Key
			, CDistributedAppLogReader_LogFilter const &_Filter
			, CFilterLogKeyContext &_Context
			, CDistributedAppLogReporter::CLogInfo const *_pLogInfo
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
					if (_Key.m_ApplicationName != Application.m_ApplicationName)
						return false;
				}
				break;
			}
		}

		if (_Filter.m_Identifier && _Key.m_Identifier != *_Filter.m_Identifier)
			return false;

		if (_Filter.m_IdentifierScope && _Key.m_IdentifierScope != *_Filter.m_IdentifierScope)
			return false;

		if (_Filter.m_Flags & CDistributedAppLogReader_LogFilter::ELogFlag_IgnoreRemoved)
		{
			if (!_Key.m_HostID.f_IsEmpty() && _Key.m_HostID != _Context.m_ThisHostID)
			{
				NLogStoreLocalDatabase::CKnownHostKey Key{.m_DbPrefix = _Context.m_Prefix, .m_HostID = _Key.m_HostID};
				NLogStoreLocalDatabase::CKnownHostValue Value;
				if (_Context.m_Transaction.f_Get(Key, Value))
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
		)
	{
		bool bPassFilter = _Filters.f_IsEmpty();

		for (auto &Filter : _Filters)
		{
			if (!fg_FilterLogKey(_Key, Filter, _Context, _pLogInfo))
				continue;

			bPassFilter = true;
			break;
		}

		return bPassFilter;
	}
}
