//
//  compiler.c
//  modelmem
//
//  Created by ThrudTheBarbarian on 09/12/2025.
//

#include <stdio.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "object.h"
#include "memory.h"

#ifdef DEBUG_PRINT_CODE
#  include "debug.h"
#endif

typedef struct
    {
    Token current;              // Current token
    Token previous;             // Previous token
    bool hadError;              // Did we encounter an error ?
    bool panicMode;             // If so, enable panic mode and suppress errors
    } Parser;

typedef struct
    {
    Token name;                 // Name of a local variable
    int depth;                  // scope-depth of the block we're in
    bool isCaptured;            // true if captured by any later nested function
    } Local;

typedef struct
    {
    uint8_t index;              // Local slot in parent that we are capturing
    bool isLocal;               // Is this local to current context
    } Upvalue;

typedef enum
    {
    TYPE_FUNCTION,
    TYPE_METHOD,
    TYPE_SCRIPT,
    TYPE_INITIALIZER
    } FunctionType;

typedef struct Compiler
    {
    struct Compiler* enclosing;     // Parent compiler, or NULL
    ObjFunction* function;          // Current function being compiled
    FunctionType type;              // Type of current function
    
    Local locals[UINT8_COUNT];      // List of local variable indices
    int localCount;                 // Number of local variables at this scope
    Upvalue upvalues[UINT8_COUNT];  // Captured-scope values
    int scopeDepth;                 // Scope identifier
    } Compiler;
    
    
typedef struct ClassCompiler        // Current, innermost class being compiled
    {
    struct ClassCompiler* enclosing;
    } ClassCompiler;

typedef enum
    {
    PREC_NONE,
    PREC_ASSIGNMENT,            // =
    PREC_OR,                    // or
    PREC_AND,                   // and
    PREC_EQUALITY,              // == !=
    PREC_COMPARISON,            // < > <= >=
    PREC_TERM,                  // + -
    PREC_FACTOR,                // * /
    PREC_UNARY,                 // ! -
    PREC_CALL,                  // . ()
    PREC_PRIMARY
    } Precedence;

// Functions used for parsing various entities
typedef void (*ParseFn)(bool canAssign);

// Define a type that provides:
//  - a function to parse a prefix expression, starting with token of type
//  - a function to parse an infix expression, followed by token of type
//  - precendence of the infix expression for token of that type
typedef struct
    {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
    } ParseRule;

// Function declarations:
static void call(bool canAssign);
static void grouping(bool canAssign);
static void unary(bool canAssign);
static void binary(bool canAssign);
static void number(bool canAssign);
static void literal(bool canAssign);
static void string(bool canAssign);
static void expression(void);
static void statement(void);
static void declaration(void);
static void variable(bool canAssign);
static void namedVariable(Token name, bool canAssign);
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);
static uint8_t identifierConstant(Token* name);
static bool match(TokenType type);
static bool identifiersEqual(Token* a, Token* b);
static void and_(bool canAssign);
static int emitJump(uint8_t instruction);
static void patchJump(int offset);
static void or_(bool canAssign);
static void beginScope(void);
static void endScope(void);
static void block(void);
static bool check(TokenType type);
static void dot(bool canAssign);
static void this_(bool canAssign);


// Define Pratt parser table using the above:
ParseRule rules[] = {
  [TOKEN_LEFT_PAREN]    = {grouping, call,   PREC_CALL},
  [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
  [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_DOT]           = {NULL,     dot,    PREC_CALL},
  [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
  [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
  [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
  [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
  [TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
  [TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_EQUALITY},
  [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
  [TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
  [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
  [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
  [TOKEN_AND]           = {NULL,     and_,   PREC_AND},
  [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
  [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
  [TOKEN_OR]            = {NULL,     or_,    PREC_OR},
  [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_THIS]          = {this_,    NULL,   PREC_NONE},
  [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
  [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};



Parser parser;                          // The current parser state
Compiler* current           = NULL;     // Local variable management
ClassCompiler* currentClass = NULL;     // Current, innermost compiled class

/*****************************************************************************\
|* Helper functions - handle syntax errors
\*****************************************************************************/
static void errorAt(Token* token, const char* message)
    {
    // If we've already seen an error, don't confuse matters, just return
    if (parser.panicMode)
        return;
    parser.panicMode = true;
    
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF)
        fprintf(stderr, " at end");
    else if (token->type == TOKEN_ERROR)
        ;
    else
        fprintf(stderr, " at '%.*s'", token->length, token->start);

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
    }

static void error(const char* message)
    {
    errorAt(&parser.previous, message);
    }

static void errorAtCurrent(const char* message)
    {
    errorAt(&parser.current, message);
    }

/*****************************************************************************\
|* Helper function - advance by one token
\*****************************************************************************/
static void advance(void)
    {
    parser.previous = parser.current;

    for (;;)
        {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR)
            break;

        errorAtCurrent(parser.current.start);
        }
    }

/*****************************************************************************\
|* Helper function - consume a token with a check to see if it's what we expect
\*****************************************************************************/
static void consume(TokenType type, const char* message)
    {
    if (parser.current.type == type)
        {
        advance();
        return;
        }

    errorAtCurrent(message);
    }

/*****************************************************************************\
|* Helper function - return the chunk we're compiling to
\*****************************************************************************/
static Chunk* currentChunk(void)
    {
    return &current->function->chunk;
    }

/*****************************************************************************\
|* Helper function - emit a single byte of compiled bytecode
\*****************************************************************************/
static void emitByte(uint8_t byte)
    {
    writeChunk(currentChunk(), byte, parser.previous.line);
    }

/*****************************************************************************\
|* Helper function - emit two  bytes of compiled bytecode
\*****************************************************************************/
static void emitBytes(uint8_t byte1, uint8_t byte2)
    {
    emitByte(byte1);
    emitByte(byte2);
    }

/*****************************************************************************\
|* Helper function - emit a 'return' operation
\*****************************************************************************/
static void emitReturn(void)
    {
    if (current->type == TYPE_INITIALIZER)
        emitBytes(OP_GET_LOCAL, 0);
    else
        emitByte(OP_NIL);
    emitByte(OP_RETURN);
    }

/*****************************************************************************\
|* Helper function - add a constant to the list, and return the index
\*****************************************************************************/
static uint8_t makeConstant(Value value)
    {
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX)
        {
        error("Too many constants in one chunk.");
        return 0;
        }

    return (uint8_t)constant;
    }

/*****************************************************************************\
|* Helper function - emit constant opcode and index
\*****************************************************************************/
static void emitConstant(Value value)
    {
    emitBytes(OP_CONSTANT, makeConstant(value));
    }

/*****************************************************************************\
|* Helper function - initialise local variable state
\*****************************************************************************/
static void initCompiler(Compiler* compiler, FunctionType type)
    {
    compiler->function      = NULL;
    compiler->type          = type;
    compiler->enclosing     = current;
    
    compiler->localCount    = 0;
    compiler->scopeDepth    = 0;
    
    // Bootstrap the compiler's current function
    compiler->function      = newFunction();
    
    current                 = compiler;
 
    if (type != TYPE_SCRIPT)
        current->function->name = copyString(parser.previous.start,
                                             parser.previous.length);

    // Claim first slot in locals for compiler's own use
    Local* local            = &current->locals[current->localCount++];
    local->depth            = 0;
    local->isCaptured       = false;
    
    // Handle this in classes
    if (type != TYPE_FUNCTION)
        {
        local->name.start = "this";
        local->name.length = 4;
        }
    else
        {
        local->name.start = "";
        local->name.length = 0;
        }
    }

/*****************************************************************************\
|* Helper function - tidy up when compilation is done
\*****************************************************************************/
static ObjFunction * endCompiler(void)
    {
    emitReturn();
    
    ObjFunction* function = current->function;
    
    #ifdef DEBUG_PRINT_CODE
        if (!parser.hadError)
            disassembleChunk(currentChunk(),
                             function->name != NULL ? function->name->chars
                                                    : "<script>");
    #endif
    
    current = current->enclosing;
    return function;
    }

/*****************************************************************************\
|* Helper function - emit strings
\*****************************************************************************/
static void string(bool canAssign)
    {
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1,
                                    parser.previous.length - 2)));
    }

/*****************************************************************************\
|* Helper function - emit numbers
\*****************************************************************************/
static void number(bool canAssign)
    {
    int64_t val;
    sscanf(parser.previous.start, VALUE_FORMAT_STRING, &val);
    emitConstant(NUMBER_VAL(val));
    }

/*****************************************************************************\
|* Helper function - emit for 'true', 'false' and 'nil'
\*****************************************************************************/
static void literal(bool canAssign)
    {
    switch (parser.previous.type)
        {
        case TOKEN_FALSE:
            emitByte(OP_FALSE);
            break;
    
        case TOKEN_NIL:
            emitByte(OP_NIL);
            break;
    
        case TOKEN_TRUE:
            emitByte(OP_TRUE);
            break;
    
        default:
            return; // Unreachable.
        }
    }

/*****************************************************************************\
|* Helper function - allow variable access
\*****************************************************************************/
static void variable(bool canAssign)
    {
    namedVariable(parser.previous, canAssign);
    }

/*****************************************************************************\
|* Helper function - resolve a local variable
\*****************************************************************************/
static int resolveLocal(Compiler* compiler, Token* name)
    {
    for (int i = compiler->localCount - 1; i >= 0; i--)
        {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name))
            {
            if (local->depth == -1)
                error("Can't read local variable in its own initializer.");
            return i;
            }
        }

    return -1;
    }

/*****************************************************************************\
|* Helper function - register captured variable in our scope
\*****************************************************************************/
static int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal)
    {
    int upvalueCount = compiler->function->upvalueCount;
    
    for (int i = 0; i < upvalueCount; i++)
        {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal)
            return i;
        }
        
    if (upvalueCount == UINT8_COUNT)
        {
        error("Too many closure variables in function.");
        return 0;
        }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    return compiler->function->upvalueCount++;
    }

/*****************************************************************************\
|* Helper function - look for capture-vars in parent scopes, recursively
\*****************************************************************************/
static int resolveUpvalue(Compiler* compiler, Token* name)
    {
    if (compiler->enclosing == NULL)
        return -1;

    // Look for a matching local var in the enclosing function
    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1)
        {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)local, true);
        }
        
    // Look for a match beyond the enclosing function
    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1)
        return addUpvalue(compiler, (uint8_t)upvalue, false);

    return -1;
    }

/*****************************************************************************\
|* Helper function - allow named variable access
\*****************************************************************************/
static void namedVariable(Token name, bool canAssign)
    {
    uint8_t getOp, setOp;
    
    // Determine if we're using local or global variables
    int arg = resolveLocal(current, &name);
    if (arg != -1)
        {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
        }
    else if ((arg = resolveUpvalue(current, &name)) != -1)
        {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
        }
    else
        {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
        }
    
    // Handle assignment by looking ahead 1
    if (canAssign && match(TOKEN_EQUAL))
        {
        expression();
        emitBytes(setOp, arg);
        }
    else
        emitBytes(getOp, arg);
    }

/*****************************************************************************\
|* Helper function - fetch and return the number of arguments to a function
\*****************************************************************************/
static uint8_t argumentList(void)
    {
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN))
        {
        do
            {
            expression();
            if (argCount == 255)
                error("Can't have more than 255 arguments.");

            argCount++;
            }
        while (match(TOKEN_COMMA));
        }
  
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
    }

/*****************************************************************************\
|* Helper function - allow function calls
\*****************************************************************************/
static void call(bool canAssign)
    {
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
    }
    
/*****************************************************************************\
|* Helper function - allow logical and, basically an if statement
\*****************************************************************************/
static void and_(bool canAssign)
    {
    int endJump = emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);
    parsePrecedence(PREC_AND);

    patchJump(endJump);
    }

/*****************************************************************************\
|* Helper function - allow logical or
\*****************************************************************************/
static void or_(bool canAssign)
    {
    int elseJump    = emitJump(OP_JUMP_IF_FALSE);
    int endJump     = emitJump(OP_JUMP);

    patchJump(elseJump);
    emitByte(OP_POP);

    parsePrecedence(PREC_OR);
    patchJump(endJump);
    }


/*****************************************************************************\
|* Helper function - parse expressions of a given precedence or higher. This
|* then allows us to parse things like -a.b + c correctly as -(a.b) + c, else
|* the expression() code would chew all the way through -(a.b + c) and negate
|* the lot...
\*****************************************************************************/
static void parsePrecedence(Precedence precedence)
    {
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL)
        {
        error("Expect expression.");
        return;
        }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while (precedence <= getRule(parser.current.type)->precedence)
        {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
        }
        
    if (canAssign && match(TOKEN_EQUAL))
        error("Invalid assignment target.");
    }


/*****************************************************************************\
|* Helper function - insert the token's lexeme to constant table as string
\*****************************************************************************/
static uint8_t identifierConstant(Token* name)
    {
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
    }

/*****************************************************************************\
|* Helper function - expression parsing. Join the parser and the emitter...
\*****************************************************************************/
static void expression(void)
    {
    parsePrecedence(PREC_ASSIGNMENT);
    }


/*****************************************************************************\
|* Helper function - handle parentheses as a group
\*****************************************************************************/
static void grouping(bool canAssign)
    {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
    }

/*****************************************************************************\
|* Helper function - handle unary minus
\*****************************************************************************/
static void unary(bool canAssign)
    {
    TokenType operatorType = parser.previous.type;

    // Compile the operand. Because we use PREC_UNARY we stop parsing the
    // expression as soon as something of lower precedence appears. This
    // means -a.b + c will stop at -a.b because PREC_UNARY (-) > PREC_TERM (+)
    parsePrecedence(PREC_UNARY);

    // Emit the operator instruction.
    switch (operatorType)
        {
        case TOKEN_BANG:
            emitByte(OP_NOT);
            break;
            
        case TOKEN_MINUS:
            emitByte(OP_NEGATE);
            break;
            
        default:
            return; // Unreachable.
        }
    }

/*****************************************************************************\
|* Helper function - class properties
\*****************************************************************************/
static void dot(bool canAssign)
    {
    consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
    uint8_t name = identifierConstant(&parser.previous);

    if (canAssign && match(TOKEN_EQUAL))
        {
        expression();
        emitBytes(OP_SET_PROPERTY, name);
        }
    else
        emitBytes(OP_GET_PROPERTY, name);
      }

/*****************************************************************************\
|* Helper function - Return the rule for a given operator type
\*****************************************************************************/
static ParseRule* getRule(TokenType type)
    {
    return &rules[type];
    }

/*****************************************************************************\
|* Helper function - class 'this' pointer
\*****************************************************************************/
static void this_(bool canAssign)
    {
    if (currentClass == NULL)
        {
        error("Can't use 'this' outside of a class.");
        return;
        }
        
    variable(false);
    }

/*****************************************************************************\
|* Helper function - handle infix arithmetic (eg: 2 + 3)
\*****************************************************************************/
static void binary(bool canAssign)
    {
    TokenType operatorType  = parser.previous.type;
    ParseRule* rule         = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));

    switch (operatorType)
        {
        case TOKEN_BANG_EQUAL:
            emitBytes(OP_EQUAL, OP_NOT);
            break;
            
        case TOKEN_EQUAL_EQUAL:
            emitByte(OP_EQUAL);
            break;
            
        case TOKEN_GREATER:
            emitByte(OP_GREATER);
            break;
            
        case TOKEN_GREATER_EQUAL:
            emitBytes(OP_LESS, OP_NOT);
            break;
            
        case TOKEN_LESS:
            emitByte(OP_LESS);
            break;
            
        case TOKEN_LESS_EQUAL:
            emitBytes(OP_GREATER, OP_NOT);
            break;
            
        case TOKEN_PLUS:
            emitByte(OP_ADD);
            break;
    
        case TOKEN_MINUS:
            emitByte(OP_SUBTRACT);
            break;
    
        case TOKEN_STAR:
            emitByte(OP_MULTIPLY);
            break;
        
        case TOKEN_SLASH:
            emitByte(OP_DIVIDE);
            break;
    
        default:
            return; // Unreachable.
        }
    }


/*****************************************************************************\
|* Helper function - check if a token is of a given type
\*****************************************************************************/
static bool check(TokenType type)
    {
    return parser.current.type == type;
    }

/*****************************************************************************\
|* Helper function - consume the token if we match a given type
\*****************************************************************************/
static bool match(TokenType type)
    {
    if (!check(type))
        return false;
    advance();
    return true;
    }

/*****************************************************************************\
|* Called to compile the code, public interface
\*****************************************************************************/
ObjFunction * compile(const char* source)
    {
    initScanner(source);
 
    // Manage scope-depthed local variables
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadError     = false;
    parser.panicMode    = false;
    
    advance();
    while (!match(TOKEN_EOF))
        {
        declaration();
        }

    ObjFunction* function = endCompiler();
    return parser.hadError ? NULL : function;
    }


/*****************************************************************************\
|* Helper function - we're panicking because of errors. Skip stuff until we
|* see something we recognise
\*****************************************************************************/
static void synchronize(void)
    {
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF)
        {
        if (parser.previous.type == TOKEN_SEMICOLON)
            return;
    
        switch (parser.current.type)
            {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;

            default:
                ; // Do nothing.
            }

        advance();
        }
    }

/*****************************************************************************\
|* Helper function - add a local variable to the scope
\*****************************************************************************/
static void addLocal(Token name)
    {
    if (current->localCount == UINT8_COUNT)
        {
        error("Too many local variables in function.");
        return;
        }

    Local* local        = &current->locals[current->localCount++];
    local->name         = name;
    local->isCaptured   = false;
    
    // updated in markInitialised(), called from defineVariable()
    local->depth        = -1;
    }

/*****************************************************************************\
|* Helper function - check to see if a name is taken
\*****************************************************************************/
static bool identifiersEqual(Token* a, Token* b)
    {
    if (a->length != b->length)
        return false;
    return memcmp(a->start, b->start, a->length) == 0;
    }


/*****************************************************************************\
|* Helper function - emit the code to store a local var if in a local scope
\*****************************************************************************/
static void declareVariable(void)
    {
    if (current->scopeDepth == 0)
        return;

    Token* name = &parser.previous;

    for (int i = current->localCount - 1; i >= 0; i--)
        {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth)
            break;
    

        if (identifiersEqual(name, &local->name))
            error("Already a variable with this name in this scope.");
        }
        
    addLocal(*name);
    }

/*****************************************************************************\
|* Helper function - parse a variable out. Requires next token to be identifier
\*****************************************************************************/
static uint8_t parseVariable(const char* errorMessage)
    {
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();
    if (current->scopeDepth > 0)
        return 0;

    return identifierConstant(&parser.previous);
    }

/*****************************************************************************\
|* Helper function - mark a local variable as initialised
\*****************************************************************************/
static void markInitialized(void)
    {
    // Functions can be in top-level scope
    if (current->scopeDepth == 0)
        return;
        
    current->locals[current->localCount - 1].depth = current->scopeDepth;
    }

/*****************************************************************************\
|* Helper function - define a global variable
\*****************************************************************************/
static void defineVariable(uint8_t global)
    {
    if (current->scopeDepth > 0)
        {
        markInitialized();
        return;
        }
        
    emitBytes(OP_DEFINE_GLOBAL, global);
    }

/*****************************************************************************\
|* Helper function - declare a variable
\*****************************************************************************/
static void varDeclaration(void)
    {
    uint8_t global = parseVariable("Expect variable name.");

    if (match(TOKEN_EQUAL))
        expression();
    else
        emitByte(OP_NIL);
  
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(global);
    }

/*****************************************************************************\
|* Helper function - compile a function
\*****************************************************************************/
static void function(FunctionType type)
    {
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    
    if (!check(TOKEN_RIGHT_PAREN))
        {
        do
            {
            current->function->arity++;
            if (current->function->arity > 255)
                errorAtCurrent("Can't have more than 255 parameters.");
                
            uint8_t constant = parseVariable("Expect parameter name.");
            defineVariable(constant);
            }
        while (match(TOKEN_COMMA));
        }

    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    ObjFunction* function = endCompiler();
    emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

    // Make sure the closure variables are captured
    for (int i = 0; i < function->upvalueCount; i++)
        {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
        }

    // no need for an endScope() call because we ended the compiler
    }

/*****************************************************************************\
|* Helper function - declare a function
\*****************************************************************************/
static void funDeclaration(void)
    {
    uint8_t global = parseVariable("Expect function name.");
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global);
    }


/*****************************************************************************\
|* Helper function - recover a class method
\*****************************************************************************/
static void method(void)
    {
    consume(TOKEN_IDENTIFIER, "Expect method name.");
    uint8_t constant = identifierConstant(&parser.previous);

    FunctionType type   = TYPE_METHOD;
    bool lengthMatch    = (parser.previous.length == 4);
    if (lengthMatch && memcmp(parser.previous.start, "init", 4) == 0)
        type = TYPE_INITIALIZER;
  
    function(type);
    
    emitBytes(OP_METHOD, constant);
    }

/*****************************************************************************\
|* Helper function - declare a class
\*****************************************************************************/
static void classDeclaration(void)
    {
    consume(TOKEN_IDENTIFIER, "Expect class name.");
    Token className = parser.previous;
    uint8_t nameConstant = identifierConstant(&parser.previous);
    declareVariable();

    emitBytes(OP_CLASS, nameConstant);
    defineVariable(nameConstant);

    ClassCompiler classCompiler;
    classCompiler.enclosing = currentClass;
    currentClass = &classCompiler;

    namedVariable(className, false);
    consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
        method();

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    emitByte(OP_POP);

    currentClass = currentClass->enclosing;
    }

/*****************************************************************************\
|* Manage declarations. We keep compiling declarations until we get to EOF
|*
|*  declaration    → classDecl
|*                 | funDecl
|*                 | varDecl
|*                 | statement ;
|*
\*****************************************************************************/
static void declaration(void)
    {
    if (match(TOKEN_CLASS))
        classDeclaration();
    else if (match(TOKEN_FUN))
        funDeclaration();
    else if (match(TOKEN_VAR))
        varDeclaration();
    else
        statement();
    
    // Stop panicking about errors if we are currently doing so
    if (parser.panicMode)
        synchronize();
    }


/*****************************************************************************\
|* Helper function - evaluates an expression and prints the result
\*****************************************************************************/
static void printStatement(void)
    {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
    }

/*****************************************************************************\
|* Helper function - evaluates an expression
\*****************************************************************************/
static void expressionStatement(void)
    {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
    }


/*****************************************************************************\
|* Helper function - enter a new scope
\*****************************************************************************/
static void beginScope(void)
    {
    current->scopeDepth++;
    }

/*****************************************************************************\
|* Helper function - exit the current scope
\*****************************************************************************/
static void endScope(void)
    {
    current->scopeDepth--;
 
    // Clean up any local variables in this scope we're exiting
    while (current->localCount > 0 &&
          current->locals[current->localCount - 1].depth > current->scopeDepth)
        {
        if (current->locals[current->localCount - 1].isCaptured)
            emitByte(OP_CLOSE_UPVALUE);
        else
            emitByte(OP_POP);
    
        current->localCount--;
        }
    }

/*****************************************************************************\
|* Helper function - handle blocks
\*****************************************************************************/
static void block(void)
    {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
        declaration();

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
    }

/*****************************************************************************\
|* Helper function - emit a jump with an unknown offset
\*****************************************************************************/
static int emitJump(uint8_t instruction)
    {
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count - 2;
    }

/*****************************************************************************\
|* Helper function - update the offset of where to jump to, in the previously
|* emitted jump
\*****************************************************************************/
static void patchJump(int offset)
    {
    // -2 to adjust for the bytecode for the jump offset itself.
    int jump = currentChunk()->count - offset - 2;

    if (jump > UINT16_MAX)
        error("Too much code to jump over.");

    currentChunk()->code[offset]        = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1]    = (jump     ) & 0xff;
    }

/*****************************************************************************\
|* Helper function - handle if statements
\*****************************************************************************/
static void ifStatement(void)
    {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);   // Tidy up the stack
    statement();

    int elseJump = emitJump(OP_JUMP);
    patchJump(thenJump);
    emitByte(OP_POP);   // =Tidy uo the stack

    if (match(TOKEN_ELSE))
        statement();
    patchJump(elseJump);
    }

/*****************************************************************************\
|* Helper function - Emit a loop opcode
\*****************************************************************************/
static void emitLoop(int loopStart)
    {
    emitByte(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX)
        error("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte( offset       & 0xff);
    }

/*****************************************************************************\
|* Helper function - handle while statements
\*****************************************************************************/
static void whileStatement(void)
    {
    int loopStart = currentChunk()->count;

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();
    
    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OP_POP);
    }

/*****************************************************************************\
|* Helper function - allow returns from functions
\*****************************************************************************/
static void returnStatement(void)
    {
    if (current->type == TYPE_SCRIPT)
        error("Can't return from top-level code.");
  
    if (match(TOKEN_SEMICOLON))
        emitReturn();
    else
        {
        if (current->type == TYPE_INITIALIZER)
            error("Can't return a value from an initializer.");
       
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);
        }
    }

/*****************************************************************************\
|* Helper function - handle for statements
\*****************************************************************************/
static void forStatement(void)
    {
    beginScope();   // In case we declare variables in the for(;;) statement

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(TOKEN_SEMICOLON))
        ; // No initializer.
    else if (match(TOKEN_VAR))
        varDeclaration();
    else
        expressionStatement();

    int loopStart   = currentChunk()->count;
    int exitJump    = -1;
    if (!match(TOKEN_SEMICOLON))
        {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // Jump out of the loop if the condition is false.
        exitJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP); // Condition.
        }
        
    // increment clause
    if (!match(TOKEN_RIGHT_PAREN))
        {
        int bodyJump        = emitJump(OP_JUMP);
        int incrementStart  = currentChunk()->count;
        
        expression();
        emitByte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emitLoop(loopStart);
        loopStart = incrementStart;
        patchJump(bodyJump);
        }

    statement();
    emitLoop(loopStart);

    if (exitJump != -1)
        {
        patchJump(exitJump);
        emitByte(OP_POP); // Condition.
        }
    
    endScope(); // tidy up the scope
   }

/*****************************************************************************\
|* Statements are declarations that are allowed inside a control-flow body.
|* They must leave the stack in the state they found it, unlike expressions
|* which must leave one value on the stack
|*
|*  statement      → exprStmt
|*                 | forStmt
|*                 | ifStmt
|*                 | printStmt
|*                 | returnStmt
|*                 | whileStmt
|*                 | block ;
|*
|*  block          → "{" declaration* "}" ;
|*
\*****************************************************************************/
static void statement(void)
    {
    if (match(TOKEN_PRINT))
        printStatement();
    else if (match(TOKEN_IF))
        ifStatement();
    else if (match(TOKEN_RETURN))
        returnStatement();
    else if (match(TOKEN_WHILE))
        whileStatement();
    else if (match(TOKEN_FOR))
        forStatement();
    else if (match(TOKEN_LEFT_BRACE))
        {
        beginScope();
        block();
        endScope();
        }
    else
        expressionStatement();
    }


/*****************************************************************************\
|* GC: Do a mark of all the compiler roots we want to keep
\*****************************************************************************/
void markCompilerRoots(void)
    {
    Compiler* compiler = current;
    while (compiler != NULL)
        {
        markObject((Obj*)compiler->function);
        compiler = compiler->enclosing;
        }
    }
