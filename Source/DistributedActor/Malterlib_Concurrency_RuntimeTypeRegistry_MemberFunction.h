// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	struct CRuntimeTypeRegistryEntry_MemberFunction
	{
		CRuntimeTypeRegistryEntry_MemberFunction
			(
				uint32 _Hash
				, uint32 _TypeHash
				, uint32 _LowestSupportedVersion
			)
		;
		~CRuntimeTypeRegistryEntry_MemberFunction();

		virtual NConcurrency::TCFuture<NContainer::CIOByteVector> f_Call
			(
				NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream
				, void *_pObject
			)
			= 0
		;

		uint32 m_Hash;
		uint32 m_TypeHash;
		uint32 m_LowestSupportedVersion;

		NIntrusive::TCAVLLink<> m_HashLink;
		NIntrusive::TCAVLLink<> m_MemberPointerLink;
	};

	template <auto t_pMemberFunction>
	struct TCLowestSupportedVersionForMemberFunction
	{
		static constexpr uint32 mc_Value = 0x0;
	};

#define DMibConcurrencyRegisterMemberFunctionLowestVersion(d_MemberFunction, d_LowestVersion) \
	template <> \
	struct NMib::NConcurrency::TCLowestSupportedVersionForMemberFunction<&d_MemberFunction> \
	{ \
		static constexpr uint32 mc_Value = d_LowestVersion; \
	};

	struct CAlternateHashForMemberFunction
	{
		uint32 m_Hash;
		uint32 m_UpToVersion;
	};

	template <uint32 t_Hash, uint32 t_UpToVersion>
	struct TCAlternateHashForMemberFunction
	{
		static constexpr uint32 mc_Hash = t_Hash;
		static constexpr uint32 mc_UpToVersion = t_UpToVersion;
	};

	template <typename t_CAlternates>
	struct TCGetAlternatesHashesArray;

	template <uint32 ...tp_Hash, uint32 ...tp_UpToVersion>
	struct TCGetAlternatesHashesArray<NMeta::TCTypeList<TCAlternateHashForMemberFunction<tp_Hash, tp_UpToVersion>...>>
	{
		static constexpr CAlternateHashForMemberFunction mc_Value[] = {{tp_Hash, tp_UpToVersion}...};
	};

	template <auto t_pMemberFunction>
	struct TCAlternateHashesForMemberFunction
	{
		static constexpr bool mc_bHasAlternates = false;
		using CAlternates = NMeta::TCTypeList<>;
	};

#define DMibConcurrencyRegisterMemberFunctionAlternateHashes(d_MemberFunction, ...) \
	template <> \
	struct NMib::NConcurrency::TCAlternateHashesForMemberFunction<&d_MemberFunction> \
	{ \
		static constexpr bool mc_bHasAlternates = true; \
		using CAlternates = NMeta::TCTypeList<__VA_ARGS__>; \
		using CAlternatesArray = TCGetAlternatesHashesArray<CAlternates>; \
	};
}

namespace NMib::NConcurrency::NPrivate
{
	template <typename t_CType>
	struct TCIsAsyncGenerator;

	template
	<
		auto t_pMemberFunction
		, uint32 t_NameHash
		, typename t_CStreamContext
		, typename t_CStreamParams
		, typename t_CStreamResult
		, typename t_CReturn = typename NTraits::TCMemberFunctionPointerTraits<decltype(t_pMemberFunction)>::CReturn
	>
	struct TCRuntimeTypeRegistryEntry_MemberFunction final : public CRuntimeTypeRegistryEntry_MemberFunction
	{
		TCRuntimeTypeRegistryEntry_MemberFunction();

		template <umint... tfp_Indices, typename... tfp_CParams>
		NConcurrency::TCFuture<NContainer::CIOByteVector> fp_Call
			(
				t_CStreamParams &_ParamsStream
				, void *_pObject
				, NMeta::TCIndices<tfp_Indices...> const &
				, NMeta::TCTypeList<tfp_CParams...> const &
			)
			requires (TCIsAsyncGenerator<t_CReturn>::mc_Value)
		;

		template <umint... tfp_Indices, typename... tfp_CParams>
		NConcurrency::TCUnsafeFuture<NContainer::CIOByteVector> fp_Call
			(
				t_CStreamParams &_ParamsStream
				, void *_pObject
				, NMeta::TCIndices<tfp_Indices...> const &
				, NMeta::TCTypeList<tfp_CParams...> const &
			)
			requires (!TCIsAsyncGenerator<t_CReturn>::mc_Value)
		;

		NConcurrency::TCFuture<NContainer::CIOByteVector> f_Call
			(
				NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream
				, void *_pObject
			)
			override
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
	struct TCRuntimeTypeRegistryEntry_MemberFunction
		<
			t_pMemberFunction
			, t_NameHash
			, t_CStreamContext
			, t_CStreamParams
			, t_CStreamResult
			, void
		> final
		: public CRuntimeTypeRegistryEntry_MemberFunction
	{
		TCRuntimeTypeRegistryEntry_MemberFunction();
		template <umint... tfp_Indices, typename... tfp_CParams>
		NConcurrency::TCUnsafeFuture<NContainer::CIOByteVector> fp_Call
			(
				t_CStreamParams &_ParamsStream
				, void *_pObject
				, NMeta::TCIndices<tfp_Indices...> const &
				, NMeta::TCTypeList<tfp_CParams...> const &
			)
		;

		NConcurrency::TCFuture<NContainer::CIOByteVector> f_Call
			(
				NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream
				, void *_pObject
			)
			override
		;
	};

	template
	<
		auto t_pMemberFunction
		, uint32 t_NameHash
		, typename t_CStreamContext
		, typename t_CStreamParams
		, typename t_CStreamResult
		, typename t_CResult
	>
	struct TCRuntimeTypeRegistryEntry_MemberFunction
		<
			t_pMemberFunction
			, t_NameHash
			, t_CStreamContext
			, t_CStreamParams
			, t_CStreamResult
			, NConcurrency::TCFuture<t_CResult>
		> final
		: public CRuntimeTypeRegistryEntry_MemberFunction
	{
		TCRuntimeTypeRegistryEntry_MemberFunction();

		template <umint... tfp_Indices, typename... tfp_CParams>
		NConcurrency::TCFuture<NContainer::CIOByteVector> fp_Call
			(
				t_CStreamParams &_ParamsStream
				, void *_pObject
				, NMeta::TCIndices<tfp_Indices...> const &
				, NMeta::TCTypeList<tfp_CParams...> const &
			)
		;

		NConcurrency::TCFuture<NContainer::CIOByteVector> f_Call
			(
				NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream
				, void *_pObject
			)
			override
		;
	};

	template
	<
		auto t_pMemberFunction
		DMibIfNotSupportMemberNameFromMemberPointer(, uint32 t_NameHash)
		, typename t_CStreamContext
		, typename t_CStreamParams
		, typename t_CStreamResult
	>
	struct TCRuntimeTypeRegistryEntry_MemberFunctionInit
	{
		template <typename ...tfp_CAlternate>
		TCRuntimeTypeRegistryEntry_MemberFunctionInit(NMeta::TCTypeList<tfp_CAlternate...>);
	};

	template
	<
		auto t_pMemberFunction
		DMibIfNotSupportMemberNameFromMemberPointer(, uint32 t_NameHash)
		, typename t_CStreamContext = zuint32
		, typename t_CStreamParams = NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault>
		, typename t_CStreamResult = NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CIOByteVector>
	>
	struct TCMemberFunctionRegistry
	{
		static TCRuntimeTypeRegistryEntry_MemberFunctionInit
			<
				t_pMemberFunction
				DMibIfNotSupportMemberNameFromMemberPointer(, t_NameHash)
				, t_CStreamContext
				, t_CStreamParams
				, t_CStreamResult
			>
			ms_EntryInit
		;
	};
}

#define DMibConcurrencyRegisterMemberFunction(d_pMemberFunction) \
	(void)::NMib::NConcurrency::NPrivate::TCMemberFunctionRegistry<DMibPointerToMemberFunctionForHash(d_pMemberFunction)>::ms_EntryInit

#define DMibConcurrencyRegisterMemberFunctionWithStreams(d_pMemberFunction, d_StreamContext, d_StreamParams, d_StreamResult) \
	(void)::NMib::NConcurrency::NPrivate::TCMemberFunctionRegistry<DMibPointerToMemberFunctionForHash(d_pMemberFunction), d_StreamContext, d_StreamParams, d_StreamResult>::ms_EntryInit
