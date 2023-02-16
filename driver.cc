#include "driver.hh"
#include "parser.hh"

Value *LogErrorV(const std::string Str) 
{
	std::cerr<<Str<<std::endl;
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
	std::cout<<Val<<" ";
};

Value *NumberExprAST::codegen(driver& drv) 
{  
	if (gettop()) 
		return TopExpression(this, drv);
	else 
		return ConstantFP::get(*drv.context, APFloat(Val));
};

/****************** Variable Expression TreeAST *******************/
VariableExprAST::VariableExprAST(std::string &Name): Name(Name) 
{ 
	std::cout<<"COSTRUTTORE VARIABLEEXPRAST: "<<this->getName()<<"\n"; // Debug
	top = false; 
};

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
	// Per le variabili non viene generato codice
	if (gettop()) 
	{
		return TopExpression(this, drv);
	} 
	else // viene effettuato l'accesso alla symbol table e se presente viene restituito il valore associato alla variabile
	{
		Value *V = drv.NamedValues[Name];
		if (!V) 
			LogErrorV("Variabile non definita");
		return V;
  	}
};

/******************** Binary Expression Tree **********************/
BinaryExprAST::BinaryExprAST(std::string Op, ExprAST* LHS, ExprAST* RHS): Op(Op), LHS(LHS), RHS(RHS) 
{ 	
	std::cout<<"COSTRUTTORE BinaryExprAST\n"; // Debug
	top = false; 
};

void BinaryExprAST::visit() 
{	
	std::cout<<"("<<Op<<" ";
	LHS->visit();
	if (RHS!=nullptr) 
		RHS->visit();
	std::cout<<")";
};

Value *BinaryExprAST::codegen(driver& drv) 
{
	std::cout<<"CODEGEN DI BINARY EXPR AST\n"; // Debug
	if (gettop()) 
	{
		std::cout<<"RITORNO TOP EXPRESSION\n"; // Debug
    	return TopExpression(this, drv);
  	} 
	else 
	{
		// Inizialmente vengono invocati codegen() sui nodi figli:
		// L e R memorizzano il riferimento ai registri SSA che contengono i risultati delle due sottoespressioni
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
				return drv.builder->CreateFDiv(L, R, "divregister");
			case '=':
				if (Op[1] == '=') // ==
				{	
					L = drv.builder->CreateFCmpUEQ(L, R, "compareEQ"); // CreateFloatCompareOrderedEqual ritorna un Value che contiene True o False (Unordered: non considera i NaN)
					return drv.builder->CreateUIToFP(L, Type::getDoubleTy(*drv.context), "cmpres"); // Conversione True/False a Double
				}
				else
					return LogErrorV("Operatore binario = non supportato"); //non utilizzato inizialmente perché il solo op binario = non è ammesso TODO: Cancellare in caso di for implementato
			case '<':
				if (Op[1] == '=') // <=
				{
					L = drv.builder->CreateFCmpULE(L, R, "compareLE"); // CreateFloatCompareUnorderedLesser(OR)Equal ritorna un Value che contiene True o False (Unordered: non considera i NaN)
					return drv.builder->CreateUIToFP(L, Type::getDoubleTy(*drv.context), "cmpLEres"); // Conversione True/False a Double
				}	
				L = drv.builder->CreateFCmpULT(L, R, "compareLT"); // CreateFloatCompareUnorderedLesserThan ritorna un Value che contiene True o False (Unordered: non considera i NaN)
				return drv.builder->CreateUIToFP(L, Type::getDoubleTy(*drv.context), "cmpLTres"); // Conversione True/False a Double
			case '>':
				if (Op[1] == '=') // >=
				{
					L = drv.builder->CreateFCmpUGE(L, R, "compareGE"); // CreateFloatCompareUnorderedGreater(OR)Equal ritorna un Value che contiene True o False (Unordered: non considera i NaN)
					return drv.builder->CreateUIToFP(L, Type::getDoubleTy(*drv.context), "cmpGEres"); // Conversione True/False a Double
				}	
				L = drv.builder->CreateFCmpUGT(L, R, "compareGT"); // CreateFloatCompareUnorderedGreaterThan ritorna un Value che contiene True o False (Unordered: non considera i NaN)
				return drv.builder->CreateUIToFP(L, Type::getDoubleTy(*drv.context), "cmpGTres"); // Conversione True/False a Double
			case ':':
				// Nel caso di un : (compound expression), è sufficiente ritornare il Value * R, ovvero la valutazione dell'espressione destra,
				// in quanto "Il valore di una espressione composta si definisce semplicemente come il valore dell’ultima espressione nella sequenza"
				return R;
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
		// Interroga il modulo corrente per recuperare l'oggetto di tipo Function che rappresenta la funzione
		Function *CalleeF = drv.module->getFunction(Callee);
		if (!CalleeF)
			return LogErrorV("Funzione non definita");

		// Controlliamo che gli argomenti coincidano in numero coi parametri specificati nella definizione di funzione
		if (CalleeF->arg_size() != Args.size())
			return LogErrorV("Numero di argomenti non corretto");
		
		// Genera il codice per il calcolo degli argomenti
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
{
	emit = true; 
};

const std::string& PrototypeAST::getName() const { return Name; };
const std::vector<std::string>& PrototypeAST::getArgs() const { return Args; };
void PrototypeAST::visit() 
{
	std::cout<<"extern "<<getName()<<"( ";
	for (auto it=getArgs().begin(); it!= getArgs().end(); ++it) 
	{
		std::cout<<*it<<' ';
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
	for (auto it = Proto->getArgs().begin(); it!= Proto->getArgs().end(); ++it) 
	{
		std::cout<<*it<<' ';
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
		LogErrorV("Funzione " + name + " già definita");
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
	drv.NamedValues.clear(); // Cancella la symbol table per via dello scope
	for (auto &Arg : TheFunction->args())
		drv.NamedValues[std::string(Arg.getName())] = &Arg; // Inserisce gli argomenti nella symbol table

	if (Value *RetVal = Body->codegen(drv)) 
	{
		// Termina la creazione del codice corrispondente alla funzione
		drv.builder->CreateRet(RetVal); // Inserimento di un'istruzione ret, che ripristina il controllo di flusso e opzionalmente un valore al chiamante

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
	std::cout<<"COSTRUTTORE IFEXPRAST\n"; //DEBUG
	this->cond = cond; // Condizione dopo l'if
	this->thenExpr = thenExpr; // Espressione dopo il then
	this->elseExpr = elseExpr; // Espressione dopo l'else
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
	std::cout<<"TOP IN IFEXPRAST: "<<this->top<<std::endl; //Debug

	// top vale 0 quando la IfExprAST è già dentro a una funzione
	// top vale 1 quando la IfExprAST NON è dentro a una funzione
	if (gettop()) 
		return TopExpression(this, drv); //se la IfExpr non è dentro a una funzione la "racchiudo" in una funzione anonima

	Value * condition = cond->codegen(drv); // Valutazione della condizione dopo l'if
	
	if (!condition)
    	return nullptr;

	// Conversione della condizione ad un booleano comparandola in NON EQUAL con 0.0
  	condition = drv.builder->CreateFCmpUNE(condition, ConstantFP::get(*drv.context, APFloat(0.0)), "iftest"); // CreateFloatCompareUnorderedNotEqual
	
	if (drv.builder == nullptr) // Debug
		std::cout<<"DRV NULL"<<std::endl; // Debug
	if (drv.builder->GetInsertBlock() == nullptr) // Debug
		std::cout<<"GetInsertBlock NULL"<<std::endl; // Debug

	Function *function = drv.builder->GetInsertBlock()->getParent(); //getParent() trova la funzione che contiene i 3 blocchi

	// Creazione dei tre blocchi base Then, Else e Merge
	BasicBlock *ThenBB = BasicBlock::Create(*drv.context, "then", function);
	BasicBlock *ElseBB = BasicBlock::Create(*drv.context, "else");
	BasicBlock *MergeBB = BasicBlock::Create(*drv.context, "merge");

	// Creo l'istruzione di salto condizionato al valore di condition
	drv.builder->CreateCondBr(condition, ThenBB, ElseBB);

	// Genero le istruzioni nel blocco condizionale 'then'
	drv.builder->SetInsertPoint(ThenBB);
	Value * ThenValue = thenExpr->codegen(drv); // Valutazione espressione del then
	if (!ThenValue)
  		return nullptr;


	drv.builder->CreateBr(MergeBB); // CreateBr: Crea un'istruzione di branch 'br label X'

	ThenBB = drv.builder->GetInsertBlock(); // GetInsertBlock: ritorna il basic block dove la nuova istruzione verrà inserita
	function->getBasicBlockList().push_back(ElseBB);

	// Genero le istruzioni nel blocco condizionale 'else'
	drv.builder->SetInsertPoint(ElseBB);
	Value * ElseValue = elseExpr->codegen(drv); // Valutazione espressione dell'else
	if (!ElseValue)
  		return nullptr;

	drv.builder->CreateBr(MergeBB); // CreateBr: Crea un'istruzione di branch 'br label X'
	ElseBB = drv.builder->GetInsertBlock(); // GetInsertBlock: ritorna il basic block dove la nuova istruzione verrà inserita

	function->getBasicBlockList().push_back(MergeBB);
	drv.builder->SetInsertPoint(MergeBB); // SetInsertPoint: cambia l'insert point al blocco MergeBB

	/* 	Type::getDoubleTy(*drv.context): tipo
		2: da quanti blocchi arrivo (coming edge)
		ifres: nome del registro SSA 	*/	
  	PHINode *IFRES = drv.builder->CreatePHI(Type::getDoubleTy(*drv.context), 2, "ifres");

	IFRES->addIncoming(ThenValue, ThenBB); // Se arrivo dal blocco Then completa l'espressione con ThenValue
  	IFRES->addIncoming(ElseValue, ElseBB); // Se arrivo dal blocco Else completa l'espressione con ElseValue

	return IFRES; // Ritorna il valore contenuto nel registro SSA
}

/********************** Operatore Unario ********************/
UnaryExprAST::UnaryExprAST(std::string sign, ExprAST* RHS)
{	
	this->sign = sign; // Segno precedente all'espressione ('-' oppure '+')
	this->RHS = RHS; // Espressione successiva all'operatore unario
};

void UnaryExprAST::visit()
{
	std::cout<<"("<<sign<<" ";
	if (RHS!=nullptr) 
		RHS->visit();
	std::cout<<")";
}

Value * UnaryExprAST::codegen(driver &drv)
{
	// Se l'operatore unario non è contenuto nel corpo di una funzione, viene inserito in una funzione anonima
	if (gettop()) 
		return TopExpression(this, drv);
	
	Value *R = RHS->codegen(drv);

	if (sign == "+") // Se l'operatore unario è un + ritorno semplicemente il risultato della parte destra
	{
		return R;
	}
	else
	{
		if (sign == "-") // Se l'operatore unario è un - converto la parte destra a double cambiato di segno
			return drv.builder->CreateFNeg(R, "unaryRes");
	}
	return LogErrorV("Operatore unario non supportato");
}


/********************** For Expression ********************/
/*
ForExprAST::ForExprAST(std::string &varName, ExprAST * start, ExprAST * end, ExprAST * step, ExprAST * body)
{
	std::cout<<"COSTRUTTORE FOREXPRAST"<<std::endl; // Debug
	this->varName = varName;
	this->start = start;
	this->end = end;
	this->step = step;
	this->body = body;
}*/

void ForExprAST::visit()
{
	std::cout<<"("<<" ";
	std::cout<<")";
}


Value * ForExprAST::codegen(driver &drv)
{
	std::cout<<"CODEGEN FOREXPRAST"<<std::endl; // Debug
	if (gettop()) 
		LogErrorV("Variabile del for non definita");

	Value * startVal = start->codegen(drv);
	if (!startVal)
		return nullptr;

	Function * function = drv.builder->GetInsertBlock()->getParent();
	BasicBlock * preHeaderBB = drv.builder->GetInsertBlock();
	BasicBlock * loopBB = BasicBlock::Create(*drv.context, "loop", function);

	drv.builder->CreateBr(loopBB);
	
	drv.builder->SetInsertPoint(loopBB);

	PHINode * variable = drv.builder->CreatePHI(Type::getDoubleTy(*drv.context), 2, varName);
	variable->addIncoming(startVal, preHeaderBB);

	Value * oldVal = drv.NamedValues[varName];
	drv.NamedValues[varName] = variable;

	Value * bodyValue = body->codegen(drv); // risolvo l'espressione nel body che devo ritornare come valore del for
	if (!bodyValue) 
		return nullptr;

	Value * stepVal = nullptr;
	if (step)
	{
		stepVal = step->codegen(drv);
		if (!stepVal)
			return nullptr;
	}
	else // Se non è stato specificato lo step
	{
		stepVal = ConstantFP::get(*drv.context, APFloat(1.0));
	}

	Value * endCond = end->codegen(drv);
	if (!endCond)
		return nullptr;

	endCond = drv.builder->CreateFCmpONE(endCond, ConstantFP::get(*drv.context, APFloat(0.0)), "loopcond");

	Value * nextVar = drv.builder->CreateFAdd(variable, stepVal, "nextvar");

	BasicBlock * loopEndBB = drv.builder->GetInsertBlock();
	BasicBlock * afterBB = BasicBlock::Create(*drv.context, "afterloop", function);

	drv.builder->CreateCondBr(endCond, loopBB, afterBB);

	drv.builder->SetInsertPoint(afterBB); // Tutto il codice seguente al loop verrà inserito nell'afterBB

	variable->addIncoming(nextVar, loopEndBB);

	if (oldVal)
		drv.NamedValues[varName] = oldVal;
	else
		drv.NamedValues.erase(varName);

	return bodyValue;
	
}

