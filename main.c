#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

enum token_kind { NUMBER, PLUS, MINUS, STAR, SLASH, PERCENT, CARET, LPAREN, RPAREN, END_OF_FILE };
enum node_type { NODE_NUMBER, NODE_UNARY, NODE_BINARY };

union token_value {
    uint8_t value;
    double number_value;
};

struct token {
    enum token_kind kind;
    union token_value value;
    size_t start, end;
};

union node_data {
    struct {
        double value;
    } number;

    struct {
        uint8_t op;
        struct ast_node *child;
    } unary;

    struct {
        uint8_t op;
        struct ast_node *left;
        struct ast_node *right;
    } binary;
};

struct ast_node {
    enum node_type type;
    size_t start, end;

    union node_data data;
};

struct lexer {
    char ch;
    size_t cursor;

    struct token *tokens;
    size_t capacity;
    size_t size;
};

struct parser {
    struct token *tokens;
    size_t size;

    size_t current_index;
};

double eval_ast(struct ast_node *root);
struct ast_node *parse(struct lexer *lex);
struct ast_node *parse_expression(struct parser *parser, uint8_t binding_power);
struct ast_node *parse_prefix(struct parser *parser, const struct token *token);
uint8_t get_left_binding_power(enum token_kind kind);
uint8_t get_right_binding_power(enum token_kind kind);
struct token get_next_token(struct parser *parser);
struct ast_node *create_ast_node(enum node_type type, union node_data data, size_t start,
                                 size_t end);
void free_ast_node(struct ast_node *node);
char *get_token_kind_string(enum token_kind kind);
void print_ast(struct ast_node *node);

void print_token(const struct token *tok);
struct token create_token(enum token_kind kind, union token_value value, size_t start, size_t end);

void init_lexer(struct lexer *lex);
void free_tokens(struct token *tokens);
void tokenize(struct lexer *lex, const char *source);
bool is_part_of_number(char character);
void parse_number(struct lexer *lex, const char *source);
void append_token(struct lexer *lex, struct token tok);

double eval_ast(struct ast_node *root)
{
    switch (root->type) {
    case NODE_NUMBER: {
        return root->data.number.value;
    }

    case NODE_UNARY: {
        switch (root->data.unary.op) {
        case MINUS:
            return -eval_ast(root->data.unary.child);
        default: {
            (void)fprintf(stderr, "Unknown unary operator");
            exit(EXIT_FAILURE);
        }
        }
    }

    case NODE_BINARY: {
        double lhs = eval_ast(root->data.binary.left);
        double rhs = eval_ast(root->data.binary.right);

        switch (root->data.binary.op) {
        case PLUS:
            return lhs + rhs;
        case MINUS:
            return lhs - rhs;
        case SLASH:
            return lhs / rhs;
        case STAR:
            return lhs * rhs;
        case CARET:
            return pow(lhs, rhs);
        case PERCENT:
            return fmod(lhs, rhs);
        default: {
            (void)fprintf(stderr, "Unknown binary operator");
            exit(EXIT_FAILURE);
        }
        }
    }
    }
}

char *get_token_kind_string(enum token_kind kind)
{
    switch (kind) {
    case MINUS:
        return "-";
    case PLUS:
        return "+";
    case SLASH:
        return "/";
    case STAR:
        return "*";
    case CARET:
        return "expt";
    case PERCENT:
        return "mod";
    default:
        return "?";
    }
}

void print_ast(struct ast_node *node)
{
    if (!node) {
        return;
    }

    switch (node->type) {
    case NODE_NUMBER: {
        printf("%g", node->data.number.value);
    } break;

    case NODE_UNARY: {
        printf("(%s ", get_token_kind_string(node->data.unary.op));
        print_ast(node->data.unary.child);
        printf(")");
    } break;

    case NODE_BINARY: {
        printf("(%s ", get_token_kind_string(node->data.binary.op));
        print_ast(node->data.binary.left);
        printf(" ");
        print_ast(node->data.binary.right);
        printf(")");
    } break;
    }
}

uint8_t get_left_binding_power(enum token_kind kind)
{
    switch (kind) {
    case PLUS:
    case MINUS:
        return 1;
    case STAR:
    case SLASH:
    case PERCENT:
        return 2;
    case CARET:
        return 4;
    default:
        return 0;
    }
}

uint8_t get_right_binding_power(enum token_kind kind)
{
    switch (kind) {
    case PLUS:
    case MINUS:
        return 1;
    case STAR:
    case SLASH:
    case PERCENT:
        return 2;
    case CARET:
        return 3;
    default:
        return 0;
    }
}

struct ast_node *create_ast_node(enum node_type type, union node_data data, size_t start,
                                 size_t end)
{
    struct ast_node *node = (struct ast_node *)malloc(sizeof(struct ast_node));
    node->type = type;
    node->start = start;
    node->end = end;
    node->data = data;

    return node;
}

struct token get_next_token(struct parser *parser)
{
    assert(parser->current_index < parser->size);

    return parser->tokens[parser->current_index++];
}

struct ast_node *parse(struct lexer *lex)
{
    struct parser parser =
        (struct parser){ .tokens = lex->tokens, .size = lex->size, .current_index = 0 };

    return parse_expression(&parser, 0);
}

struct ast_node *parse_expression(struct parser *parser, uint8_t binding_power)
{
    if (parser->current_index >= parser->size) {
        return NULL;
    }

    struct token tok = get_next_token(parser);
    struct ast_node *lhs = parse_prefix(parser, &tok);

    while (true) {
        if (parser->current_index >= parser->size) {
            break;
        }

        struct token next = parser->tokens[parser->current_index];

        uint8_t left_binding_power = get_left_binding_power(next.kind);
        if (left_binding_power <= binding_power) {
            break;
        }

        struct token operator = get_next_token(parser);
        uint8_t right_binding_power = get_right_binding_power(operator.kind);

        struct ast_node *rhs = parse_expression(parser, right_binding_power);
        lhs = create_ast_node(NODE_BINARY,
                              (union node_data){ .binary.left = lhs,
                                                 .binary.right = rhs,
                                                 .binary.op = operator.kind },
                              lhs->start, rhs->end);
    }

    return lhs;
}

struct ast_node *parse_prefix(struct parser *parser, const struct token *token)
{
    if (token->kind == NUMBER) {
        return create_ast_node(NODE_NUMBER,
                               (union node_data){ .number.value = token->value.number_value },
                               token->start, token->end);
    }

    if (token->kind == MINUS) {
        struct ast_node *rhs = parse_expression(parser, get_left_binding_power(MINUS) + 1);
        return create_ast_node(NODE_UNARY,
                               (union node_data){ .unary.child = rhs, .unary.op = token->kind },
                               token->start, token->end);
    }

    if (token->kind == LPAREN) {
        struct ast_node *expr = parse_expression(parser, 0);
        struct token tok = get_next_token(parser);
        assert(tok.kind == RPAREN);

        return expr;
    }

    (void)fprintf(stderr, "Invalid prefix token");
    exit(EXIT_FAILURE);
}

void free_ast_node(struct ast_node *node)
{
    if (!node) {
        return;
    }

    switch (node->type) {
    case NODE_NUMBER:
        break;
    case NODE_UNARY: {
        free_ast_node(node->data.unary.child);
    } break;
    case NODE_BINARY: {
        free_ast_node(node->data.binary.left);
        free_ast_node(node->data.binary.right);
    } break;
    }

    free(node);
}

void free_tokens(struct token *tokens)
{
    if (!tokens) {
        return;
    }

    free(tokens);
}

void append_token(struct lexer *lex, struct token tok)
{
    if (lex->size >= lex->capacity) {
        lex->capacity *= 2;
        struct token *new_tokens = realloc(lex->tokens, lex->capacity * sizeof(struct token));

        if (!new_tokens) {
            (void)fprintf(stderr, "Go download more ram");
            exit(EXIT_FAILURE);
        }

        lex->tokens = new_tokens;
    }

    lex->tokens[lex->size++] = tok;
}

void print_token(const struct token *tok)
{
    switch (tok->kind) {
    case NUMBER: {
        printf("Kind = %d, Value = %g, Start = %lu, End = %lu\n", tok->kind,
               tok->value.number_value, tok->start, tok->end);
    } break;

    default: {
        if (tok->kind != END_OF_FILE) {
            printf("Kind = %d, Value = %c, Start = %lu, End = %lu\n", tok->kind, tok->value.value,
                   tok->start, tok->end);
        }
    }
    }
}

struct token create_token(enum token_kind kind, union token_value value, size_t start, size_t end)
{
    return (struct token){ .kind = kind, .value = value, .start = start, .end = end };
}

void init_lexer(struct lexer *lex)
{
    lex->cursor = 0;

    const size_t capacity = 10;
    lex->capacity = capacity;
    lex->size = 0;
    lex->tokens = (struct token *)malloc(capacity * sizeof(struct token));

    if (!lex->tokens) {
        (void)fprintf(stderr, "Go download more ram");
        exit(EXIT_FAILURE);
    }
}

bool is_part_of_number(char character)
{
    return isdigit(character) || character == '.' || character == 'e' || character == 'E' ||
           character == '+' || character == '-';
}

void parse_number(struct lexer *lex, const char *source)
{
    size_t source_len = strlen(source);
    size_t start = lex->cursor;

    while (lex->cursor < source_len && is_part_of_number(source[lex->cursor]) &&
           !isspace(source[lex->cursor])) {
        lex->cursor += 1;
    }

    size_t digits_len = lex->cursor - start;

    if (!digits_len) {
        (void)fprintf(stderr, "Invalid character");
        exit(EXIT_FAILURE);
    }

    char *digits = (char *)malloc(digits_len + 1);
    if (!digits) {
        (void)fprintf(stderr, "Go download more ram");
        exit(EXIT_FAILURE);
    }

    memcpy(digits, source + start, digits_len);
    digits[digits_len] = '\0';

    errno = 0;
    char *end = NULL;
    double val = strtod(digits, &end);

    if (errno == ERANGE || *end != '\0') {
        (void)fprintf(stderr, "Invalid or out of range number: %s", digits);
        free(digits);
        exit(EXIT_FAILURE);
    }

    append_token(lex, create_token(NUMBER, (union token_value){ .number_value = val }, start,
                                   start + digits_len - 1));
    free(digits);
}

void tokenize(struct lexer *lex, const char *source)
{
    size_t source_len = strlen(source);

    while (lex->cursor < source_len) {
        size_t cursor = lex->cursor;
        char character = source[cursor];

        if (isspace(character)) {
            lex->cursor += 1;
            continue;
        }

        union token_value value = (union token_value){ .value = character };

        switch (character) {
        case '-': {
            if (isdigit(source[cursor + 1])) {
                parse_number(lex, source);
                continue;
            }

            append_token(lex, create_token(MINUS, value, cursor, cursor));
        } break;

        case '+': {
            append_token(lex, create_token(PLUS, value, cursor, cursor));
        } break;

        case '*': {
            append_token(lex, create_token(STAR, value, cursor, cursor));
        } break;

        case '/': {
            append_token(lex, create_token(SLASH, value, cursor, cursor));
        } break;

        case '%': {
            append_token(lex, create_token(PERCENT, value, cursor, cursor));
        } break;

        case '^': {
            append_token(lex, create_token(CARET, value, cursor, cursor));
        } break;

        case '(': {
            append_token(lex, create_token(LPAREN, value, cursor, cursor));
        } break;

        case ')': {
            append_token(lex, create_token(RPAREN, value, cursor, cursor));
        } break;

        default: {
            if (isdigit(character)) {
                parse_number(lex, source);
                continue;
            }

            (void)fprintf(stderr, "Unknown token '%c' at position %zu\n", character, lex->cursor);
            exit(EXIT_FAILURE);
        };
        }

        lex->cursor += 1;
    }

    append_token(lex, create_token(END_OF_FILE, (union token_value){ .value = '\0' }, lex->cursor,
                                   lex->cursor));
}

int main(void)
{
    const char *source = "(-3.24121 + 4) * 1e+20 / (1 - 5) ^ 2 ^ 3 % 7 - 9 * (8 + 6 / 3)";
    struct lexer lex = { 0 };
    init_lexer(&lex);

    tokenize(&lex, source);
    struct ast_node *root = parse(&lex);

    print_ast(root);

    printf("\nResult: %.15g", eval_ast(root));

    free_ast_node(root);
    free_tokens(lex.tokens);

    return 0;
}
