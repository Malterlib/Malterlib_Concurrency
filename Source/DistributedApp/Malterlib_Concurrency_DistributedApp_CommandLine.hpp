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
		_Stream % m_AnsiFlags;
	}
}
