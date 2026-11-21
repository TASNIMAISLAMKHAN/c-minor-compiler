%define api.prefix {cmyy}
%define parse.error detailed
%locations

%code requires {
    #include <stdint.h>
    #include "cminor/ast/ast.h"
    #include "cminor/parser/parser_context.h"
}

%code provides {
    int cmyylex(ParserContext *ctx);
    void cmyyerror(ParserContext *ctx, const char *message);
}

%code {
    #include <stdlib.h>
    #include "cminor/ast/ast.h"

    static SourceSpan make_span(CMYYLTYPE loc) {
        SourceSpan span;

        span.start = (SourceLocation){
            (size_t)loc.first_line,
            (size_t)loc.first_column,
            0
        };

        span.end = (SourceLocation){
            (size_t)loc.last_line,
            (size_t)loc.last_column,
            0
        };

        return span;
    }

    static AstNode *make_statement(
        AstKind kind,
        SourceSpan span,
        AstNode *first,
        AstNode *second
    ) {
        AstNode *node = ast_new(kind, span);

        if (node != NULL) {
            node->first = first;
            node->second = second;
        }

        return node;
    }
}

%parse-param { ParserContext *ctx }
%lex-param { ParserContext *ctx }

%union {
    AstNode *node;
    char *text;
    int32_t integer;
}

%token T_INT T_VOID T_IF T_ELSE T_WHILE T_FOR
%token T_BREAK T_CONTINUE T_RETURN

%token T_PLUS T_MINUS T_MULTIPLY T_DIVIDE T_MODULO
%token T_ASSIGN T_EQUAL T_NOT_EQUAL
%token T_LESS T_GREATER T_LESS_EQUAL T_GREATER_EQUAL
%token T_NOT T_LOGICAL_AND T_LOGICAL_OR

%token T_LEFT_PAREN T_RIGHT_PAREN
%token T_LEFT_BRACE T_RIGHT_BRACE
%token T_SEMICOLON T_COMMA

%token <text> T_IDENTIFIER
%token <integer> T_INTEGER_LITERAL

%type <node> program external_list external
%type <node> function_decl function_def return_type
%type <node> parameters param_list parameter
%type <node> local_decl block block_items block_item
%type <node> statement expr_stmt if_stmt while_stmt for_stmt
%type <node> for_init assignment_opt
%type <node> assignment_expr logical_or logical_and equality
%type <node> relational additive multiplicative unary primary
%type <node> argument_list arguments_opt

%destructor { free($$); } <text>

%precedence IF_WITHOUT_ELSE
%precedence T_ELSE

%start program

%%

program:
    external_list
    {
        $$ = ast_new(AST_PROGRAM, make_span(@$));
        if ($$ != NULL) {
            $$->children = $1;
            ctx->program = $$;
        }
    }
;

external_list:
    %empty
    {
        $$ = NULL;
    }
  | external_list external
    {
        $$ = ast_list_append($1, $2);
    }
;

external:
    function_decl { $$ = $1; }
  | function_def  { $$ = $1; }
;

return_type:
    T_INT
    {
        $$ = ast_new(AST_INTEGER, make_span(@$));
        if ($$ != NULL) $$->integer_value = 0;
    }
  | T_VOID
    {
        $$ = ast_new(AST_INTEGER, make_span(@$));
        if ($$ != NULL) $$->integer_value = 1;
    }
;

function_decl:
    return_type T_IDENTIFIER T_LEFT_PAREN parameters
    T_RIGHT_PAREN T_SEMICOLON
    {
        $$ = ast_new_named(AST_FUNCTION, make_span(@$), $2);
        if ($$ != NULL) {
            $$->is_void = $1->integer_value == 1;
            $$->is_definition = false;
            $$->first = $4;
        }
        ast_free($1);
    }
;

function_def:
    return_type T_IDENTIFIER T_LEFT_PAREN parameters
    T_RIGHT_PAREN block
    {
        $$ = ast_new_named(AST_FUNCTION, make_span(@$), $2);
        if ($$ != NULL) {
            $$->is_void = $1->integer_value == 1;
            $$->is_definition = true;
            $$->first = $4;
            $$->second = $6;
        }
        ast_free($1);
    }
;

parameters:
    T_VOID { $$ = NULL; }
  | param_list { $$ = $1; }
;

param_list:
    parameter
    {
        $$ = $1;
    }
  | param_list T_COMMA parameter
    {
        $$ = ast_list_append($1, $3);
    }
;

parameter:
    T_INT
    {
        $$ = ast_new(AST_PARAMETER, make_span(@$));
    }
  | T_INT T_IDENTIFIER
    {
        $$ = ast_new_named(AST_PARAMETER, make_span(@$), $2);
    }
;

local_decl:
    T_INT T_IDENTIFIER T_SEMICOLON
    {
        $$ = ast_new_named(AST_VARIABLE_DECLARATION, make_span(@$), $2);
    }
  | T_INT T_IDENTIFIER T_ASSIGN assignment_expr T_SEMICOLON
    {
        $$ = ast_new_named(AST_VARIABLE_DECLARATION, make_span(@$), $2);
        if ($$ != NULL) $$->first = $4;
    }
;

block:
    T_LEFT_BRACE block_items T_RIGHT_BRACE
    {
        $$ = ast_new(AST_BLOCK, make_span(@$));
        if ($$ != NULL) $$->children = $2;
    }
;

block_items:
    %empty
    {
        $$ = NULL;
    }
  | block_items block_item
    {
        $$ = ast_list_append($1, $2);
    }
;

block_item:
    local_decl { $$ = $1; }
  | statement  { $$ = $1; }
;

statement:
    block { $$ = $1; }
  | expr_stmt { $$ = $1; }
  | if_stmt { $$ = $1; }
  | while_stmt { $$ = $1; }
  | for_stmt { $$ = $1; }
  | T_BREAK T_SEMICOLON
    {
        $$ = ast_new(AST_BREAK_STATEMENT, make_span(@$));
    }
  | T_CONTINUE T_SEMICOLON
    {
        $$ = ast_new(AST_CONTINUE_STATEMENT, make_span(@$));
    }
  | T_RETURN T_SEMICOLON
    {
        $$ = ast_new(AST_RETURN_STATEMENT, make_span(@$));
    }
  | T_RETURN assignment_expr T_SEMICOLON
    {
        $$ = make_statement(
            AST_RETURN_STATEMENT, make_span(@$), $2, NULL
        );
    }
;

expr_stmt:
    T_SEMICOLON
    {
        $$ = ast_new(AST_EMPTY_STATEMENT, make_span(@$));
    }
  | assignment_expr T_SEMICOLON
    {
        $$ = make_statement(
            AST_EXPRESSION_STATEMENT, make_span(@$), $1, NULL
        );
    }
;

if_stmt:
    T_IF T_LEFT_PAREN assignment_expr T_RIGHT_PAREN statement
    %prec IF_WITHOUT_ELSE
    {
        $$ = make_statement(
            AST_IF_STATEMENT, make_span(@$), $3, $5
        );
    }
  | T_IF T_LEFT_PAREN assignment_expr T_RIGHT_PAREN statement
    T_ELSE statement
    {
        $$ = make_statement(
            AST_IF_STATEMENT, make_span(@$), $3, $5
        );
        if ($$ != NULL) $$->third = $7;
    }
;

while_stmt:
    T_WHILE T_LEFT_PAREN assignment_expr T_RIGHT_PAREN statement
    {
        $$ = make_statement(
            AST_WHILE_STATEMENT, make_span(@$), $3, $5
        );
    }
;

for_stmt:
    T_FOR T_LEFT_PAREN for_init assignment_opt T_SEMICOLON
    assignment_opt T_RIGHT_PAREN statement
    {
        $$ = ast_new(AST_FOR_STATEMENT, make_span(@$));
        if ($$ != NULL) {
            $$->first = $3;
            $$->second = $4;
            $$->third = $6;
            $$->children = $8;
        }
    }
;

for_init:
    local_decl
    {
        $$ = $1;
    }
  | assignment_opt T_SEMICOLON
    {
        if ($1 != NULL) {
            $$ = make_statement(
                AST_EXPRESSION_STATEMENT, make_span(@$), $1, NULL
            );
        } else {
            $$ = NULL;
        }
    }
;

assignment_opt:
    %empty { $$ = NULL; }
  | assignment_expr { $$ = $1; }
;

assignment_expr:
    logical_or { $$ = $1; }
  | logical_or T_ASSIGN assignment_expr
    {
        $$ = ast_new_operation(
            AST_ASSIGNMENT, make_span(@$), TOKEN_ASSIGN, $1, $3
        );
    }
;

logical_or:
    logical_and { $$ = $1; }
  | logical_or T_LOGICAL_OR logical_and
    {
        $$ = ast_new_operation(
            AST_BINARY, make_span(@$), TOKEN_LOGICAL_OR, $1, $3
        );
    }
;

logical_and:
    equality { $$ = $1; }
  | logical_and T_LOGICAL_AND equality
    {
        $$ = ast_new_operation(
            AST_BINARY, make_span(@$), TOKEN_LOGICAL_AND, $1, $3
        );
    }
;

equality:
    relational { $$ = $1; }
  | equality T_EQUAL relational
    {
        $$ = ast_new_operation(
            AST_BINARY, make_span(@$), TOKEN_EQUAL, $1, $3
        );
    }
  | equality T_NOT_EQUAL relational
    {
        $$ = ast_new_operation(
            AST_BINARY, make_span(@$), TOKEN_NOT_EQUAL, $1, $3
        );
    }
;

relational:
    additive { $$ = $1; }
  | relational T_LESS additive
    {
        $$ = ast_new_operation(
            AST_BINARY, make_span(@$), TOKEN_LESS, $1, $3
        );
    }
  | relational T_LESS_EQUAL additive
    {
        $$ = ast_new_operation(
            AST_BINARY, make_span(@$), TOKEN_LESS_EQUAL, $1, $3
        );
    }
  | relational T_GREATER additive
    {
        $$ = ast_new_operation(
            AST_BINARY, make_span(@$), TOKEN_GREATER, $1, $3
        );
    }
  | relational T_GREATER_EQUAL additive
    {
        $$ = ast_new_operation(
            AST_BINARY, make_span(@$), TOKEN_GREATER_EQUAL, $1, $3
        );
    }
;

additive:
    multiplicative { $$ = $1; }
  | additive T_PLUS multiplicative
    {
        $$ = ast_new_operation(
            AST_BINARY, make_span(@$), TOKEN_PLUS, $1, $3
        );
    }
  | additive T_MINUS multiplicative
    {
        $$ = ast_new_operation(
            AST_BINARY, make_span(@$), TOKEN_MINUS, $1, $3
        );
    }
;

multiplicative:
    unary { $$ = $1; }
  | multiplicative T_MULTIPLY unary
    {
        $$ = ast_new_operation(
            AST_BINARY, make_span(@$), TOKEN_MULTIPLY, $1, $3
        );
    }
  | multiplicative T_DIVIDE unary
    {
        $$ = ast_new_operation(
            AST_BINARY, make_span(@$), TOKEN_DIVIDE, $1, $3
        );
    }
  | multiplicative T_MODULO unary
    {
        $$ = ast_new_operation(
            AST_BINARY, make_span(@$), TOKEN_MODULO, $1, $3
        );
    }
;

unary:
    primary { $$ = $1; }
  | T_PLUS unary
    {
        $$ = ast_new_operation(
            AST_UNARY, make_span(@$), TOKEN_PLUS, $2, NULL
        );
    }
  | T_MINUS unary
    {
        $$ = ast_new_operation(
            AST_UNARY, make_span(@$), TOKEN_MINUS, $2, NULL
        );
    }
  | T_NOT unary
    {
        $$ = ast_new_operation(
            AST_UNARY, make_span(@$), TOKEN_NOT, $2, NULL
        );
    }
;

primary:
    T_INTEGER_LITERAL
    {
        $$ = ast_new_integer(make_span(@$), $1);
    }
  | T_IDENTIFIER
    {
        $$ = ast_new_named(AST_IDENTIFIER, make_span(@$), $1);
    }
  | T_IDENTIFIER T_LEFT_PAREN arguments_opt T_RIGHT_PAREN
    {
        $$ = ast_new_named(AST_CALL, make_span(@$), $1);
        if ($$ != NULL) $$->children = $3;
    }
  | T_LEFT_PAREN assignment_expr T_RIGHT_PAREN
    {
        $$ = make_statement(
            AST_GROUP, make_span(@$), $2, NULL
        );
    }
;

arguments_opt:
    %empty { $$ = NULL; }
  | argument_list { $$ = $1; }
;

argument_list:
    assignment_expr
    {
        $$ = $1;
    }
  | argument_list T_COMMA assignment_expr
    {
        $$ = ast_list_append($1, $3);
    }
;

%%

void cmyyerror(ParserContext *ctx, const char *message) {
    SourceSpan span = {
        {
            (size_t)cmyylloc.first_line,
            (size_t)cmyylloc.first_column,
            0
        },
        {
            (size_t)cmyylloc.last_line,
            (size_t)cmyylloc.last_column,
            0
        }
    };

    ctx->syntax_errors++;

    diagnostic_report(
        ctx->diagnostics,
        DIAGNOSTIC_ERROR,
        span,
        "syntax error: %s",
        message
    );
}
