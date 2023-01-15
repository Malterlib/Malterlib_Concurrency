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

	template <typename ...tfp_CResult>
	void CPromiseReceiveAnyUnwrapFunctor::operator () (tfp_CResult && ...p_Result)
	{
		if (m_Promise.f_IsSet())
			return;

		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bSuccess = true;

		TCInitializerList<bool> Dummy =
			{
				[&]
				{
					if (m_Promise.f_IsSet())
						return true;
					if (!p_Result)
					{
						ErrorCollector.f_AddError(fg_Move(p_Result).f_GetException());
						bSuccess = false;
					}
					else
					{
						if constexpr (NContainer::TCIsContainer<typename NTraits::TCRemoveReferenceAndQualifiers<decltype(*p_Result)>::CType>::mc_Value)
						{
							try
							{
								fg_Unwrap(fg_Move(*p_Result));
							}
							catch (NException::CExceptionCoroutineWrapper &_WrappedException) // When a co_await returns an exception
							{
								ErrorCollector.f_AddError(fg_Move(_WrappedException.f_GetSpecific().m_pException));
								bSuccess = false;
							}
							catch (NException::CExceptionBase &_Exception)
							{
								ErrorCollector.f_AddError(fg_Move(_Exception).f_ExceptionPointer());
								bSuccess = false;
							}
						}
					}
					return true;
				}
				()...
			}
		;

		if (!bSuccess)
		{
			m_Promise.f_SetException(fg_Move(ErrorCollector).f_GetException());
			return;
		}

		(void)Dummy;
		if (m_Promise.f_IsSet())
			return;
		m_Promise.f_SetResult();
	}
}
