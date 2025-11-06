#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

enum token_kind { NUMBER, PLUS, MINUS, STAR, SLASH, PERCENT, LPAREN, RPAREN, END_OF_FILE };
enum node_type { NODE_NUMBER, NODE_UNARY, NODE_BINARY };

struct token {
    enum token_kind kind;
    uint16_t value;
    size_t start, end;
};

struct ast_node {
    enum node_type type;
    size_t start, end;

    union {
        struct {
            int value;
        } number;

        struct {
            char op;
            struct ast_node *child;
        } unary;

        struct {
            char op;
            struct ast_node *left;
            struct ast_node *right;
        } binary;
    } data;
};

struct lexer {
    char ch;
    size_t cursor;

    struct token *tokens;
    size_t capacity;
    size_t size;
};

void free_ast_node(struct ast_node *node);

void print_token(const struct token *tok);
struct token create_token(enum token_kind kind, uint16_t value, size_t start, size_t end);

void init_lexer(struct lexer *lex);
void free_lexer(struct lexer *lex);
void tokenize(struct lexer *lex, const char *source);
void parse_number(struct lexer *lex, const char *source);
void append_token(struct lexer *lex, struct token tok);

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

void free_lexer(struct lexer *lex)
{
    if (!lex) {
        return;
    }

    free(lex->tokens);
    lex->tokens = NULL;
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
        printf("Kind = %d, Value = %d, Start = %lu, End = %lu\n", tok->kind, tok->value, tok->start,
               tok->end);
    } break;

    default: {
        printf("Kind = %d, Value = %c, Start = %lu, End = %lu\n", tok->kind, tok->value, tok->start,
               tok->end);
    }
    }
}

struct token create_token(enum token_kind kind, uint16_t value, size_t start, size_t end)
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

void parse_number(struct lexer *lex, const char *source)
{
    size_t source_len = strlen(source);
    size_t start = lex->cursor;

    while (lex->cursor < source_len && isdigit(source[lex->cursor]) &&
           !isspace(source[lex->cursor])) {
        lex->cursor += 1;
    }

    size_t digits_len = lex->cursor - start;

    if (!digits_len) {
        (void)fprintf(stderr, "Go download more ram");
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
    const int base = 10;
    unsigned long val = strtoul(digits, &end, base);

    if (errno == ERANGE || val > UINT16_MAX || *end != '\0') {
        (void)fprintf(stderr, "Invalid or out of range number: %s", digits);
        free(digits);
        exit(EXIT_FAILURE);
    }

    append_token(lex, create_token(NUMBER, (uint16_t)val, start, start + digits_len - 1));
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

        switch (character) {
        case '-': {
            append_token(lex, create_token(MINUS, character, cursor, cursor));
        } break;

        case '+': {
            append_token(lex, create_token(PLUS, character, cursor, cursor));
        } break;

        case '*': {
            append_token(lex, create_token(STAR, character, cursor, cursor));
        } break;

        case '/': {
            append_token(lex, create_token(SLASH, character, cursor, cursor));
        } break;

        case '%': {
            append_token(lex, create_token(PERCENT, character, cursor, cursor));
        } break;

        case '(': {
            append_token(lex, create_token(LPAREN, character, cursor, cursor));
        } break;

        case ')': {
            append_token(lex, create_token(RPAREN, character, cursor, cursor));
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

    append_token(lex, create_token(END_OF_FILE, '\0', lex->cursor, lex->cursor));
}

int main(void)
{
    const char *source = "12 + 34 - 5 * (6 / 7) % 8";
    struct lexer lex = { 0 };
    init_lexer(&lex);

    tokenize(&lex, source);

    for (size_t i = 0; i < lex.size; ++i) {
        print_token(&lex.tokens[i]);
    }

    return 0;
}
