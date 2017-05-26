// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		template <typename tf_CStream>
		void CDistributedAppCommandLineResults::COutput::f_Feed(tf_CStream &_Stream) const
		{
			_Stream << m_OutputType;
			_Stream << m_Output;
		}
		
		template <typename tf_CStream>
		void CDistributedAppCommandLineResults::COutput::f_Consume(tf_CStream &_Stream)
		{
			_Stream >> m_OutputType;
			_Stream >> m_Output;
		}
		
		template <typename tf_CStream>
		void CDistributedAppCommandLineResults::f_Feed(tf_CStream &_Stream) const
		{
			_Stream << EVersion;
			_Stream << m_Output;
			_Stream << m_Status;
		}
		
		template <typename tf_CStream>
		void CDistributedAppCommandLineResults::f_Consume(tf_CStream &_Stream)
		{
			uint32 Version;
			_Stream >> Version;
			if (Version > EVersion)
				DMibError("Invalid command line results version");
			_Stream >> m_Output;
			_Stream >> m_Status;
		}
		

		COneOf::COneOf(NEncoding::CEJSON const &_Config)
		{
			m_Config.f_Array().f_Insert(_Config);
		}

		template <typename ...tfp_CParams>
		COneOf::COneOf(tfp_CParams const &...p_Config)
		{
			auto &Array = m_Config.f_Array();

			TCInitializerList<bool> Dummy = 
				{
					[&]
					{
						Array.f_Insert(NEncoding::CEJSON(p_Config));
						return false;
					}
					()...
				}
			;
		}
		
		COneOf::operator NEncoding::CEJSON () &&
		{
			return NEncoding::fg_UserType("$OneOf", m_Config.f_ToJSON());
		}

		COneOf::operator NEncoding::CEJSON () const &
		{
			return NEncoding::fg_UserType("$OneOf", m_Config.f_ToJSON());
		}
	
		COneOfType::COneOfType(NEncoding::CEJSON const &_Config)
		{
			m_Config.f_Array().f_Insert(_Config);
		}

		template <typename ...tfp_CParams>
		COneOfType::COneOfType(tfp_CParams const &...p_Config)
		{
			auto &Array = m_Config.f_Array();

			TCInitializerList<bool> Dummy = 
				{
					[&]
					{
						Array.f_Insert(NEncoding::CEJSON(p_Config));
						return false;
					}
					()...
				}
			;
		}
		
		COneOfType::operator NEncoding::CEJSON () &&
		{
			return NEncoding::fg_UserType("$OneOfType", m_Config.f_ToJSON());
		}

		COneOfType::operator NEncoding::CEJSON () const &
		{
			return NEncoding::fg_UserType("$OneOfType", m_Config.f_ToJSON());
		}
	}
}
