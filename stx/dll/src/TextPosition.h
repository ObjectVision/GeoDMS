// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


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
typedef SharedStr                                              error_descr_t;
typedef boost::spirit::parser_error<error_descr_t, iterator_t> parser_error_t;

#endif //!defined( __STX_TEXTPOSITION_H)
