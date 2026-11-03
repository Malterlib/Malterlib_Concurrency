// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	struct CRuntimeTypeRegistryEntry_MemberFunction
	{
		CRuntimeTypeRegistryEntry_MemberFunction(uint32 _Hash, uint32 _TypeHash, uint32 _LowestSupportedVersion);
		~CRuntimeTypeRegistryEntry_MemberFunction();

		virtual NConcurrency::TCFuture<NStream::CBinaryStorage> f_Call(NStream::TCBinaryStreamStoragePtr<NStream::CBinaryStreamDefault> &_Stream, void *_pObject) = 0;

		uint32 m_Hash;
		uint32 m_TypeHash;
		uint32 m_LowestSupportedVersion;

		NIntrusive::TCAVLLink<> m_HashLink;
		NIntrusive::TCAVLLink<> m_MemberPointerLink;
	};

	template <auto t_fMemberFunction>
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

	template <auto t_fMemberFunction>
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
		auto t_fMemberFunction
		, uint32 t_NameHash
		, typename t_CStreamContext
		, typename t_CStreamParams
		, typename t_CStreamResult
		, typename t_CReturn = typename NTraits::TCMemberFunctionPointerTraits<decltype(t_fMemberFunction)>::CReturn
	>
	struct TCRuntimeTypeRegistryEntry_MemberFunction final : public CRuntimeTypeRegistryEntry_MemberFunction
	{
		TCRuntimeTypeRegistryEntry_MemberFunction();

		NConcurrency::TCFuture<NStream::CBinaryStorage> f_Call(NStream::TCBinaryStreamStoragePtr<NStream::CBinaryStreamDefault> &_Stream, void *_pObject) override;

	private:
		template <umint ...tfp_Indices, typename ...tfp_CParams>
		auto fp_Call(t_CStreamParams &_ParamsStream, void *_pObject, NMeta::TCIndices<tfp_Indices...> const &_Indices, NMeta::TCTypeList<tfp_CParams...> const &_ParamTypes)
			-> NConcurrency::TCFuture<NStream::CBinaryStorage>
			requires (TCIsAsyncGenerator<t_CReturn>::mc_Value)
		;

		template <umint ...tfp_Indices, typename ...tfp_CParams>
		auto fp_Call(t_CStreamParams &_ParamsStream, void *_pObject, NMeta::TCIndices<tfp_Indices...> const &_Indices, NMeta::TCTypeList<tfp_CParams...> const &_ParamTypes)
			-> NConcurrency::TCUnsafeFuture<NStream::CBinaryStorage>
			requires (!TCIsAsyncGenerator<t_CReturn>::mc_Value)
		;
	};

	template <auto t_fMemberFunction, uint32 t_NameHash, typename t_CStreamContext, typename t_CStreamParams, typename t_CStreamResult>
	struct TCRuntimeTypeRegistryEntry_MemberFunction<t_fMemberFunction, t_NameHash, t_CStreamContext, t_CStreamParams, t_CStreamResult, void> final
		: public CRuntimeTypeRegistryEntry_MemberFunction
	{
		TCRuntimeTypeRegistryEntry_MemberFunction();

		NConcurrency::TCFuture<NStream::CBinaryStorage> f_Call(NStream::TCBinaryStreamStoragePtr<NStream::CBinaryStreamDefault> &_Stream, void *_pObject) override;

	private:
		template <umint ...tfp_Indices, typename ...tfp_CParams>
		auto fp_Call(t_CStreamParams &_ParamsStream, void *_pObject, NMeta::TCIndices<tfp_Indices...> const &_Indices, NMeta::TCTypeList<tfp_CParams...> const &_ParamTypes)
			-> NConcurrency::TCUnsafeFuture<NStream::CBinaryStorage>
		;
	};

	template <auto t_fMemberFunction, uint32 t_NameHash, typename t_CStreamContext, typename t_CStreamParams, typename t_CStreamResult, typename t_CResult>
	struct TCRuntimeTypeRegistryEntry_MemberFunction<t_fMemberFunction, t_NameHash, t_CStreamContext, t_CStreamParams, t_CStreamResult, NConcurrency::TCFuture<t_CResult>> final
		: public CRuntimeTypeRegistryEntry_MemberFunction
	{
		TCRuntimeTypeRegistryEntry_MemberFunction();

		NConcurrency::TCFuture<NStream::CBinaryStorage> f_Call(NStream::TCBinaryStreamStoragePtr<NStream::CBinaryStreamDefault> &_Stream, void *_pObject) override;

	private:
		template <umint ...tfp_Indices, typename ...tfp_CParams>
		auto fp_Call(t_CStreamParams &_ParamsStream, void *_pObject, NMeta::TCIndices<tfp_Indices...> const &_Indices, NMeta::TCTypeList<tfp_CParams...> const &_ParamTypes)
			-> NConcurrency::TCFuture<NStream::CBinaryStorage>
		;
	};

	template
	<
		auto t_fMemberFunction
		DMibIfNotSupportMemberNameFromMemberPointer(, uint32 t_NameHash)
		, typename t_CStreamContext
		, typename t_CStreamParams
		, typename t_CStreamResult
	>
	struct TCRuntimeTypeRegistryEntry_MemberFunctionInit
	{
		template <typename ...tfp_CAlternate>
		TCRuntimeTypeRegistryEntry_MemberFunctionInit(NMeta::TCTypeList<tfp_CAlternate...> _AlternateTypes);
	};

	template
	<
		auto t_fMemberFunction
		DMibIfNotSupportMemberNameFromMemberPointer(, uint32 t_NameHash)
		, typename t_CStreamContext = zuint32
		, typename t_CStreamParams = NStream::TCBinaryStreamStoragePtr<NStream::CBinaryStreamDefault>
		, typename t_CStreamResult = NStream::TCBinaryStreamStorage<NStream::CBinaryStreamDefault>
	>
	struct TCMemberFunctionRegistry
	{
		static TCRuntimeTypeRegistryEntry_MemberFunctionInit
			<
				t_fMemberFunction
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
