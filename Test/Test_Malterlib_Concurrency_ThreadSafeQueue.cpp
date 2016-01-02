// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

namespace NMib
{
	namespace NContainer
	{
		template <typename t_CType>
		struct TCLocklessQueue
		{
			struct CNode
			{
				mint m_Next;
				mint m_Prev;
			};
			
			NAtomic::TCAtomic<mint> m_First;
			NAtomic::TCAtomic<mint> m_Last;

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


