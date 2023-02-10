%skeleton "lalr1.cc" /* -*- C++ -*- */
%require "3.2"
%defines

%define api.token.constructor
%define api.location.file none
%define api.value.type variant
%define parse.assert

%code requires {
	#include <string>
	#include <exception>
	class driver;
	class RootAST;
	class ExprAST;
	class FunctionAST;
	class SeqAST;
	class PrototypeAST;
	class IfExprAST;
	class ForExprAST;
	class StepAST;
}

// The parsing context.
%param { driver& drv }

%locations

%define parse.trace
%define parse.error verbose

%code {
    #include "driver.hh"
}

%define api.token.prefix {TOK_}

/* Definizione token terminali */
%token
	END  0  "end of file"
	SEMICOLON  	";"
	COMMA      	","
	MINUS      	"-"
	PLUS       	"+"
	STAR       	"*"
	SLASH      	"/"
	LPAREN     	"("
	RPAREN     	")"
	EXTERN     	"extern"
	DEF        	"def"
	IF			"if"
	THEN		"then"
	ELSE		"else"
	ENDEXPR 	"end"
	FOR 		"for"
	IN 			"in"
	LE			"<="
	GE			">="
	COMPARE 	"=="
	LT			"<"
	GT			">"
	EQUAL 		"="
	COLON		":"
;

%token <std::string> IDENTIFIER "id"
%token <double> NUMBER "number"
%type <ExprAST*> exp
%type <ExprAST*> idexp
%type <std::vector<ExprAST*>> optexp
%type <std::vector<ExprAST*>> explist
%type <RootAST*> program
%type <RootAST*> top
%type <FunctionAST*> definition
%type <PrototypeAST*> external
%type <PrototypeAST*> proto
%type <std::vector<std::string>> idseq
%type <IfExprAST*> ifexpr
%type <ForExprAST*> forexpr
%type <StepAST*> step



%%
%start startsymb;

startsymb:
program             	{ drv.root = $1; } /* Per passare al driver la radice dell'albero */

program:
  %empty               	{ $$ = new SeqAST(nullptr,nullptr); }
|  top ";" program     	{ $$ = new SeqAST($1,$3); };

top:
  %empty                { $$ = nullptr; }
| definition           	{ $$ = $1; }
| external             	{ $$ = $1; }
| exp                  	{ $$ = $1; $1->toggle(); };

definition:
  "def" proto exp      	{ $$ = new FunctionAST($2, $3); $2->noemit(); };

external:
  "extern" proto       	{ $$ = $2; };

proto:
  "id" "(" idseq ")"   	{ $$ = new PrototypeAST($1, $3); };

idseq:
  %empty               	{ std::vector<std::string> args; $$ = args; }
| "id" idseq           	{ $2.insert($2.begin(), $1); $$ = $2; };


/* Specifica dell'associatività e delle precedenze */
%left ":";
%left "<" ">";
%left "<=" ">=" "==";
%left "-" "+";
%left "*" "/";

exp:
  exp "+" exp          	{ $$ = new BinaryExprAST("+", $1, $3); }
| exp "-" exp          	{ $$ = new BinaryExprAST("-", $1, $3); }
| exp "*" exp          	{ $$ = new BinaryExprAST("*", $1, $3); }
| exp "/" exp          	{ $$ = new BinaryExprAST("/", $1, $3); }
| exp "<" exp			{ $$ = new BinaryExprAST("<", $1, $3); }
| exp ">" exp			{ $$ = new BinaryExprAST(">", $1, $3); }
| exp "<=" exp			{ $$ = new BinaryExprAST("<=", $1, $3); }
| exp ">=" exp			{ $$ = new BinaryExprAST(">=", $1, $3); }
| exp "==" exp			{ $$ = new BinaryExprAST("==", $1, $3); }
| exp ":" exp			{ $$ = new BinaryExprAST(":", $1, $3); }
| "-" exp				{ $$ = new UnaryExprAST("-", $2); }
| "+" exp				{ $$ = new UnaryExprAST("+", $2); }
| idexp                	{ $$ = $1; }
| "(" exp ")"          	{ $$ = $2; }
| "number"           	{ $$ = new NumberExprAST($1); }
| ifexpr 				{ $$ = $1; }
| forexpr				{ std::cout<<"FOREXPR\n"; };

ifexpr: 
  "if" exp "then" exp "else" exp "end" 	{ $$ = new IfExprAST($2, $4, $6);}; 

forexpr:
  "for" idexp "=" exp "," exp step "in" exp "end" 	{ std::cout<<"FOR IDEXP = EXP, EXP STEP IN EXP END"; };

step:
  %empty 				{ std::cout<<"empty step\n"; }
| "," exp				{ $$ = new StepAST($2); };

idexp:
  "id"                 { $$ = new VariableExprAST($1); }
| "id" "(" optexp ")"  { $$ = new CallExprAST($1, $3); };

optexp:
%empty                 { std::vector<ExprAST*> args;
                         args.push_back(nullptr);
			 			 $$ = args;
                       }
| explist              { $$ = $1; };

explist:
  exp                  { std::vector<ExprAST*> args;
                         args.push_back($1);
			 			 $$ = args;
                       }
| exp "," explist      { $3.insert($3.begin(), $1); $$ = $3; };

%%

void yy::parser::error (const location_type& l, const std::string& m)
{
	std::cerr<<l<<": "<<m<<'\n';
}
