#include "driver.hh"
#include "parser.hh"

Value *LogErrorV(const std::string Str) 
{
	std::cerr << Str << std::endl;
	return nullptr;
}

/*************************** Driver class *************************/
driver::driver(): trace_parsing (false), trace_scanning (false), ast_print (false) 
{
	context = new LLVMContext;
	module = new Module("Kaleidoscope", *context);
	builder = new IRBuilder(*context);
};

int driver::parse(const std::string &f) 
{
	file = f;
	location.initialize(&file);
	scan_begin();
	yy::parser parser(*this);
	parser.set_debug_level(trace_parsing);
	int res = parser.parse();
	scan_end();
	return res;
}

void driver::codegen() 
{
	if (ast_print) root->visit();
	std::cout << std::endl;
	root->codegen(*this);
};

/********************** Handle Top Expressions ********************/
Value* TopExpression(ExprAST* E, driver& drv) 
{
	// Crea una funzione anonima il cui body è un'espressione top-level
	// viene "racchiusa" un'espressione top-level
	std::cout<<"CREO LA FUNZIONE ANONIMA\n"; // Debug
	E->toggle(); // Evita la doppia emissione del prototipo
	PrototypeAST *Proto = new PrototypeAST("__espr_anonima" + std::to_string(++drv.Cnt), std::vector<std::string>());
	Proto->noemit();
	FunctionAST *F = new FunctionAST(std::move(Proto), E);
	auto *FnIR = F->codegen(drv);
	FnIR->eraseFromParent();
	return nullptr;
};

/************************ Expression tree *************************/
// Inverte il flag che definisce le TopLevelExpression quando viene chiamata
void ExprAST::toggle() 
{
	top = top ? false : true;
};

bool ExprAST::gettop() 
{
	return top;
};

/************************* Sequence tree **************************/
SeqAST::SeqAST(RootAST* first, RootAST* continuation):
	first(first), continuation(continuation) {};

void SeqAST:: visit() 
{
	if (first != nullptr) 
	{
    	first->visit();
  	} 
	else 
	{
		if (continuation == nullptr) 
		{
			return;
    	};
  	};
  	std::cout << ";";
  	continuation->visit();
};

Value *SeqAST::codegen(driver& drv) 
{
	if (first != nullptr) 
  	{
		Value *f = first->codegen(drv);
  	} 
	else 
  	{
    	if (continuation == nullptr) return nullptr;
  	}
  	Value *c = continuation->codegen(drv);
  	return nullptr;
};

/********************* Number Expression Tree *********************/
NumberExprAST::NumberExprAST(double Val): Val(Val) { top = false; };
void NumberExprAST::visit() 
{
	std::cout << Val << " ";
};

Value *NumberExprAST::codegen(driver& drv) 
{  
	if (gettop()) 
		return TopExpression(this, drv);
	else 
		return ConstantFP::get(*drv.context, APFloat(Val));
};

/****************** Variable Expression TreeAST *******************/
VariableExprAST::VariableExprAST(std::string &Name):
	Name(Name) { top = false; };

const std::string& VariableExprAST::getName() const 
{
	return Name;
};

void VariableExprAST::visit() 
{
	std::cout<<getName()<<" ";
};

Value *VariableExprAST::codegen(driver& drv) 
{
	if (gettop()) 
	{
		return TopExpression(this, drv);
	} 
	else 
	{
		Value *V = drv.NamedValues[Name];
		if (!V) 
			LogErrorV("Variabile non definita");
		return V;
  	}
};

/******************** Binary Expression Tree **********************/
BinaryExprAST::BinaryExprAST(std::string Op, ExprAST* LHS, ExprAST* RHS): Op(Op), LHS(LHS), RHS(RHS) 
	{ 	std::cout<<"COSTRUTTORE BinaryExprAST\n";
		top = false; };
 
void BinaryExprAST::visit() 
{	
	std::cout<<"Visit BinaryExprAST\n";
	std::cout<<"("<<Op<<" ";
	LHS->visit();
	if (RHS!=nullptr) 
		RHS->visit();
	std::cout<<")";
};

Value *BinaryExprAST::codegen(driver& drv) 
{
	std::cout<<"CODEGEN DI BINARY EXPR AST\n";
	if (gettop()) 
	{
		std::cout<<"RITORNO TOP EXPRESSION\n";
    	return TopExpression(this, drv);
  	} 
	else 
	{
		Value *L = LHS->codegen(drv);
		Value *R = RHS->codegen(drv);
		if (!L || !R) return nullptr;
		switch (Op[0])
		{
			case '+':
				return drv.builder->CreateFAdd(L, R, "addregister");
			case '-':
				return drv.builder->CreateFSub(L, R, "subregister");
			case '*':
				return drv.builder->CreateFMul(L, R, "mulregister");
			case '/':
				return drv.builder->CreateFDiv(L, R, "addregister");
			case '=':
				//std::cout<<"PRIMO CARATTERE ="<<std::endl; //Debug
				if (Op[1] == '=')
				{	
					//std::cout<<"SECONDO CARATTERE ="<<std::endl; //Debug
					L = drv.builder->CreateFCmpOEQ(L, R, "compare");
					return drv.builder->CreateUIToFP(L, Type::getDoubleTy(*drv.context), "cmpres");
				}
				else
					return LogErrorV("Operatore binario = non supportato"); //non utilizzato inizialmente perché il solo op binario = non è ammesso
			case '<':
				if (Op[1] == '=')
				{
					L = drv.builder->CreateFCmpULE(L, R, "tmp");
					return drv.builder->CreateUIToFP(L, Type::getDoubleTy(*drv.context), "cmpres");
				}	
				L = drv.builder->CreateFCmpULT(L, R, "tmp");
				return drv.builder->CreateUIToFP(L, Type::getDoubleTy(*drv.context), "cmpres"); 
			case '>':
				if (Op[1] == '=')
				{
					L = drv.builder->CreateFCmpUGE(L, R, "tmp");
					return drv.builder->CreateUIToFP(L, Type::getDoubleTy(*drv.context), "cmpres");
				}	
				L = drv.builder->CreateFCmpUGT(L, R, "tmp");
				return drv.builder->CreateUIToFP(L, Type::getDoubleTy(*drv.context), "cmpres"); 
			default:  
				return LogErrorV("Operatore binario non supportato");
		}
	}
};

/********************* Call Expression Tree ***********************/
CallExprAST::CallExprAST(std::string Callee, std::vector<ExprAST*> Args): Callee(Callee), Args(std::move(Args)) 
	{ top = false; };

void CallExprAST::visit() 
{
  	std::cout<<Callee<<"( ";
 	for (ExprAST* arg : Args) 
	{
    	arg->visit();
  	};
  	std::cout<<')';
};

Value *CallExprAST::codegen(driver& drv) 
{
	if (gettop()) 
	{
		return TopExpression(this, drv);
	} 
	else 
	{
		// Cerchiamo la funzione nell'ambiente globale
		Function *CalleeF = drv.module->getFunction(Callee);
		if (!CalleeF)
			return LogErrorV("Funzione non definita");

		// Controlliamo che gli argomenti coincidano in numero coi parametri
		if (CalleeF->arg_size() != Args.size())
			return LogErrorV("Numero di argomenti non corretto");

		std::vector<Value *> ArgsV;
		for (auto arg : Args) 
		{
			ArgsV.push_back(arg->codegen(drv));
			if (!ArgsV.back())
				return nullptr;
		}
		return drv.builder->CreateCall(CalleeF, ArgsV, "calltmp");
	}
}

/************************* Prototype Tree *************************/
PrototypeAST::PrototypeAST(std::string Name, std::vector<std::string> Args): Name(Name), Args(std::move(Args)) 
	{ emit = true; };

const std::string& PrototypeAST::getName() const { return Name; };
const std::vector<std::string>& PrototypeAST::getArgs() const { return Args; };
void PrototypeAST::visit() 
{
	std::cout << "extern " << getName() << "( ";
	for (auto it=getArgs().begin(); it!= getArgs().end(); ++it) 
	{
		std::cout<<*it <<' ';
	};
	std::cout<<')';
};

void PrototypeAST::noemit() { emit = false; };

bool PrototypeAST::emitp() { return emit; };

Function *PrototypeAST::codegen(driver& drv) 
{
  	// Costruisce una struttura double(double,...,double) che descrive 
  	// tipo di ritorno e tipo dei parametri (in Kaleidoscope solo double)
  	std::vector<Type*> Doubles(Args.size(), Type::getDoubleTy(*drv.context));
  	FunctionType *FT = FunctionType::get(Type::getDoubleTy(*drv.context), Doubles, false);
  	Function *F = Function::Create(FT, Function::ExternalLinkage, Name, *drv.module);

  	// Attribuiamo agli argomenti il nome dei parametri formali specificati dal programmatore
  	unsigned Idx = 0;
  	for (auto &Arg : F->args())
    	Arg.setName(Args[Idx++]);

  	if (emitp()) // emitp() restituisce true se e solo se il prototipo è definito extern
	{  
    	F->print(errs());
    	fprintf(stderr, "\n");
  	};
  
  	return F;
}

/************************* Function Tree **************************/
FunctionAST::FunctionAST(PrototypeAST* Proto, ExprAST* Body): Proto(Proto), Body(Body) 
{
	if (Body == nullptr) external=true;
	else external=false;
};

void FunctionAST::visit() 
{
	std::cout << Proto->getName() << "( ";
	for (auto it=Proto->getArgs().begin(); it!= Proto->getArgs().end(); ++it) 
	{
		std::cout << *it << ' ';
  	};
	std::cout << ')';
	Body->visit();
};

Function *FunctionAST::codegen(driver& drv) 
{
	// Verifica che non esiste già, nel contesto, una funzione con lo stesso nome
	std::string name = Proto->getName();
	Function *TheFunction = drv.module->getFunction(name);
	// E se non esiste prova a definirla
	if (TheFunction) 
	{
		LogErrorV("Funzione "+name+" già definita");
		return nullptr;
	}
	if (!TheFunction)
		TheFunction = Proto->codegen(drv);
	if (!TheFunction)
    	return nullptr;  // Se la definizione "fallisce" restituisce nullptr

	// Crea un blocco di base in cui iniziare a inserire il codice
	BasicBlock *BB = BasicBlock::Create(*drv.context, "entry", TheFunction);
	drv.builder->SetInsertPoint(BB);

	// Registra gli argomenti nella symbol table
	drv.NamedValues.clear();
	for (auto &Arg : TheFunction->args())
		drv.NamedValues[std::string(Arg.getName())] = &Arg;

	if (Value *RetVal = Body->codegen(drv)) 
	{
		// Termina la creazione del codice corrispondente alla funzione
		drv.builder->CreateRet(RetVal);

		// Effettua la validazione del codice e un controllo di consistenza
		verifyFunction(*TheFunction);

		TheFunction->print(errs());
		fprintf(stderr, "\n");
		return TheFunction;
	}

	// Errore nella definizione. La funzione viene rimossa
	TheFunction->eraseFromParent();
	return nullptr;
};

/********************** If Expressions ********************/
IfExprAST::IfExprAST(ExprAST * cond, ExprAST * thenExpr, ExprAST * elseExpr)
{	
	std::cout<<"BBBBBBB\n";
	this->cond = cond;
	this->thenExpr = thenExpr;
	this->elseExpr = elseExpr;
};

void IfExprAST::visit()
{
	std::cout<<"(";
	cond->visit();
	std::cout<<"then";
	thenExpr->visit();
	std::cout<<"else";
	elseExpr->visit();
	std::cout<<")"<<std::endl;
}

Value * IfExprAST::codegen(driver &drv)
{
	std::cout<<"TOP IN IFEXPRAST: "<<this->top<<std::endl;

	// this->top vale 0 quando la IfExprAST è già dentro a una funzione
	// this->top vale 1 quando la IfExprAST non è dentro a una funzione
	if (gettop()) 
		return TopExpression(this, drv); //se la IfExpr non è dentro a una funzione la "racchiudo" in una funzione anonima

	Value * condition = cond->codegen(drv);
	
	if (!condition)
	{
		std::cout<<"condition valutato come false"<<std::endl;
    	return nullptr;
	}

	// Conversione della condizione ad un booleano comparandola in NON EQUAL con 0.0
  	condition = drv.builder->CreateFCmpONE(condition, ConstantFP::get(*drv.context, APFloat(0.0)), "iftest");
	std::cout<<"AAAAAAA\n";

	if (drv.builder == nullptr)
		std::cout<<"DRV NULL"<<std::endl;
	if (drv.builder->GetInsertBlock() == nullptr)
		std::cout<<"GetInsertBlock NULL"<<std::endl;

	Function *TheFunction = drv.builder->GetInsertBlock()->getParent(); //getParent() trova la funzione che contiene i 3 blocchi

	// Create blocks for the then and else cases.  Insert the 'then' block at the
	// end of the function.
	BasicBlock *ThenBB = BasicBlock::Create(*drv.context, "then", TheFunction);
	BasicBlock *ElseBB = BasicBlock::Create(*drv.context, "else");
	BasicBlock *MergeBB = BasicBlock::Create(*drv.context, "merge");

	// Creo l'istruzione di salto condizionato al valore di condition
	drv.builder->CreateCondBr(condition, ThenBB, ElseBB);

	// Genero le istruzioni nel blocco condizionale 'then'
	drv.builder->SetInsertPoint(ThenBB);
	Value * ThenValue = thenExpr->codegen(drv);
	if (!ThenValue)
	{
		std::cout<<"ThenValue valutato come false"<<std::endl;
  		return nullptr;
	}

	drv.builder->CreateBr(MergeBB);

	ThenBB = drv.builder->GetInsertBlock();
	TheFunction->getBasicBlockList().push_back(ElseBB);

	// Genero le istruzioni nel blocco condizionale 'else'
	drv.builder->SetInsertPoint(ElseBB);
	Value * ElseValue = elseExpr->codegen(drv);	
	if (!ElseValue)
	{
		std::cout<<"ElseValue valutato come false"<<std::endl;
  		return nullptr;
	}

	drv.builder->CreateBr(MergeBB);
	ElseBB = drv.builder->GetInsertBlock();

	TheFunction->getBasicBlockList().push_back(MergeBB);
	drv.builder->SetInsertPoint(MergeBB);

	/* 	Type::getDoubleTy(*drv.context): tipo
		2: da quanti blocchi arrivo (coming edge)
		ifres: nome del registro SSA 	*/	
  	PHINode *IFRES = drv.builder->CreatePHI(Type::getDoubleTy(*drv.context), 2, "ifres");

	IFRES->addIncoming(ThenValue, ThenBB); // Se arrivo dal blocco Then completa l'espressione con ThenValue
  	IFRES->addIncoming(ElseValue, ElseBB); // Se arrivo dal blocco Else completa l'espressione con ElseValue

	

	return IFRES; // Ritorna il valore contenuto nel registro SSA
}
