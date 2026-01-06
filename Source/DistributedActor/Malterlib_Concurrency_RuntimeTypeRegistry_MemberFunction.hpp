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
		NStorage::TCTuple<NTraits::TCDecay<tfp_CParams>...> ParamList;

		(
			[&]
			{
				_ParamsStream >> fg_Get<tfp_Indices>(ParamList);
			}
			()
			, ...
		);

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
		-> NConcurrency::TCFuture<NContainer::CIOByteVector>
		requires (TCIsAsyncGenerator<t_CReturn>::mc_Value)
	{
		using CClass = typename NTraits::TCMemberFunctionPointerTraits<decltype(t_pMemberFunction)>::CClass;

		NStorage::TCTuple<NTraits::TCDecay<tfp_CParams>...> ParamList;

		try
		{
			ParamList = fg_DecodeParams(_ParamsStream, _Indices, _TypeList);
		}
		catch (NException::CException const &_Exception)
		{
			return _Exception.f_ExceptionPointer();
		}

		NConcurrency::TCAsyncResult<t_CReturn> AsyncResult;

		AsyncResult.f_SetResult((((CClass *)_pObject)->*t_pMemberFunction)(fg_Move(fg_Get<tfp_Indices>(ParamList))...));

		return fg_StreamAsyncResult<t_CStreamResult>(fg_Move(AsyncResult), _ParamsStream.f_GetContext(), _ParamsStream.f_GetVersion());
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
		-> NConcurrency::TCUnsafeFuture<NContainer::CIOByteVector>
		requires (!TCIsAsyncGenerator<t_CReturn>::mc_Value)
	{
		using CClass = typename NTraits::TCMemberFunctionPointerTraits<decltype(t_pMemberFunction)>::CClass;

		NStorage::TCTuple<NTraits::TCDecay<tfp_CParams>...> ParamList;

		try
		{
			ParamList = fg_DecodeParams(_ParamsStream, _Indices, _TypeList);
		}
		catch (NException::CException const &_Exception)
		{
			co_return _Exception.f_ExceptionPointer();
		}

		NConcurrency::TCAsyncResult<t_CReturn> AsyncResult;

		AsyncResult.f_SetResult((((CClass *)_pObject)->*t_pMemberFunction)(fg_Move(fg_Get<tfp_Indices>(ParamList))...));

		co_return fg_StreamAsyncResult<t_CStreamResult>(fg_Move(AsyncResult), _ParamsStream.f_GetContext(), _ParamsStream.f_GetVersion());
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
		-> NConcurrency::TCFuture<NContainer::CIOByteVector>
	{
		using CParams = typename NTraits::TCMemberFunctionPointerTraits<decltype(t_pMemberFunction)>::CParams;
		return fp_Call
			(
				static_cast<t_CStreamParams &>(_Stream)
				, _pObject
				, NMeta::TCConsecutiveIndices<NMeta::gc_TypeList_Len<CParams>>()
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
		-> NConcurrency::TCUnsafeFuture<NContainer::CIOByteVector>
	{
		using CClass = typename NTraits::TCMemberFunctionPointerTraits<decltype(t_pMemberFunction)>::CClass;

		NStorage::TCTuple<NTraits::TCDecay<tfp_CParams>...> ParamList;

		try
		{
			ParamList = fg_DecodeParams(_ParamsStream, _Indices, _TypeList);
		}
		catch (NException::CException const &_Exception)
		{
			co_return _Exception.f_ExceptionPointer();
		}

		(((CClass *)_pObject)->*t_pMemberFunction)(fg_Move(fg_Get<tfp_Indices>(ParamList))...);

		NConcurrency::TCAsyncResult<void> AsyncResult;
		AsyncResult.f_SetResult();

		co_return fg_StreamAsyncResult<t_CStreamResult>(fg_Move(AsyncResult), _ParamsStream.f_GetContext(), _ParamsStream.f_GetVersion());
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
		-> NConcurrency::TCFuture<NContainer::CIOByteVector>
	{
		using CParams = typename NTraits::TCMemberFunctionPointerTraits<decltype(t_pMemberFunction)>::CParams;
		return fp_Call
			(
				static_cast<t_CStreamParams &>(_Stream)
				, _pObject
				,
				NMeta::TCConsecutiveIndices<NMeta::gc_TypeList_Len<CParams>>()
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
		-> NConcurrency::TCFuture<NContainer::CIOByteVector>
	{
		using CClass = typename NTraits::TCMemberFunctionPointerTraits<decltype(t_pMemberFunction)>::CClass;

		t_CStreamContext *pContext = (t_CStreamContext *)_ParamsStream.f_GetContext();

		NStorage::TCTuple<NTraits::TCDecay<tfp_CParams>...> ParamList;
		try
		{
			ParamList = fg_DecodeParams(_ParamsStream, _Indices, _TypeList);
		}
		catch (NException::CException const &_Exception)
		{
			return _Exception.f_ExceptionPointer();
		}

		NConcurrency::TCPromiseFuturePair<NContainer::CIOByteVector> Return;

		TCFutureOnResult<t_CResult> fOnResultSet =
#if DMibEnableSafeCheck > 0 && defined(DMibCheckOnResultSizes)
		TCFutureOnResultOverSized<t_CResult>
			(
#endif
				[Return = fg_Move(Return.m_Promise), Context = *pContext, Version = _ParamsStream.f_GetVersion()]
				(NConcurrency::TCAsyncResult<t_CResult> &&_Result) mutable
				{
					Return.f_SetResult(fg_StreamAsyncResult<t_CStreamResult>(fg_Move(_Result), &Context, Version));
				}
#if DMibEnableSafeCheck > 0 && defined(DMibCheckOnResultSizes)
			)
#endif
		;

		auto &ThreadLocal = fg_SystemThreadLocal();
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

		auto pPreviousExpectCoroutineCallSetConsumedBy = PromiseThreadLocal.m_pExpectCoroutineCallConsumedBy;
		PromiseThreadLocal.m_pExpectCoroutineCallConsumedBy = nullptr;

		auto bPreviousCaptureDebugException = PromiseThreadLocal.m_bCaptureDebugException;
		PromiseThreadLocal.m_bCaptureDebugException = true;

		auto CleanupOnResultSetConsumedBy = g_OnScopeExit / [&]
			{
				PromiseThreadLocal.m_pOnResultSetConsumedBy = pPreviousOnResultSetConsumedBy;
				PromiseThreadLocal.m_pExpectCoroutineCallConsumedBy = pPreviousExpectCoroutineCallSetConsumedBy;
				PromiseThreadLocal.m_bCaptureDebugException = bPreviousCaptureDebugException;
			}
		;

		PromiseThreadLocal.m_OnResultSetTypeHash = fg_GetTypeHash<t_CResult>();

		auto Future = (((CClass *)_pObject)->*t_pMemberFunction)(fg_Move(fg_Get<tfp_Indices>(ParamList))...);

		DMibFastCheck(PromiseThreadLocal.m_pOnResultSet == &fOnResultSet || Future.f_Debug_HasData(PromiseThreadLocal.m_pOnResultSetConsumedBy));
#else
		auto Future = (((CClass *)_pObject)->*t_pMemberFunction)(fg_Move(fg_Get<tfp_Indices>(ParamList))...);
#endif
		if (PromiseThreadLocal.m_pOnResultSet)
		{
			DMibFastCheck(PromiseThreadLocal.m_pOnResultSet == &fOnResultSet);
			Future.f_OnResultSet(fg_Move(fOnResultSet));
		}

		return fg_Move(Return.m_Future);
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
		-> NConcurrency::TCFuture<NContainer::CIOByteVector>
	{
		using CParams = typename NTraits::TCMemberFunctionPointerTraits<decltype(t_pMemberFunction)>::CParams;
		return fp_Call
			(
				static_cast<t_CStreamParams &>(_Stream)
				, _pObject
				, NMeta::TCConsecutiveIndices<NMeta::gc_TypeList_Len<CParams>>()
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

		(
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
			}
			()
			, ...
		);
	}
}
