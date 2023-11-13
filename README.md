# Programma del corso

## 1. Front end. 48h
1) Compilatori ed interpreti. Struttura di un compilatore, front-, middle- e back-end. Fasi della compilazione. Applicazioni della tecnologia dei compilatori. Richiami su alcuni aspetti dei linguaggi di programmazione. 3h

2) Analisi lessicale. Il lexer. Tokenizzazione dell’input. Symbol table. Linguaggi ed espressioni regolari. Definizione e riconoscimento di token. Automi finiti. Dalle espressioni regolari agli automi. Simulazione e “compilazione” del non determinismo. Strumenti per generare un lexer. LEX con esempi. 10h

3) Analisi sintattica. Il parser. Grammatiche regolari e context-free, derivazioni, parse tree, ambiguità. Progetto di grammatiche. Aspetti demandati all’analisi semantica. Parsing top-down: a discesa ricorsiva e predittivo. Parsing bottom-up. Riduzioni, parsing shft-reduce. Parsing LR. Tabelle SLR e LALR. Gestione dei conflitti. Symbol table e analisi semantica. Strumenti per la generazione automatica di parser. YACC con esempi. 14h

4) Traduzione guidata dalla sintassi. Definizioni guidate dalla sintassi. Attributi e regole. Ordine di valutazione degli attributi. Attributi sintetizzati e attributi ereditati. Applicazioni alla costruzione di Abstract-Syntax Tree (AST). Schemi di traduzione. 7h

5) Interpretazione. Modelli di esecuzione AST (caso del Perl). 4h
   
6) Generazione di codice intermedio. Codice a tre indirizzi. Variabili Static Single-Assignment (SSA). Rappresentazione intermedia (IR) nel modello LLVM. Cenni su trattamento dei tipi. Espressioni e controllo di flusso. Backpatching. 10h

## 2. Middle-end e ruolo dell’ottimizzazione. 32h
1) Introduzione. Rappresentazione dei programmi, richiami sulle IR. Sintassi concreta e astratta (AST). Istruzioni (3AC). Rappresentazioni di più alto livello. Control flow graph (CFG): basic block e terminator, successori e predecessori. Costruzione di un CFG di basic block. 3h

2) Analisi e ottimizzazione del codice. Analisi locale, globale e interprocedurale. Esempio di ottimizzazione: dead code elimination (DCE). Identificare le istruzioni “dead” e “killed”. I limiti dell’ottimizzazione locale. 3h

3) Data Flow Analysis. Le reaching definition come esempio di proprietà globale che richiede di ragionare sull’intero CFG. Dominance e liveness analysis. 4h
4) Ottimizzazione dei loop. Forma SSA e ottimizzazione. Esempi notevoli di ottimizzazioni. Loop-Invariant Code Motion (LICM). Induction Variable Elimination. Loop Fission & Fusion. 10h

5) Ottimizzazione interprocedurale. Call graph Inlining e outlining. Link-time optimization. 6h

6) Cenni di ottimizzazione per architetture parallele. Ruolo della gerarchia di memoria. Instruction-level parallelism e thread-level parallelism. Vettorizzazione. 6h

## 3. Backend. 16h
1) Machine description. Istruzioni, registri, calling convention. 3h

2) Instruction Selection. Da IR machine-agnostic ad equivalente funzionale dell’Instruction Set Archtecture target. 3h

3) Instruction Scheduling. Ordine di esecuzione delle istruzioni per massimizzare la performance. 3h

4) Register Allocation. Dai registri virtuali a quelli fisici. Register spilling. 3h

5) Altre ottimizzazioni machine-specific. 4h

---

# Laboratorio

- Nella prima parte è stato realizzato il ***front-end*** fi un compilatore per il linguaggio ***Kaleidoscope*** con la produzione di Intermediate Representation di ***LLVM***.
- Nella parte di ***back-end*** sono stati realizzati diversi passi per l'ottimizzazione ottimizzazione del codice da sorgente a rappresentazione intermedia.