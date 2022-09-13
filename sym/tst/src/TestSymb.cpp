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
#include "RtcBase.h"
#include "parser.h"
#include "LispList.h"
#include "ser/FormattedStream.h"
#include "ser/MoreStreamBuff.h"
#include "dbg/TraceToConsole.h"
#include "ser/FormattedStream.h"
#include "ser/AsString.h"

template <class T, class U>
FormattedOutStream& operator << (FormattedOutStream& ostr, const std::pair<T, U>& p)
{
	return ostr << "[" << p.first << ", " << p.second << "]";
}

void TestSymbol()
{
  LispRef x(LispRef("Hello"), LispRef("World"));
  std::cout << x << char(0);
  std::cout << List2(LispRef("Hello"), LispRef("World")) << char(0);
  std::cout << LispRef(List2(LispRef("Hello"), LispRef("World"))) << char(0);
}

#include "lispeval.h"

void TestLisp()
{
	FileInpStreamBuff insb("testlisp.inp", true);
	FormattedInpStream fin(&insb);
//	fin.imbue(std::locale("C"));

	LispRef globalEnv = GetExpr(fin);

	LispRef x = GetExpr(fin);
	std::cout << "Given Expression:\n" << x << std::endl;
	std::cout << RepeatedEval(x, globalEnv) << std::endl;
};

#include "prolog.h"

void TestProlog()
{
	FileInpStreamBuff insb("testpro.inp", true);
	FormattedInpStream fin(&insb);

 	RuleList ruleBase = GetExpr(fin);

	PredicateList x = GetExpr(fin);

	std::cout << "Given Goal:\n" << AsString(x) << std::endl;
	std::cout << "And   Env:\n"  << AsString(ruleBase) << std::endl;
	AnswerList answers =  Solve(ruleBase, x);
	std::cout << "Answers:\n" << AsString(answers) << "\n";

	RuleList rewrittenGoals = RewriteGoals(x, answers);

	std::cout << "In Goal:\n" << AsString(rewrittenGoals) << "\n";
};

int main()
{
	LispRef::MAX_PRINT_LEVEL = -1;
/*
	std::cout << "Symbolic Hello World\n";
	TestSymbol();
	std::cout << std::endl;

	std::cout << "Symbolic Lisp\n";
	TestLisp();
	std::cout << std::endl;
*/
	std::cout << "Symbolic Prolog\n";
	TestProlog();
	std::cout << std::endl;

	return 0;
}

