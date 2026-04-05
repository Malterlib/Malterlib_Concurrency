// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency::NPrivate
{
	template <typename t_CPromise>
	struct TCPromiseReceiveAnyFunctor
	{
		TCPromiseReceiveAnyFunctor(t_CPromise const &_Promise)
			: m_Promise(_Promise)
		{
		}

		TCPromiseReceiveAnyFunctor(t_CPromise &&_Promise)
			: m_Promise(fg_Move(_Promise))
		{
		}

		template <typename ...tfp_CResult>
		void operator () (tfp_CResult && ...p_Result);

		t_CPromise m_Promise;
	};

	struct CPromiseReceiveAnyUnwrapFunctor
	{
		CPromiseReceiveAnyUnwrapFunctor(TCPromise<void> const &_Promise)
			: m_Promise(_Promise)
		{
		}

		CPromiseReceiveAnyUnwrapFunctor(TCPromise<void> &&_Promise)
			: m_Promise(fg_Move(_Promise))
		{
		}

		template <typename ...tfp_CResult>
		void operator () (tfp_CResult && ...p_Result);

		TCPromise<void> m_Promise;
	};

	template <typename t_CPromise>
	template <typename ...tfp_CResult>
	void TCPromiseReceiveAnyFunctor<t_CPromise>::operator () (tfp_CResult && ...p_Result)
	{
		if (m_Promise.f_IsSet())
			return;

		(
			[&]
			{
				if (m_Promise.f_IsSet())
					return;

				if (!p_Result)
					m_Promise.f_SetException(fg_Move(p_Result));
			}
			()
			, ...
		);

		if (m_Promise.f_IsSet())
			return;

		(
			[&]
			{
				if (m_Promise.f_IsSet())
					return;

				if (p_Result)
					m_Promise.f_SetResult(fg_Move(*p_Result));
			}
			()
			, ...
		);
	}

	template <>
	template <typename ...tfp_CResult>
	void TCPromiseReceiveAnyFunctor<TCPromise<void>>::operator () (tfp_CResult && ...p_Result)
	{
		if (m_Promise.f_IsSet())
			return;

		(
			[&]
			{
				if (m_Promise.f_IsSet())
					return;

				if (!p_Result)
					m_Promise.f_SetException(fg_Move(p_Result));
			}
			()
			, ...
		);

		if (m_Promise.f_IsSet())
			return;

		m_Promise.f_SetResult();
	}

	template <typename ...tfp_CResult>
	void CPromiseReceiveAnyUnwrapFunctor::operator () (tfp_CResult && ...p_Result)
	{
		if (m_Promise.f_IsSet())
			return;

		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bSuccess = true;

		(
			[&]
			{
				if (!p_Result)
				{
					ErrorCollector.f_AddError(fg_Move(p_Result).f_GetException());
					bSuccess = false;
				}
				else
				{
					if constexpr (NContainer::TCIsContainer<NTraits::TCRemoveReferenceAndQualifiers<decltype(*p_Result)>>::mc_Value)
					{
						auto Result = fg_Unwrap(fg_Move(*p_Result));
						if (!Result)
						{
							ErrorCollector.f_AddError(fg_Move(Result).f_GetException());
							bSuccess = false;
						}
					}
				}
			}
			()
			, ...
		);

		if (!bSuccess)
		{
			m_Promise.f_SetException(fg_Move(ErrorCollector).f_GetException());
			return;
		}

		m_Promise.f_SetResult();
	}
}
