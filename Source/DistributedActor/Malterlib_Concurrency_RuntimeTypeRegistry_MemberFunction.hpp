// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once
#include <Mib/Storage/Aggregate>

namespace NMib::NConcurrency::NPrivate
{
	template
	<
		auto t_pMemberFunction
		, uint32 t_NameHash
		, typename t_CStreamContext
		, typename t_CStreamParams
		, typename t_CStreamResult
		, typename t_CReturn
	>
	TCRuntimeTypeRegistryEntry_MemberFunction
		<
			t_pMemberFunction
			, t_NameHash
			, t_CStreamContext
			, t_CStreamParams
			, t_CStreamResult
			, t_CReturn
		>::TCRuntimeTypeRegistryEntry_MemberFunction()
		: CRuntimeTypeRegistryEntry_MemberFunction
		(
			t_NameHash
			, DMibConstantTypeHash(typename NTraits::TCMemberFunctionPointerTraits<decltype(t_pMemberFunction)>::CClass)
			, TCLowestSupportedVersionForMemberFunction<t_pMemberFunction>::mc_Value
		)
	{
	}

	template <typename tf_CStream, mint... tfp_Indices, typename... tfp_CParams>
	auto fg_DecodeParams
		(
			tf_CStream &_ParamsStream
			, NMeta::TCIndices<tfp_Indices...> const &
			, NMeta::TCTypeList<tfp_CParams...> const &
		)
	{
		NStorage::TCTuple<typename NTraits::TCDecay<tfp_CParams>::CType...> ParamList;
		TCInitializerList<bool> Dummy =
			{
				[&]
				{
					_ParamsStream >> fg_Get<tfp_Indices>(ParamList);
					return true;
				}
				()...
			}
		;
		(void)Dummy;
		return ParamList;
	}

	template
	<
		auto t_pMemberFunction
		, uint32 t_NameHash
		, typename t_CStreamContext
		, typename t_CStreamParams
		, typename t_CStreamResult
		, typename t_CReturn
	>
	template <mint... tfp_Indices, typename... tfp_CParams>
	auto TCRuntimeTypeRegistryEntry_MemberFunction
		<
			t_pMemberFunction
			, t_NameHash
			, t_CStreamContext
			, t_CStreamParams
			, t_CStreamResult
			, t_CReturn
		>::fp_Call
		(
			t_CStreamParams &_ParamsStream
			, void *_pObject
			, NMeta::TCIndices<tfp_Indices...> const &_Indices
			, NMeta::TCTypeList<tfp_CParams...> const &_TypeList
		)
		-> NConcurrency::TCFuture<NContainer::CSecureByteVector>
	{
		NConcurrency::TCPromise<NContainer::CSecureByteVector> Promise;

		using CClass = typename NTraits::TCMemberFunctionPointerTraits<decltype(t_pMemberFunction)>::CClass;

		NStorage::TCTuple<typename NTraits::TCDecay<tfp_CParams>::CType...> ParamList;

		try
		{
			ParamList = fg_DecodeParams(_ParamsStream, _Indices, _TypeList);
		}
		catch (NException::CException const &_Exception)
		{
			return Promise <<= _Exception;
		}

		NConcurrency::TCAsyncResult<t_CReturn> AsyncResult;

		if constexpr (TCIsAsyncGenerator<t_CReturn>::mc_Value)
			AsyncResult.f_SetResult(fg_CallSafe((CClass *)_pObject, t_pMemberFunction, fg_Forward<tfp_CParams>(fg_Get<tfp_Indices>(ParamList))...));
		else
			AsyncResult.f_SetResult((((CClass *)_pObject)->*t_pMemberFunction)(fg_Forward<tfp_CParams>(fg_Get<tfp_Indices>(ParamList))...));

		return Promise <<= fg_StreamAsyncResult<t_CStreamResult>(fg_Move(AsyncResult), _ParamsStream.f_GetContext(), _ParamsStream.f_GetVersion());
	}

	template
	<
		auto t_pMemberFunction
		, uint32 t_NameHash
		, typename t_CStreamContext
		, typename t_CStreamParams
		, typename t_CStreamResult
		, typename t_CReturn
	>
	auto TCRuntimeTypeRegistryEntry_MemberFunction
		<
			t_pMemberFunction
			, t_NameHash
			, t_CStreamContext
			, t_CStreamParams
			, t_CStreamResult
			, t_CReturn
		>::f_Call
		(
			NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream
			, void *_pObject
		)
		-> NConcurrency::TCFuture<NContainer::CSecureByteVector>
	{
		using CParams = typename NTraits::TCMemberFunctionPointerTraits<decltype(t_pMemberFunction)>::CParams;
		return fp_Call
			(
				static_cast<t_CStreamParams &>(_Stream)
				, _pObject
				,
				typename NMeta::TCMakeConsecutiveIndices<NMeta::TCTypeList_Len<CParams>::mc_Value>::CType()
				, CParams()
			)
		;
	}

	template
	<
		auto t_pMemberFunction
		, uint32 t_NameHash
		, typename t_CStreamContext
		, typename t_CStreamParams
		, typename t_CStreamResult
	>
	TCRuntimeTypeRegistryEntry_MemberFunction
		<
			t_pMemberFunction
			, t_NameHash
			, t_CStreamContext
			, t_CStreamParams
			, t_CStreamResult
			, void
		>::TCRuntimeTypeRegistryEntry_MemberFunction()
		: CRuntimeTypeRegistryEntry_MemberFunction
		(
			t_NameHash
			, DMibConstantTypeHash(typename NTraits::TCMemberFunctionPointerTraits<decltype(t_pMemberFunction)>::CClass)
			, TCLowestSupportedVersionForMemberFunction<t_pMemberFunction>::mc_Value
		)
	{
	}

	template
	<
		auto t_pMemberFunction
		, uint32 t_NameHash
		, typename t_CStreamContext
		, typename t_CStreamParams
		, typename t_CStreamResult
	>
	template <mint... tfp_Indices, typename... tfp_CParams>
	auto TCRuntimeTypeRegistryEntry_MemberFunction
		<
			t_pMemberFunction
			, t_NameHash
			, t_CStreamContext
			, t_CStreamParams
			, t_CStreamResult
			, void
		>::fp_Call
		(
			t_CStreamParams &_ParamsStream
			, void *_pObject
			, NMeta::TCIndices<tfp_Indices...> const &_Indices
			, NMeta::TCTypeList<tfp_CParams...> const &_TypeList
		)
		-> NConcurrency::TCFuture<NContainer::CSecureByteVector>
	{
		NConcurrency::TCPromise<NContainer::CSecureByteVector> Promise;

		using CClass = typename NTraits::TCMemberFunctionPointerTraits<decltype(t_pMemberFunction)>::CClass;

		NStorage::TCTuple<typename NTraits::TCDecay<tfp_CParams>::CType...> ParamList;

		try
		{
			ParamList = fg_DecodeParams(_ParamsStream, _Indices, _TypeList);
		}
		catch (NException::CException const &_Exception)
		{
			return Promise <<= _Exception;
		}

		(((CClass *)_pObject)->*t_pMemberFunction)(fg_Forward<tfp_CParams>(fg_Get<tfp_Indices>(ParamList))...);

		NConcurrency::TCAsyncResult<void> AsyncResult;
		AsyncResult.f_SetResult();

		return Promise <<= fg_StreamAsyncResult<t_CStreamResult>(fg_Move(AsyncResult), _ParamsStream.f_GetContext(), _ParamsStream.f_GetVersion());
	}

	template
	<
		auto t_pMemberFunction
		, uint32 t_NameHash
		, typename t_CStreamContext
		, typename t_CStreamParams
		, typename t_CStreamResult
	>
	auto TCRuntimeTypeRegistryEntry_MemberFunction
		<
			t_pMemberFunction
			, t_NameHash
			, t_CStreamContext
			, t_CStreamParams
			, t_CStreamResult
			, void
		>::f_Call
		(
			NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream
			, void *_pObject
		)
		-> NConcurrency::TCFuture<NContainer::CSecureByteVector>
	{
		using CParams = typename NTraits::TCMemberFunctionPointerTraits<decltype(t_pMemberFunction)>::CParams;
		return fp_Call
			(
				static_cast<t_CStreamParams &>(_Stream)
				, _pObject
				,
				typename NMeta::TCMakeConsecutiveIndices<NMeta::TCTypeList_Len<CParams>::mc_Value>::CType()
				, CParams()
			)
		;
	}

#ifndef DDocumentation_Doxygen
	template
	<
		auto t_pMemberFunction
		, uint32 t_NameHash
		, typename t_CStreamContext
		, typename t_CStreamParams
		, typename t_CStreamResult
		, typename t_CResult
	>
	TCRuntimeTypeRegistryEntry_MemberFunction
		<
			t_pMemberFunction
			, t_NameHash
			, t_CStreamContext
			, t_CStreamParams
			, t_CStreamResult
			, NConcurrency::TCFuture<t_CResult>
		>::TCRuntimeTypeRegistryEntry_MemberFunction()
		: CRuntimeTypeRegistryEntry_MemberFunction
		(
			t_NameHash
			, DMibConstantTypeHash(typename NTraits::TCMemberFunctionPointerTraits<decltype(t_pMemberFunction)>::CClass)
			, TCLowestSupportedVersionForMemberFunction<t_pMemberFunction>::mc_Value
		)
	{
	}

	template
	<
		auto t_pMemberFunction
		, uint32 t_NameHash
		, typename t_CStreamContext
		, typename t_CStreamParams
		, typename t_CStreamResult
		, typename t_CResult
	>
	template <mint... tfp_Indices, typename... tfp_CParams>
	auto TCRuntimeTypeRegistryEntry_MemberFunction
		<
			t_pMemberFunction
			, t_NameHash
			, t_CStreamContext
			, t_CStreamParams
			, t_CStreamResult
			, NConcurrency::TCFuture<t_CResult>
		>::fp_Call
		(
			t_CStreamParams &_ParamsStream
			, void *_pObject
			, NMeta::TCIndices<tfp_Indices...> const &_Indices
			, NMeta::TCTypeList<tfp_CParams...> const &_TypeList
		)
		-> NConcurrency::TCFuture<NContainer::CSecureByteVector>
	{
		using CClass = typename NTraits::TCMemberFunctionPointerTraits<decltype(t_pMemberFunction)>::CClass;

		NConcurrency::TCPromise<NContainer::CSecureByteVector> Return;

		struct CState
		{
			t_CStreamContext m_Context;
			NStorage::TCTuple<typename NTraits::TCDecay<tfp_CParams>::CType...> m_ParamList;
			uint32 m_Version;
		};

		t_CStreamContext *pContext = (t_CStreamContext *)_ParamsStream.f_GetContext();

		NStorage::TCUniquePointer<CState> pState;
		try
		{
			pState = NStorage::TCUniquePointer<CState>
				(
				 	new CState{*pContext, fg_DecodeParams(_ParamsStream, _Indices, _TypeList), _ParamsStream.f_GetVersion()}
				)
			;
		}
		catch (NException::CException const &_Exception)
		{
			return Return <<= _Exception;
		}

		auto &State = *pState;

		NFunction::TCFunctionMovable<void (TCAsyncResult<t_CResult> &&_AsyncResult)> fOnResultSet
			= [Return, pState = fg_Move(pState)](NConcurrency::TCAsyncResult<t_CResult> &&_Result) mutable
			{
				Return.f_SetResult(fg_StreamAsyncResult<t_CStreamResult>(fg_Move(_Result), &pState->m_Context, pState->m_Version));
			}
		;

		auto &ThreadLocal = **g_SystemThreadLocal;

#if DMibEnableSafeCheck > 0
		bool bPreviousExpectCoroutineCall = ThreadLocal.m_bExpectCoroutineCall;
		ThreadLocal.m_bExpectCoroutineCall = true;
		auto Cleanup = g_OnScopeExit / [&]
			{
				ThreadLocal.m_bExpectCoroutineCall = bPreviousExpectCoroutineCall;
			}
		;
#endif

		auto &PromiseThreadLocal = ThreadLocal.m_PromiseThreadLocal;
		auto pPreviousOnResultSet = PromiseThreadLocal.m_pOnResultSet;
		PromiseThreadLocal.m_pOnResultSet = &fOnResultSet;

		auto CleanupOnResultSet = g_OnScopeExit / [&]
			{
				PromiseThreadLocal.m_pOnResultSet = pPreviousOnResultSet;
			}
		;

#if DMibEnableSafeCheck > 0
		auto pPreviousOnResultSetConsumedBy = PromiseThreadLocal.m_pOnResultSetConsumedBy;
		PromiseThreadLocal.m_pOnResultSetConsumedBy = nullptr;

		auto pPreviousExpectCoroutineCallSetConsumedBy = PromiseThreadLocal.m_pExpectCoroutineCallSetConsumedBy;
		PromiseThreadLocal.m_pExpectCoroutineCallSetConsumedBy = nullptr;

		auto bPreviousCaptureDebugException = PromiseThreadLocal.m_bCaptureDebugException;
		PromiseThreadLocal.m_bCaptureDebugException = true;

		auto CleanupOnResultSetConsumedBy = g_OnScopeExit / [&]
			{
				PromiseThreadLocal.m_pOnResultSetConsumedBy = pPreviousOnResultSetConsumedBy;
				PromiseThreadLocal.m_pExpectCoroutineCallSetConsumedBy = pPreviousExpectCoroutineCallSetConsumedBy;
				PromiseThreadLocal.m_bCaptureDebugException = bPreviousCaptureDebugException;
			}
		;

		PromiseThreadLocal.m_OnResultSetTypeHash = fg_GetTypeHash<t_CResult>();

		auto Future = (((CClass *)_pObject)->*t_pMemberFunction)(fg_Forward<tfp_CParams>(fg_Get<tfp_Indices>(State.m_ParamList))...);

		DMibFastCheck(PromiseThreadLocal.m_pOnResultSet == &fOnResultSet || Future.f_HasData(PromiseThreadLocal.m_pOnResultSetConsumedBy));
		DMibFastCheck
			(
				ThreadLocal.m_bExpectCoroutineCall
				|| !PromiseThreadLocal.m_pExpectCoroutineCallSetConsumedBy
				|| Future.f_HasData(PromiseThreadLocal.m_pExpectCoroutineCallSetConsumedBy)
			)
		;

		ThreadLocal.m_bExpectCoroutineCall = bPreviousExpectCoroutineCall;
		Cleanup.f_Clear();
#else
		auto Future = (((CClass *)_pObject)->*t_pMemberFunction)(fg_Forward<tfp_CParams>(fg_Get<tfp_Indices>(State.m_ParamList))...);
#endif
		if (PromiseThreadLocal.m_pOnResultSet)
		{
			DMibFastCheck(PromiseThreadLocal.m_pOnResultSet == &fOnResultSet);
			Future.f_OnResultSet(fg_Move(fOnResultSet));
		}

		return Return.f_MoveFuture();
	}

	template
	<
		auto t_pMemberFunction
		, uint32 t_NameHash
		, typename t_CStreamContext
		, typename t_CStreamParams
		, typename t_CStreamResult
		, typename t_CResult
	>
	auto TCRuntimeTypeRegistryEntry_MemberFunction
		<
			t_pMemberFunction
			, t_NameHash
			, t_CStreamContext
			, t_CStreamParams
			, t_CStreamResult
			, NConcurrency::TCFuture<t_CResult>
		>::f_Call
		(
			NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream
			, void *_pObject
		)
		-> NConcurrency::TCFuture<NContainer::CSecureByteVector>
	{
		using CParams = typename NTraits::TCMemberFunctionPointerTraits<decltype(t_pMemberFunction)>::CParams;
		return fp_Call
			(
				static_cast<t_CStreamParams &>(_Stream)
				, _pObject
				,
				typename NMeta::TCMakeConsecutiveIndices<NMeta::TCTypeList_Len<CParams>::mc_Value>::CType()
				, CParams()
			)
		;
	}
#endif

	template
	<
		auto t_pMemberFunction
		DMibIfNotSupportMemberNameFromMemberPointer(, uint32 t_NameHash)
		, typename t_CStreamContext
		, typename t_CStreamParams
		, typename t_CStreamResult
	>
	TCRuntimeTypeRegistryEntry_MemberFunctionInit
		<
			t_pMemberFunction
			DMibIfNotSupportMemberNameFromMemberPointer(, t_NameHash)
			, t_CStreamContext
			, t_CStreamParams
			, t_CStreamResult
		>
		TCMemberFunctionRegistry
		<
			t_pMemberFunction
			DMibIfNotSupportMemberNameFromMemberPointer(, t_NameHash)
			, t_CStreamContext
			, t_CStreamParams
			, t_CStreamResult
		>::ms_EntryInit{typename TCAlternateHashesForMemberFunction<t_pMemberFunction>::CAlternates()}
	;

	template
	<
		auto t_pMemberFunction
		, uint32 t_NameHash
		, typename t_CStreamContext
		, typename t_CStreamParams
		, typename t_CStreamResult
	>
	struct TCMemberFunctionRegistryImpl
	{
		static NStorage::TCAggregate
			<
				TCRuntimeTypeRegistryEntry_MemberFunction
				<
					t_pMemberFunction
					, t_NameHash
					, t_CStreamContext
					, t_CStreamParams
					, t_CStreamResult
				>
			> ms_Entry
		;
	};

	template
	<
		auto t_pMemberFunction
		, uint32 t_NameHash
		, typename t_CStreamContext
		, typename t_CStreamParams
		, typename t_CStreamResult
	>
	constinit NStorage::TCAggregate
		<
			TCRuntimeTypeRegistryEntry_MemberFunction
			<
				t_pMemberFunction
				, t_NameHash
				, t_CStreamContext
				, t_CStreamParams
				, t_CStreamResult
			>
		>
		TCMemberFunctionRegistryImpl
		<
			t_pMemberFunction
			, t_NameHash
			, t_CStreamContext
			, t_CStreamParams
			, t_CStreamResult
		>::ms_Entry = {DAggregateInit}
	;

	template
	<
		auto t_pMemberFunction
		DMibIfNotSupportMemberNameFromMemberPointer(, uint32 t_NameHash)
		, typename t_CStreamContext
		, typename t_CStreamParams
		, typename t_CStreamResult
	>
	template <typename ...tfp_CAlternate>
	TCRuntimeTypeRegistryEntry_MemberFunctionInit
		<
			t_pMemberFunction
			DMibIfNotSupportMemberNameFromMemberPointer(, t_NameHash)
			, t_CStreamContext
			, t_CStreamParams
			, t_CStreamResult
		>::TCRuntimeTypeRegistryEntry_MemberFunctionInit(NMeta::TCTypeList<tfp_CAlternate...>)
	{
		*TCMemberFunctionRegistryImpl
			<
				t_pMemberFunction
				, ::NMib::fg_GetMemberFunctionHash<t_pMemberFunction>(DMibIfNotSupportMemberNameFromMemberPointer(t_NameHash))
				, t_CStreamContext
				, t_CStreamParams
				, t_CStreamResult
			>::ms_Entry
		;

		[[maybe_unused]] TCInitializerList<bool> Dummy =
			{
				[&]
				{
					*TCMemberFunctionRegistryImpl
						<
							t_pMemberFunction
							, tfp_CAlternate::mc_Hash
							, t_CStreamContext
							, t_CStreamParams
							, t_CStreamResult
						>::ms_Entry
					;
					return true;
				}
				()...
			}
		;

	}
}
