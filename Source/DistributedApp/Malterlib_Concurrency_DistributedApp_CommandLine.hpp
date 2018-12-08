// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CStream>
	void CCommandLineControl::f_Stream(tf_CStream &_Stream)
	{
		_Stream % fg_Move(m_ControlActor);
		_Stream % m_CommandLineWidth;
		_Stream % m_CommandLineHeight;
		_Stream % m_bColorEnabled;
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
		(void)Dummy;
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
		(void)Dummy;
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
