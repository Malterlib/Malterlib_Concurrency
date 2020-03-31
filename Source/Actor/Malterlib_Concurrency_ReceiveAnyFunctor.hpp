// Copyright © 2020 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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

		template <typename ...tfp_CResult>
		void operator () (tfp_CResult && ...p_Result);

		t_CPromise m_Promise;
	};

	template <typename t_CPromise>
	template <typename ...tfp_CResult>
	void TCPromiseReceiveAnyFunctor<t_CPromise>::operator () (tfp_CResult && ...p_Result)
	{
		if (m_Promise.f_IsSet())
			return;

		TCInitializerList<bool> Dummy =
			{
				[&]
				{
					if (m_Promise.f_IsSet())
						return true;
					if (!p_Result)
						m_Promise.f_SetException(fg_Move(p_Result));
					return true;
				}
				()...
			}
		;
		(void)Dummy;
		if (m_Promise.f_IsSet())
			return;

		TCInitializerList<bool> Dummy2 =
			{
				[&]
				{
					if (m_Promise.f_IsSet())
						return true;
					if (p_Result)
						m_Promise.f_SetResult(fg_Move(*p_Result));
					return true;
				}
				()...
			}
		;
		(void)Dummy2;
	}

	template <>
	template <typename ...tfp_CResult>
	void TCPromiseReceiveAnyFunctor<TCPromise<void>>::operator () (tfp_CResult && ...p_Result)
	{
		if (m_Promise.f_IsSet())
			return;
		TCInitializerList<bool> Dummy =
			{
				[&]
				{
					if (m_Promise.f_IsSet())
						return true;
					if (!p_Result)
						m_Promise.f_SetException(fg_Move(p_Result));
					return true;
				}
				()...
			}
		;
		(void)Dummy;
		if (m_Promise.f_IsSet())
			return;
		m_Promise.f_SetResult();
	}
}
