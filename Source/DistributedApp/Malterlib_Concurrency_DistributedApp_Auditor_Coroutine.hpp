// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "../Actor/Malterlib_Concurrency_Promise_Coroutine.hpp"

namespace NMib::NConcurrency
{
	template <typename tf_CReturnValue>
	auto operator % (TCFuture<tf_CReturnValue> &&_Future, CDistributedAppAuditor const &_Auditor)
	{
		return TCFutureWithExceptionTransformer<tf_CReturnValue, TCFuture<tf_CReturnValue>>(fg_Move(_Future), fg_ExceptionTransformer(nullptr, _Auditor));
	}

	template <typename tf_CReturnValue, typename tf_CFutureLike>
	auto operator % (TCFutureWithExceptionTransformer<tf_CReturnValue, tf_CFutureLike> &&_Future, CDistributedAppAuditor const &_Auditor)
	{
		return TCFutureWithExceptionTransformer<tf_CReturnValue, tf_CFutureLike>(fg_Move(_Future.m_Future), fg_ExceptionTransformer(fg_Move(_Future.m_fTransformer), _Auditor));
	}

	template <typename tf_CReturnValue, typename tf_CActor, typename tf_FFunctionPointer, CBindActorOptions tf_BindOptions, bool tf_bByValue, typename ...tfp_CParams>
	auto operator %
		(
			TCBoundActorCall<TCFuture<tf_CReturnValue>, tf_CActor, tf_FFunctionPointer, tf_BindOptions, tf_bByValue, tfp_CParams...> &&_BoundActorCall
			, CDistributedAppAuditor const &_Auditor
		)
	{
		return TCFutureWithExceptionTransformer<tf_CReturnValue, TCBoundActorCall<TCFuture<tf_CReturnValue>, tf_CActor, tf_FFunctionPointer, tf_BindOptions, tf_bByValue, tfp_CParams...>>
			(
				fg_Move(_BoundActorCall)
				, fg_ExceptionTransformer(nullptr, _Auditor)
			)
		;
	}

	template <typename tf_CReturnValue>
	auto operator % (TCFuture<tf_CReturnValue> &&_Future, CDistributedAppAuditorWithError const &_Auditor)
	{
		return TCFutureWithExceptionTransformer<tf_CReturnValue, TCFuture<tf_CReturnValue>>(fg_Move(_Future), fg_ExceptionTransformer(nullptr, _Auditor));
	}

	template <typename tf_CReturnValue, typename tf_CFutureLike>
	auto operator % (TCFutureWithExceptionTransformer<tf_CReturnValue, tf_CFutureLike> &&_Future, CDistributedAppAuditorWithError const &_Auditor)
	{
		return TCFutureWithExceptionTransformer<tf_CReturnValue, tf_CFutureLike>(fg_Move(_Future.m_Future), fg_ExceptionTransformer(fg_Move(_Future.m_fTransformer), _Auditor));
	}

	template <typename tf_CReturnValue, typename tf_CActor, typename tf_FFunctionPointer, CBindActorOptions tf_BindOptions, bool tf_bByValue, typename ...tfp_CParams>
	auto operator %
		(
			TCBoundActorCall<TCFuture<tf_CReturnValue>, tf_CActor, tf_FFunctionPointer, tf_BindOptions, tf_bByValue, tfp_CParams...> &&_BoundActorCall
			, CDistributedAppAuditorWithError const &_Auditor
		)
	{
		return TCFutureWithExceptionTransformer<tf_CReturnValue, TCBoundActorCall<TCFuture<tf_CReturnValue>, tf_CActor, tf_FFunctionPointer, tf_BindOptions, tf_bByValue, tfp_CParams...>>
			(
				fg_Move(_BoundActorCall)
				, fg_ExceptionTransformer(nullptr, _Auditor)
			)
		;
	}
}
