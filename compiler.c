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


// Define Pratt parser table using the above:
ParseRule rules[] = {
  [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
  [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
  [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
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
  [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
  [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
  [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
  [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};



Parser parser;                  // The current parser state
Chunk* compilingChunk;          // The chunk we're compiling to right now

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
    return compilingChunk;
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
|* Helper function - tidy up when compilation is done
\*****************************************************************************/
static void endCompiler(void)
    {
    emitReturn();
    
    #ifdef DEBUG_PRINT_CODE
        if (!parser.hadError)
            disassembleChunk(currentChunk(), "code");
    #endif
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
|* Helper function - allow named variable access
\*****************************************************************************/
static void namedVariable(Token name, bool canAssign)
    {
    uint8_t arg = identifierConstant(&name);
    
    // Handle assignment by looking ahead 1
    if (canAssign && match(TOKEN_EQUAL))
        {
        expression();
        emitBytes(OP_SET_GLOBAL, arg);
        }
    else
        emitBytes(OP_GET_GLOBAL, arg);
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
|* Helper function - Return the rule for a given operator type
\*****************************************************************************/
static ParseRule* getRule(TokenType type)
    {
    return &rules[type];
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
bool compile(const char* source, Chunk* chunk)
    {
    initScanner(source);
    
    compilingChunk      = chunk;
    parser.hadError     = false;
    parser.panicMode    = false;
    
    advance();
    while (!match(TOKEN_EOF))
        {
        declaration();
        }

    endCompiler();
    return !parser.hadError;
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
|* Helper function - parse a variable out. Requires next token to be identifier
\*****************************************************************************/
static uint8_t parseVariable(const char* errorMessage)
    {
    consume(TOKEN_IDENTIFIER, errorMessage);
    return identifierConstant(&parser.previous);
    }

/*****************************************************************************\
|* Helper function - define a global variable
\*****************************************************************************/
static void defineVariable(uint8_t global)
    {
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
    if (match(TOKEN_VAR))
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
\*****************************************************************************/
static void statement(void)
    {
    if (match(TOKEN_PRINT))
        printStatement();
    else
        expressionStatement();
    }
