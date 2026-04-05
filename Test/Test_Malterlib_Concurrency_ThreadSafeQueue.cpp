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

			NAtomic::TCAtomic<umint> m_First;
			NAtomic::TCAtomic<umint> m_Last;

			void f_Push(CNode *_pData)
			{

			}

			CNode *f_Pop()
			{

			}
		};
	}
}
namespace
{

	class CLocklessQueueu_Tests : public NMib::NTest::CTest
	{
	public:
		void f_DoTests()
		{

		}
	};

	DMibTestRegister(CLocklessQueueu_Tests, Malterlib::Concurrency);
}


