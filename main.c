#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <getopt.h>

#define UNARY_DEFAULT 10
#define DEFAULT_CAPACITY 32
#define MAX_STACK_SIZE 255

enum node_type { NODE_NUMBER, NODE_UNARY, NODE_BINARY };
enum ast_print_type { AST_S_EXPR = 1, AST_JSON = 2 };
enum token_kind { NUMBER, PLUS, MINUS, STAR, SLASH, PERCENT, CARET, LPAREN, RPAREN, END_OF_FILE };
// clang-format off
enum opcode {
    OP_CONSTANT, OP_ADD, OP_SUBTRACT, OP_MULTIPLY, OP_DIVIDE, 
    OP_MODULO, OP_POWER,  OP_NEGATE, OP_PLUS, OP_HALT
};
// clang-format on

struct bytecode {
    enum opcode code;
    size_t const_index;
};

struct chunk {
    struct bytecode *code;
    size_t code_capacity;
    size_t code_size;

    double *constants;
    size_t const_capacity;
    size_t const_size;
};

struct vm {
    struct chunk *chunks;
    size_t ip;
    double stack[MAX_STACK_SIZE];
    size_t top;
};

union token_value {
    char value;
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
        enum token_kind op;
        struct ast_node *child;
    } unary;

    struct {
        enum token_kind op;
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

struct cli_options {
    bool show_help;
    enum ast_print_type show_ast;
    char *expression;
};

void push(struct vm *stack_vm, double value);
double pop(struct vm *stack_vm);
double run_vm(struct vm *stack_vm);

enum opcode get_opcode_from_token_kind(enum token_kind kind);
size_t add_constant(struct chunk *chunks, double value);
void compile_ast_to_bytecode(struct chunk *chunks, struct ast_node *node);
void emit_bytecode(struct chunk *chunks, enum opcode code, size_t const_index);
void init_chunks(struct chunk *chunks);
void free_chunks(struct chunk *chunks);

void process_expression(struct cli_options *opts);
void print_help(void);
void parse_args(int argc, char **argv, struct cli_options *opts);

double eval_ast(const struct ast_node *root);
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
void print_indent(size_t level);
void print_ast_json(const struct ast_node *node, size_t level);
void print_ast(const struct ast_node *node);

struct token create_token(enum token_kind kind, union token_value value, size_t start, size_t end);

void init_lexer(struct lexer *lex);
void free_tokens(struct token *tokens);
void tokenize(struct lexer *lex, const char *source);
bool is_part_of_number(char character);
void parse_number(struct lexer *lex, const char *source);
void append_token(struct lexer *lex, struct token tok);

void push(struct vm *stack_vm, double value)
{
    if (stack_vm->top >= MAX_STACK_SIZE) {
        (void)fprintf(stderr, "Stack overflow\n");
        exit(EXIT_FAILURE);
    }

    stack_vm->stack[stack_vm->top++] = value;
}

double pop(struct vm *stack_vm)
{
    if (stack_vm->top <= 0) {
        (void)fprintf(stderr, "Stack undeflow\n");
        exit(EXIT_FAILURE);
    }

    return stack_vm->stack[--stack_vm->top];
}

double run_vm(struct vm *stack_vm)
{
    while (true) {
        struct bytecode instruction = stack_vm->chunks->code[stack_vm->ip];

        switch (instruction.code) {
        case OP_CONSTANT: {
            double value = stack_vm->chunks->constants[instruction.const_index];

            push(stack_vm, value);
        } break;

        case OP_NEGATE: {
            double value = pop(stack_vm);
            push(stack_vm, -value);
        } break;

        case OP_ADD: {
            double rhs = pop(stack_vm);
            double lhs = pop(stack_vm);

            push(stack_vm, lhs + rhs);
        } break;
        case OP_SUBTRACT: {
            double rhs = pop(stack_vm);
            double lhs = pop(stack_vm);

            push(stack_vm, lhs - rhs);
        } break;
        case OP_MULTIPLY: {
            double rhs = pop(stack_vm);
            double lhs = pop(stack_vm);

            push(stack_vm, lhs * rhs);
        } break;
        case OP_DIVIDE: {
            double rhs = pop(stack_vm);
            double lhs = pop(stack_vm);

            if (rhs == 0.0) {
                (void)fprintf(stderr, "Division by zero\n");
                exit(EXIT_FAILURE);
            }

            push(stack_vm, lhs / rhs);
        } break;
        case OP_MODULO: {
            double rhs = pop(stack_vm);
            double lhs = pop(stack_vm);

            if (rhs == 0.0) {
                (void)fprintf(stderr, "Division by zero\n");
                exit(EXIT_FAILURE);
            }

            push(stack_vm, fmod(lhs, rhs));
        } break;
        case OP_POWER: {
            double rhs = pop(stack_vm);
            double lhs = pop(stack_vm);

            push(stack_vm, pow(lhs, rhs));
        } break;

        case OP_HALT: {
            return pop(stack_vm);
        }

        default: {
            (void)fprintf(stderr, "Unknown instruction code");
            exit(EXIT_FAILURE);
        }
        }

        stack_vm->ip += 1;
    }
}

void free_chunks(struct chunk *chunks)
{
    if (!chunks) {
        return;
    }

    free(chunks->code);
    chunks->code = NULL;

    free(chunks->constants);
    chunks->constants = NULL;
}

void init_chunks(struct chunk *chunks)
{
    chunks->code_capacity = DEFAULT_CAPACITY;
    chunks->code_size = 0;
    chunks->code = malloc(chunks->code_capacity * sizeof(*chunks->code));

    if (!chunks->code) {
        (void)fprintf(stderr, "Go download more ram\n");
        exit(EXIT_FAILURE);
    }

    chunks->const_capacity = DEFAULT_CAPACITY;
    chunks->const_size = 0;
    chunks->constants = malloc(chunks->const_capacity * sizeof(*chunks->constants));

    if (!chunks->constants) {
        (void)fprintf(stderr, "Go download more ram\n");
        exit(EXIT_FAILURE);
    }
}

enum opcode get_opcode_from_token_kind(enum token_kind kind)
{
    switch (kind) {
    case NUMBER:
        return OP_CONSTANT;
    case PLUS:
        return OP_ADD;
    case MINUS:
        return OP_SUBTRACT;
    case STAR:
        return OP_MULTIPLY;
    case SLASH:
        return OP_DIVIDE;
    case PERCENT:
        return OP_MODULO;
    case CARET:
        return OP_POWER;
    default: {
        return OP_HALT;
    }
    }
}

size_t add_constant(struct chunk *chunks, double value)
{
    size_t index = chunks->const_size;
    if (chunks->const_size >= chunks->const_capacity) {
        chunks->const_capacity *= 2;

        double *new_constants =
            realloc(chunks->constants, chunks->const_capacity * sizeof(*chunks->constants));

        if (!new_constants) {
            (void)fprintf(stderr, "Go download more ram\n");
            exit(EXIT_FAILURE);
        }

        chunks->constants = new_constants;
    }

    chunks->constants[chunks->const_size++] = value;

    return index;
}

void compile_ast_to_bytecode(struct chunk *chunks, struct ast_node *node)
{
    if (!node) {
        return;
    }

    switch (node->type) {
    case NODE_NUMBER: {
        size_t const_index = add_constant(chunks, node->data.number.value);
        emit_bytecode(chunks, OP_CONSTANT, const_index);
    } break;

    case NODE_UNARY: {
        compile_ast_to_bytecode(chunks, node->data.unary.child);

        switch (node->data.unary.op) {
        case MINUS: {
            emit_bytecode(chunks, OP_NEGATE, 0);
        } break;

        case PLUS:
            break;

        default: {
            (void)fprintf(stderr, "No such unary operator");
            exit(EXIT_FAILURE);
        }
        };
    } break;

    case NODE_BINARY: {
        compile_ast_to_bytecode(chunks, node->data.binary.left);
        compile_ast_to_bytecode(chunks, node->data.binary.right);

        emit_bytecode(chunks, get_opcode_from_token_kind(node->data.binary.op), 0);
    } break;
    }
}

void emit_bytecode(struct chunk *chunks, enum opcode code, size_t const_index)
{
    if (chunks->code_size >= chunks->code_capacity) {
        chunks->code_capacity *= 2;

        struct bytecode *new_code =
            realloc(chunks->code, chunks->code_capacity * sizeof(*chunks->code));

        if (!new_code) {
            (void)fprintf(stderr, "Go download more ram\n");
            exit(EXIT_FAILURE);
        }

        chunks->code = new_code;
    }

    chunks->code[chunks->code_size++] =
        (struct bytecode){ .code = code, .const_index = const_index };
}

double eval_ast(const struct ast_node *root)
{
    switch (root->type) {
    case NODE_NUMBER: {
        return root->data.number.value;
    }

    case NODE_UNARY: {
        switch (root->data.unary.op) {
        case MINUS:
            return -eval_ast(root->data.unary.child);
        case PLUS:
            return eval_ast(root->data.unary.child);
        default: {
            (void)fprintf(stderr, "Unknown unary operator\n");
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
        case SLASH: {
            if (rhs == 0.0) {
                (void)fprintf(stderr, "Division by zero\n");
                exit(EXIT_FAILURE);
            }

            return lhs / rhs;
        }
        case STAR:
            return lhs * rhs;
        case CARET:
            return pow(lhs, rhs);
        case PERCENT: {
            if (rhs == 0.0) {
                (void)fprintf(stderr, "Division by zero\n");
                exit(EXIT_FAILURE);
            }

            return fmod(lhs, rhs);
        }
        default: {
            (void)fprintf(stderr, "Unknown binary operator\n");
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

void print_indent(size_t level)
{
    for (size_t i = 0; i < level; i++) {
        printf(" ");
    }
}

void print_ast_json(const struct ast_node *node, size_t level)
{
    if (!node) {
        printf("null");
        return;
    }

    size_t indent = level * 2;

    switch (node->type) {
    case NODE_NUMBER: {
        printf("{\n");
        print_indent(indent + 2);
        printf("\"type\": \"number\",\n");
        print_indent(indent + 2);
        printf("\"value\": %g,\n", node->data.number.value);
        print_indent(indent + 2);
        printf("\"start\": %zu,\n", node->start);
        print_indent(indent + 2);
        printf("\"end\": %zu\n", node->end);
        print_indent(indent);
        printf("}");
    } break;

    case NODE_UNARY: {
        printf("{\n");
        print_indent(indent + 2);
        printf("\"type\": \"unary\",\n");
        print_indent(indent + 2);
        printf("\"op\": \"%s\",\n", get_token_kind_string(node->data.unary.op));
        print_indent(indent + 2);
        printf("\"start\": %zu,\n", node->start);
        print_indent(indent + 2);
        printf("\"end\": %zu,\n", node->end);
        print_indent(indent + 2);
        printf("\"child\": ");
        print_ast_json(node->data.unary.child, level + 1);
        printf("\n");
        print_indent(indent);
        printf("}");
    } break;

    case NODE_BINARY: {
        printf("{\n");
        print_indent(indent + 2);
        printf("\"type\": \"binary\",\n");
        print_indent(indent + 2);
        printf("\"op\": \"%s\",\n", get_token_kind_string(node->data.binary.op));
        print_indent(indent + 2);
        printf("\"start\": %zu,\n", node->start);
        print_indent(indent + 2);
        printf("\"end\": %zu,\n", node->end);
        print_indent(indent + 2);
        printf("\"left\": ");
        print_ast_json(node->data.binary.left, level + 1);
        printf(",\n");
        print_indent(indent + 2);
        printf("\"right\": ");
        print_ast_json(node->data.binary.right, level + 1);
        printf("\n");
        print_indent(indent);
        printf("}");
    } break;
    }
}

void print_ast(const struct ast_node *node)
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
    struct ast_node *node = malloc(sizeof(struct ast_node));
    if (!node) {
        (void)fprintf(stderr, "Go download more ram\n");
        exit(EXIT_FAILURE);
    }

    node->type = type;
    node->start = start;
    node->end = end;
    node->data = data;

    return node;
}

struct token get_next_token(struct parser *parser)
{
    if (parser->tokens[parser->current_index].kind == END_OF_FILE) {
        (void)fprintf(stderr, "EOF\n");
        exit(EXIT_FAILURE);
    }

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
    if (parser->tokens[parser->current_index].kind == END_OF_FILE) {
        return NULL;
    }

    struct token tok = get_next_token(parser);
    struct ast_node *lhs = parse_prefix(parser, &tok);

    if (!lhs) {
        (void)fprintf(stderr, "Expected left hand side\n");
        exit(EXIT_FAILURE);
    }

    while (parser->tokens[parser->current_index].kind != END_OF_FILE) {
        struct token next = parser->tokens[parser->current_index];

        uint8_t left_binding_power = get_left_binding_power(next.kind);
        if (left_binding_power <= binding_power) {
            break;
        }

        struct token operator = get_next_token(parser);
        uint8_t right_binding_power = get_right_binding_power(operator.kind);

        struct ast_node *rhs = parse_expression(parser, right_binding_power);

        if (!rhs) {
            (void)fprintf(stderr, "Expected right hand side\n");
            exit(EXIT_FAILURE);
        }

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
        struct ast_node *rhs = parse_expression(parser, UNARY_DEFAULT);

        if (!rhs) {
            (void)fprintf(stderr, "Expected right hand side\n");
            exit(EXIT_FAILURE);
        }

        return create_ast_node(NODE_UNARY,
                               (union node_data){ .unary.child = rhs, .unary.op = token->kind },
                               token->start, token->end);
    }

    if (token->kind == PLUS) {
        struct ast_node *rhs = parse_expression(parser, UNARY_DEFAULT);

        if (!rhs) {
            (void)fprintf(stderr, "Expected right hand side\n");
            exit(EXIT_FAILURE);
        }

        return create_ast_node(NODE_UNARY,
                               (union node_data){ .unary.child = rhs, .unary.op = token->kind },
                               token->start, token->end);
    }

    if (token->kind == LPAREN) {
        struct ast_node *expr = parse_expression(parser, 0);

        if (!expr) {
            (void)fprintf(stderr, "Expected expression\n");
            exit(EXIT_FAILURE);
        }

        struct token tok = get_next_token(parser);

        if (tok.kind != RPAREN) {
            (void)fprintf(stderr, "Expected ')' at position %zu\n", tok.start);
            exit(EXIT_FAILURE);
        }

        return expr;
    }

    (void)fprintf(stderr, "Invalid prefix token\n");
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
        struct token *new_tokens = realloc(lex->tokens, lex->capacity * sizeof(*lex->tokens));

        if (!new_tokens) {
            (void)fprintf(stderr, "Go download more ram\n");
            exit(EXIT_FAILURE);
        }

        lex->tokens = new_tokens;
    }

    lex->tokens[lex->size++] = tok;
}

struct token create_token(enum token_kind kind, union token_value value, size_t start, size_t end)
{
    return (struct token){ .kind = kind, .value = value, .start = start, .end = end };
}

void init_lexer(struct lexer *lex)
{
    lex->cursor = 0;
    lex->capacity = DEFAULT_CAPACITY;
    lex->size = 0;
    lex->tokens = malloc(lex->capacity * sizeof(*lex->tokens));

    if (!lex->tokens) {
        (void)fprintf(stderr, "Go download more ram\n");
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
        (void)fprintf(stderr, "Invalid character at position %zu", lex->cursor);
        exit(EXIT_FAILURE);
    }

    char *digits = malloc(digits_len + 1);
    if (!digits) {
        (void)fprintf(stderr, "Go download more ram\n");
        exit(EXIT_FAILURE);
    }

    memcpy(digits, source + start, digits_len);
    digits[digits_len] = '\0';

    errno = 0;
    char *end = NULL;
    double val = strtod(digits, &end);

    if (errno == ERANGE || *end != '\0') {
        (void)fprintf(stderr, "Invalid or out of range number: %s\n", digits);
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
            if (cursor + 1 < source_len && isdigit(source[cursor + 1])) {
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

void print_help(void)
{
    printf("Usage:\n");
    printf("  ./main [OPTIONS] [EXPRESSION]\n\n");

    printf("Options:\n");
    printf(
        "  -e, --eval EXPRESSION       Evaluate expression directly (default if expression provided)\n");
    printf("  -a, --ast [FORMAT]          Show AST visualization\n");
    printf("                               FORMAT can be 'json' (default is S-expression)\n");
    printf("  -h, --help                  Show this help message\n\n");
}

void parse_args(int argc, char **argv, struct cli_options *opts)
{
    static struct option long_options[] = {
        { "help", no_argument, 0, 'h' },
        { "eval", required_argument, 0, 'e' },
        { "ast", optional_argument, 0, 'a' },
        { NULL, 0, NULL, 0 },
    };

    int opt = 0;

    while ((opt = getopt_long(argc, argv, "he:a::", long_options, NULL)) != -1) {
        switch (opt) {
        case 'h': {
            opts->show_help = true;
        } break;

        case 'e': {
            opts->expression = optarg;
        } break;

        case 'a': {
            // optarg is null for short option?? e.g. -a json

            if (!argv[optind]) {
                opts->show_ast = AST_S_EXPR;
            } else if (optind < argc && strcmp(argv[optind], "json") == 0) {
                opts->show_ast = AST_JSON;
            }
        } break;

        case '?':
        default:
            break;
        }
    }

    if (!opts->expression && optind < argc) {
        opts->expression = argv[optind];
    }
}

void process_expression(struct cli_options *opts)
{
    if (!opts->expression) {
        (void)fprintf(stderr, "Missinng expression");
        return;
    }

    struct lexer lex = { 0 };
    init_lexer(&lex);

    tokenize(&lex, opts->expression);
    struct ast_node *root = parse(&lex);

    if (!root) {
        (void)fprintf(stderr, "Error: Empty expression\n");
        free_tokens(lex.tokens);
        return;
    }

    if (opts->show_ast == AST_S_EXPR) {
        printf("AST: ");
        print_ast(root);
        printf("\n");
    }

    if (opts->show_ast == AST_JSON) {
        print_ast_json(root, 0);
        printf("\n");
    }

    struct chunk chunks = { 0 };
    init_chunks(&chunks);

    compile_ast_to_bytecode(&chunks, root);
    emit_bytecode(&chunks, OP_HALT, 0);

    struct vm stack_vm = { .ip = 0, .top = 0, .chunks = &chunks, .stack = { 0 } };
    double result = run_vm(&stack_vm);
    double eval_result = eval_ast(root);

    assert(result == eval_result);
    printf("VM Result: %.15g\n", result);
    printf("Eval Result: %.15g\n", eval_result);

    free_ast_node(root);
    free_tokens(lex.tokens);
    free_chunks(&chunks);
}

int main(int argc, char **argv)
{
    struct cli_options opts = { 0 };
    parse_args(argc, argv, &opts);

    if (opts.show_help) {
        print_help();
        return 0;
    }

    process_expression(&opts);

    return 0;
}
