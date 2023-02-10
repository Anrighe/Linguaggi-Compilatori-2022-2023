%{ /* -*- C++ -*- */
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <string>
#include <cmath>
#include "driver.hh"
#include "parser.hh"

// Work around an incompatibility in flex (at least versions
// 2.5.31 through 2.5.33): it generates code that does
// not conform to C89.  See Debian bug 333231
// <http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=333231>.
#undef yywrap
#define yywrap() 1

// Pacify warnings in yy_init_buffer (observed with Flex 2.6.4)
// and GCC 7.3.0.
#if defined __GNUC__ && 7 <= __GNUC__
# pragma GCC diagnostic ignored "-Wnull-dereference"
#endif

yy::parser::symbol_type check_keywords(std::string lexeme, yy::location& loc);
%} 

%option noyywrap nounput batch debug noinput

id		[a-zA-Z][a-zA-Z_0-9]*
fpnum	[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?
fixnum	(0|[1-9][0-9]*)\.?[0-9]*
num		{fpnum}|{fixnum}
blank	[ \t]

%{
    // Code run each time a pattern is matched.
    # define YY_USER_ACTION loc.columns(yyleng);
%}
%%
%{
    // A handy shortcut to the location held by the driver.
    yy::location& loc = drv.location;
    // Code run each time yylex is called.
	loc.step(); // step() fa avanzare la posizione begin fino al valore di end
%}
{blank}+	loc.step(); /*se trovo uno spazio o tabulazione avanzo semplicemente con lo step*/
[\r\n]+	    loc.lines (yyleng); loc.step (); /*"\r\n se si è su Windows, altrimenti \n"*/


"-"     return yy::parser::make_MINUS     	(loc);
"+"     return yy::parser::make_PLUS      	(loc);
"*"     return yy::parser::make_STAR      	(loc);
"/"     return yy::parser::make_SLASH     	(loc);
"("     return yy::parser::make_LPAREN    	(loc);
")"     return yy::parser::make_RPAREN    	(loc);
";"     return yy::parser::make_SEMICOLON 	(loc);
","     return yy::parser::make_COMMA     	(loc);
"<="	return yy::parser::make_LE		   	(loc);
">="	return yy::parser::make_GE		   	(loc);
"=="	return yy::parser::make_COMPARE		(loc);
"<"		return yy::parser::make_LT		   	(loc);
">"		return yy::parser::make_GT		   	(loc);
"="		return yy::parser::make_EQUAL		(loc);
":"		return yy::parser::make_COLON		(loc);

{num} {
	errno = 0;
	double n = strtod(yytext, NULL); //conversione stringa yytext a double
	if (! (n!=HUGE_VAL && n!=-HUGE_VAL && errno != ERANGE)) //se il double è fuori dal range di rappresentazione solleva un syntax error
    	throw yy::parser::syntax_error(loc, "Float value is out of range: " + std::string(yytext));
  	return yy::parser::make_NUMBER(n, loc);
}

{id} return check_keywords(yytext, loc);

. {throw yy::parser::syntax_error(loc, "invalid character: " + std::string(yytext));}

<<EOF>> return yy::parser::make_END(loc);
%%

yy::parser::symbol_type check_keywords(std::string lexeme, yy::location& loc)  
{
	if (lexeme == "def") 
   	{
     	return yy::parser::make_DEF(loc);
   	} 
	else if (lexeme == "extern") 
	{
    	return yy::parser::make_EXTERN(loc);
   	} 
	else if (lexeme == "if")
    {
		return yy::parser::make_IF(loc);
    }
	else if (lexeme == "then")
    {
		return yy::parser::make_THEN(loc);
    }
	else if (lexeme == "else")
    {
		return yy::parser::make_ELSE(loc);
    }
	else if (lexeme == "end")
    {
		return yy::parser::make_ENDEXPR(loc);
    }
	else if (lexeme == "for")
    {
		return yy::parser::make_FOR(loc);
    }
	else if (lexeme == "in")
    {
		return yy::parser::make_IN(loc);
    }
    else //se non è nessun token sopra indicato allora è necessariamente un identificatore
   	{
     	return yy::parser::make_IDENTIFIER(yytext, loc);
   	}
}

void driver::scan_begin()
{
  	yy_flex_debug = trace_scanning;
  	if (file.empty () || file == "-")
    	yyin = stdin;
  	else if (!(yyin = fopen(file.c_str(), "r")))
    {
      	std::cerr<<"Cannot open "<<file<<": "<<strerror(errno)<<'\n';
      	exit(EXIT_FAILURE);
    }
}

void driver::scan_end()
{
  	fclose(yyin);
}