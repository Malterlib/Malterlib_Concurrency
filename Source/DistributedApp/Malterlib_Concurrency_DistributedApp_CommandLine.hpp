// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CStream>
	void ICCommandLineControl::CScreenChange::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Width;
		_Stream % m_Height;
		_Stream % m_GlyphWidth;
		_Stream % m_GlyphHeight;
	}

	template <typename tf_CStream>
	void CCommandLineControl::f_Stream(tf_CStream &_Stream)
	{
		_Stream % fg_Move(m_ControlActor);
		_Stream % m_CommandLineWidth;
		_Stream % m_CommandLineHeight;
		if (_Stream.f_GetVersion() >= ICCommandLine::EProtocolVersion_SupportGlyphSize)
		{
			_Stream % m_CommandLineGlyphWidth;
			_Stream % m_CommandLineGlyphHeight;
		}
		_Stream % m_AnsiFlags;
	}
}
