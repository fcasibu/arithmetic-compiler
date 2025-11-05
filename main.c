#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum token_kind { NUMBER, PLUS, MINUS, STAR, SLASH, PERCENT, LPAREN, RPAREN };
enum node_type { NODE_NUMBER, NODE_UNARY, NODE_BINARY };

struct token {
    enum token_kind kind;
    uint8_t value;
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

void print_token(const struct token *tok);
struct token create_token(enum token_kind kind, uint8_t value);

void init_lexer(struct lexer *lex);
void tokenize(struct lexer *lex, const char *source);
void append_token(struct lexer *lex, struct token tok);

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
    printf("Kind = %d, value = %c\n", tok->kind, tok->value);
}

struct token create_token(enum token_kind kind, uint8_t value)
{
    return (struct token){ .kind = kind, .value = value };
}

void init_lexer(struct lexer *lex)
{
    lex->cursor = 0;

    const size_t capacity = 10;
    lex->capacity = capacity;
    lex->size = 0;
    lex->tokens = malloc(capacity * sizeof(struct token));

    if (!lex->tokens) {
        (void)fprintf(stderr, "Go download more ram");
        exit(EXIT_FAILURE);
    }
}

void tokenize(struct lexer *lex, const char *source)
{
    size_t source_len = strlen(source);

    while (lex->cursor < source_len) {
        char character = source[lex->cursor];

        switch (character) {
        case '-': {
            append_token(lex, create_token(MINUS, character));
        } break;

        case '+': {
            append_token(lex, create_token(PLUS, character));
        } break;

        case '*': {
            append_token(lex, create_token(STAR, character));
        } break;

        case '/': {
            append_token(lex, create_token(SLASH, character));
        } break;

        case '%': {
            append_token(lex, create_token(PERCENT, character));
        } break;

        case '(': {
            append_token(lex, create_token(LPAREN, character));
        } break;

        case ')': {
            append_token(lex, create_token(RPAREN, character));
        } break;

        case ' ':
            break;

        default: {
            if (isdigit(character)) {
                append_token(lex, create_token(NUMBER, character));
            } else {
                (void)fprintf(stderr, "Unknown token '%c' at position %zu\n", character,
                              lex->cursor);
                exit(EXIT_FAILURE);
            }
        };
        }

        lex->cursor += 1;
    }
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
