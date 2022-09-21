//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>
#pragma once

#if !defined( __STX_TEXTPOSITION_H)
#define __STX_TEXTPOSITION_H

#if !defined(__STX_PCH) && ! defined(__CLC_PCH_H)
#error "Include StxPCH.h from using code unit in order to support PCH"
#endif

#include "xct/ErrMsg.h"

#include <boost/spirit/include/classic_core.hpp>
#include <boost/spirit/include/classic_position_iterator.hpp>
#include <boost/spirit/include/classic_exceptions.hpp>


///////////////////////////////////////////////////////////////////////////////
//
//	text_position
//
//	used by our position_iterator to keep track of (line, column) position
//	without the overhead of dragging a filename (as in file_position) string
//	along all copies of iterators (and parsing makes a lot of temp iterator copies)
//
///////////////////////////////////////////////////////////////////////////////

struct text_position
{
	text_position(): line(1), column(1) {}
	int line, column;
};

///////////////////////////////////////////////////////////////////////////////
//
//  position_policy<text_position>
//
//  Specialization to handle text_position. Track characters and tabulation
//  to compute the current column correctly.
//
//  Default tab size is 4. You can change this with the set_tabchars member
//  of position_iterator.
//
//	This policy is added to provide the needs of boost_1_31_0::spirit
//
///////////////////////////////////////////////////////////////////////////////

template <>
class boost::spirit::position_policy<text_position> {

public:
    position_policy()
        : m_CharsPerTab(4)
    {}

    void next_line(text_position& pos)
    {
        ++pos.line;
        pos.column = 1;
    }

    void set_tab_chars(unsigned int chars)
    {
        m_CharsPerTab = chars;
    }

    void next_char(text_position& pos)
    {
        ++pos.column;
    }

    void tabulation(text_position& pos)
    {
        pos.column += m_CharsPerTab - (pos.column - 1) % m_CharsPerTab;
    }

private:
    unsigned int m_CharsPerTab;
};

typedef const char                                             char_t;
typedef text_position                                          position_t;
typedef boost::spirit::position_iterator<char_t*, position_t>  iterator_t;
typedef boost::spirit::parse_info<iterator_t>                  parse_info_t;
typedef ErrMsgPtr                                              error_descr_t;
typedef boost::spirit::parser_error<error_descr_t, iterator_t> parser_error_t;

#endif //!defined( __STX_TEXTPOSITION_H)
