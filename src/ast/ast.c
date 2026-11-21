#include <stdlib.h>
#include <string.h>

#include "cminor/ast/ast.h"

AstNode *ast_new(AstKind kind, SourceSpan span) {
    AstNode *node = calloc(1, sizeof(AstNode));

    if (node == NULL) {
        return NULL;
    }

    node->kind = kind;
    node->span = span;

    return node;
}

AstNode *ast_new_named(AstKind kind, SourceSpan span, char *name) {
    AstNode *node = ast_new(kind, span);

    if (node == NULL) {
        free(name);
        return NULL;
    }

    node->name = name;
    return node;
}

AstNode *ast_new_integer(SourceSpan span, int32_t value) {
    AstNode *node = ast_new(AST_INTEGER, span);

    if (node != NULL) {
        node->integer_value = value;
    }

    return node;
}

AstNode *ast_new_operation(
    AstKind kind,
    SourceSpan span,
    int operation,
    AstNode *left,
    AstNode *right
) {
    AstNode *node = ast_new(kind, span);

    if (node != NULL) {
        node->operation = operation;
        node->first = left;
        node->second = right;
    }

    return node;
}

AstNode *ast_list_append(AstNode *list, AstNode *node) {
    if (node == NULL) {
        return list;
    }

    if (list == NULL) {
        return node;
    }

    AstNode *tail = list;

    while (tail->next != NULL) {
        tail = tail->next;
    }

    tail->next = node;
    return list;
}

static const char *kind_name(AstKind kind) {
    switch (kind) {
        case AST_PROGRAM: return "Program";
        case AST_FUNCTION: return "Function";
        case AST_PARAMETER: return "Parameter";
        case AST_BLOCK: return "Block";
        case AST_VARIABLE_DECLARATION: return "VariableDeclaration";
        case AST_EXPRESSION_STATEMENT: return "ExpressionStatement";
        case AST_EMPTY_STATEMENT: return "EmptyStatement";
        case AST_IF_STATEMENT: return "IfStatement";
        case AST_WHILE_STATEMENT: return "WhileStatement";
        case AST_FOR_STATEMENT: return "ForStatement";
        case AST_BREAK_STATEMENT: return "BreakStatement";
        case AST_CONTINUE_STATEMENT: return "ContinueStatement";
        case AST_RETURN_STATEMENT: return "ReturnStatement";
        case AST_INTEGER: return "Integer";
        case AST_IDENTIFIER: return "Identifier";
        case AST_CALL: return "Call";
        case AST_ASSIGNMENT: return "Assignment";
        case AST_BINARY: return "BinaryExpression";
        case AST_UNARY: return "UnaryExpression";
        case AST_GROUP: return "ParenthesizedExpression";
        default: return "Unknown";
    }
}

static void indent(FILE *out, int depth) {
    for (int i = 0; i < depth; i++) {
        fputs("  ", out);
    }
}

static void dump_node(const AstNode *node, FILE *out, int depth) {
    for (; node != NULL; node = node->next) {
        indent(out, depth);
        fprintf(out, "%s", kind_name(node->kind));

        if (node->name != NULL) {
            fprintf(out, " name=\"%s\"", node->name);
        }

        if (node->kind == AST_INTEGER) {
            fprintf(out, " value=%d", node->integer_value);
        }

        if (node->kind == AST_FUNCTION) {
            fprintf(out, " return_type=%s",
                    node->is_void ? "void" : "int");
            fprintf(out, " form=%s",
                    node->is_definition ? "definition" : "prototype");
        }

        if (node->kind == AST_BINARY ||
            node->kind == AST_UNARY ||
            node->kind == AST_ASSIGNMENT) {
            fprintf(out, " operator=%s",
                    token_kind_name((TokenKind)node->operation));
        }

        fprintf(
            out,
            " [%zu:%zu-%zu:%zu]\n",
            node->span.start.line,
            node->span.start.column,
            node->span.end.line,
            node->span.end.column
        );

        if (node->kind == AST_PROGRAM) {
            dump_node(node->children, out, depth + 1);
        } else if (node->kind == AST_FUNCTION) {
            indent(out, depth + 1);
            fputs("Parameters\n", out);
            dump_node(node->first, out, depth + 2);

            if (node->second != NULL) {
                indent(out, depth + 1);
                fputs("Body\n", out);
                dump_node(node->second, out, depth + 2);
            }
        } else if (node->kind == AST_BLOCK) {
            dump_node(node->children, out, depth + 1);
        } else if (node->kind == AST_IF_STATEMENT) {
            indent(out, depth + 1);
            fputs("Condition\n", out);
            dump_node(node->first, out, depth + 2);
            indent(out, depth + 1);
            fputs("Then\n", out);
            dump_node(node->second, out, depth + 2);

            if (node->third != NULL) {
                indent(out, depth + 1);
                fputs("Else\n", out);
                dump_node(node->third, out, depth + 2);
            }
        } else if (node->kind == AST_WHILE_STATEMENT) {
            indent(out, depth + 1);
            fputs("Condition\n", out);
            dump_node(node->first, out, depth + 2);
            indent(out, depth + 1);
            fputs("Body\n", out);
            dump_node(node->second, out, depth + 2);
        } else if (node->kind == AST_FOR_STATEMENT) {
            indent(out, depth + 1);
            fputs("Initializer\n", out);
            dump_node(node->first, out, depth + 2);
            indent(out, depth + 1);
            fputs("Condition\n", out);
            dump_node(node->second, out, depth + 2);
            indent(out, depth + 1);
            fputs("Update\n", out);
            dump_node(node->third, out, depth + 2);
            indent(out, depth + 1);
            fputs("Body\n", out);
            dump_node(node->children, out, depth + 2);
        } else if (node->kind == AST_VARIABLE_DECLARATION ||
                   node->kind == AST_EXPRESSION_STATEMENT ||
                   node->kind == AST_RETURN_STATEMENT ||
                   node->kind == AST_UNARY ||
                   node->kind == AST_GROUP) {
            dump_node(node->first, out, depth + 1);
        } else if (node->kind == AST_ASSIGNMENT ||
                   node->kind == AST_BINARY) {
            dump_node(node->first, out, depth + 1);
            dump_node(node->second, out, depth + 1);
        } else if (node->kind == AST_CALL) {
            dump_node(node->children, out, depth + 1);
        }
    }
}

void ast_dump(const AstNode *node, FILE *output) {
    if (node != NULL && output != NULL) {
        dump_node(node, output, 0);
    }
}

void ast_free(AstNode *node) {
    while (node != NULL) {
        AstNode *next = node->next;

        ast_free(node->first);
        ast_free(node->second);
        ast_free(node->third);
        ast_free(node->children);

        free(node->name);
        free(node);

        node = next;
    }
}
