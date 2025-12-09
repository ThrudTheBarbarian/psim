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
typedef void (*ParseFn)(void);

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
static void grouping(void);
static void unary(void);
static void binary(void);
static void number(void);
static void literal(void);
static void expression(void);
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);



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
  [TOKEN_IDENTIFIER]    = {NULL,     NULL,   PREC_NONE},
  [TOKEN_STRING]        = {NULL,     NULL,   PREC_NONE},
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
|* Helper function - emit numbers
\*****************************************************************************/
static void number(void)
    {
    int64_t val;
    sscanf(parser.previous.start, VALUE_FORMAT_STRING, &val);
    emitConstant(NUMBER_VAL(val));
    }

/*****************************************************************************\
|* Helper function - emit for 'true', 'false' and 'nil'
\*****************************************************************************/
static void literal(void)
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

    prefixRule();

    while (precedence <= getRule(parser.current.type)->precedence)
        {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule();
        }
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
static void grouping(void)
    {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
    }

/*****************************************************************************\
|* Helper function - handle unary minus
\*****************************************************************************/
static void unary(void)
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
static void binary(void)
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
|* Called to compile the code, public interface
\*****************************************************************************/
bool compile(const char* source, Chunk* chunk)
    {
    initScanner(source);
    
    compilingChunk      = chunk;
    parser.hadError     = false;
    parser.panicMode    = false;
    
    advance();
    expression();
    consume(TOKEN_EOF, "Expect end of expression.");

    endCompiler();
    return !parser.hadError;
    }
