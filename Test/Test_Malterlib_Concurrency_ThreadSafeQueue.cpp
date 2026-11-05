// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

namespace NMib
{
	namespace NContainer
	{
		template <typename t_CType>
		struct TCLocklessQueue
		{
			struct CNode
			{
				umint m_Next;
				umint m_Prev;
			};

			void f_Push(CNode *_pData);

			CNode *f_Pop();

			NAtomic::TCAtomic<umint> m_First;
			NAtomic::TCAtomic<umint> m_Last;
		};

		template <typename t_CType>
		void TCLocklessQueue<t_CType>::f_Push(CNode *_pData)
		{
		}

		template <typename t_CType>
		typename TCLocklessQueue<t_CType>::CNode *TCLocklessQueue<t_CType>::f_Pop()
		{
		}
	}
}
namespace
{
	struct CLocklessQueueu_Tests : public NMib::NTest::CTest
	{
		void f_DoTests();
	};

	void CLocklessQueueu_Tests::f_DoTests()
	{
	}

	DMibTestRegister(CLocklessQueueu_Tests, Malterlib::Concurrency);
}

