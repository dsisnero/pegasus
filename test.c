#include <stdlib.h>
#include <string.h>

/* == Generated code == */
/* Generate: Token ID to item kind */
/* Generate: Item kind to string */
/* Generate: Item kind defines (for ==) */
/* Generate: Max terminal */
/* Generate: Max nonterminal */
/* Generate: Parser state table */
/* Generate: Parser action table */
/* Generate: lexer state table */
/* Generate: lexer final table */
/* Generate: Items */

struct pgs_item_s {
    long int left_id;
    size_t right_count;
};

#define PGS_MAX_TERMINAL -1
#define PGS_MAX_NONTERMINAL -1
long int lexer_state_table[25][256];
long int lexer_final_table[25];
long int parse_state_table[25][PGS_MAX_TERMINAL + 2];
long int parse_action_table[25][PGS_MAX_TERMINAL + 2 + PGS_MAX_NONTERMINAL + 1];
struct pgs_item_s items[10];

/* == General Definitions == */
#define PGS_MAX_ERROR_LENGTH 255

enum pgs_error_e {
    PGS_NONE = 0,
    PGS_MALLOC,
    PGS_BAD_CHARACTER,
    PGS_BAD_TOKEN,
    PGS_EOF_SHIFT
};

struct pgs_state_s {
    enum pgs_error_e error;
    char errbuff[PGS_MAX_ERROR_LENGTH];
};

typedef enum pgs_error_e pgs_error;
typedef struct pgs_state_s pgs_state;

void pgs_state_init(pgs_state* s);
void pgs_state_error(pgs_state* s, pgs_error, const char* message);

/* == General Code */

void pgs_state_init(pgs_state* s) {
    s->error = PGS_NONE;
    s->errbuff[0] = '\0';
}

void pgs_state_error(pgs_state* s, pgs_error e, const char* message) {
    s->error = e;
    strncpy(s->errbuff, message, PGS_MAX_ERROR_LENGTH);
}

/* == Lexing Definitions ==*/

struct pgs_token_s {
    long int terminal;
    size_t from;
    size_t to;
};

struct pgs_token_list_s {
    size_t capacity;
    size_t token_count;
    struct pgs_token_s* tokens;
};

typedef struct pgs_token_s pgs_token;
typedef struct pgs_token_list_s pgs_token_list;

pgs_error pgs_token_list_init(pgs_token_list* l);
pgs_error pgs_token_list_append(pgs_token_list* l, long int terminal, size_t from, size_t to);
pgs_error pgs_token_list_top_id(pgs_token_list* l);
void pgs_token_list_free(pgs_token_list* l);

pgs_error pgs_token_list_init(pgs_token_list* l) {
    l->capacity = 8;
    l->token_count = 0;
    l->tokens = malloc(sizeof(*(l->tokens)) * l->capacity);

    if(l->tokens == NULL) return PGS_MALLOC;
    return PGS_NONE;
}

pgs_error pgs_token_list_append(pgs_token_list* l, long int terminal, size_t from, size_t to) {
    if(l->capacity == l->token_count) {
        pgs_token* new_tokens = realloc(l->tokens, sizeof(*new_tokens) * l->capacity * 2);
        if(new_tokens == NULL) return PGS_MALLOC;
        l->capacity *= 2;
        l->tokens = new_tokens;
    }

    l->tokens[l->token_count].terminal = terminal;
    l->tokens[l->token_count].from = from;
    l->tokens[l->token_count].to = to;
    l->token_count++;

    return PGS_NONE;
}

pgs_error pgs_token_list_top_id(pgs_token_list* l) {
    if(l->token_count) return l->tokens[l->token_count - 1].terminal;
    return 0;
}

void pgs_token_list_free(pgs_token_list* l) {
    free(l->tokens);
}

pgs_error pgs_do_lex(pgs_state* s, pgs_token_list* list, const char* source) {
    pgs_error error;
    size_t index = 0;
    long int final;
    long int last_final;
    long int last_final_index;
    long int last_start;
    long int state;
    size_t length = strlen(source);

    if((error = pgs_token_list_init(list))) return error;
    while(!error && index < length) {
        last_final = -1;
        last_final_index = -1;
        last_start = index;
        state = 1;

        while(index < length && state) {
            state = lexer_state_table[state][(unsigned int) source[index]];

            if((final = lexer_final_table[state])) {
                last_final = final;
                last_final_index = index;
            }

            if(state) index++;
        }

        if(last_final == -1) break;
        error = pgs_token_list_append(list, last_final, last_start, last_final_index);
    }

    if(error == PGS_MALLOC) {
        pgs_token_list_free(list);
    } else if (index != length) {
        pgs_state_error(s, PGS_BAD_CHARACTER, "Invalid character at position");
        pgs_token_list_free(list);
        return PGS_BAD_CHARACTER;
    }

    return PGS_NONE;
}

/* == Parsing Definitions == */

enum pgs_tree_variant_e {
    PGS_TREE_TERMINAL,
    PGS_TREE_NONTERMINAL
};

struct pgs_tree_terminal_s {
    pgs_token token;
};

struct pgs_tree_nonterminal_s {
    long int nonterminal;
    size_t child_count;
    struct pgs_tree_s** children;
};

struct pgs_tree_s {
    enum pgs_tree_variant_e variant;
    union {
        struct pgs_tree_terminal_s terminal;
        struct pgs_tree_nonterminal_s nonterminal;
    } tree_data;
};

struct pgs_parse_stack_element_s {
    struct pgs_tree_s* tree;
    long int state;
};

struct pgs_parse_stack_s {
    size_t capacity;
    size_t size;
    struct pgs_parse_stack_element_s* data;
};

typedef enum pgs_tree_variant_e pgs_tree_variant;
typedef struct pgs_tree_terminal_s pgs_tree_terminal;
typedef struct pgs_tree_nontermnal_s pgs_tree_nonterminal;
typedef struct pgs_tree_s pgs_tree;
typedef struct pgs_parse_stack_element_s pgs_parse_stack_element;
typedef struct pgs_parse_stack_s pgs_parse_stack;

pgs_tree* pgs_create_tree_nonterminal(long int nonterminal, size_t child_count);
pgs_tree* pgs_create_tree_terminal(pgs_token* t);
void pgs_free_tree_nonterminal(pgs_tree* tree);
void pgs_free_tree_terminal(pgs_tree* tree);
long int pgs_tree_table_index(pgs_tree* tree);
void pgs_free_tree(pgs_tree* tree);

pgs_error pgs_parse_stack_init(pgs_parse_stack* s);
pgs_error pgs_parse_stack_append(pgs_parse_stack* s, pgs_tree* tree, long int state);
pgs_error pgs_parse_stack_append_terminal(pgs_parse_stack* s, pgs_token* t);
pgs_error pgs_parse_stack_append_nonterminal(pgs_parse_stack* s, long int id, size_t count);
long int pgs_parse_stack_top_state(pgs_parse_stack* s);
pgs_tree* pgs_parse_stack_top_tree(pgs_parse_stack* s);
void pgs_parse_stack_free(pgs_parse_stack* s);

/* == Parsing Code == */

pgs_tree* pgs_create_tree_nonterminal(long int nonterminal, size_t child_count) {
    pgs_tree* tree = malloc(sizeof(*tree));
    pgs_tree** children = malloc(sizeof(*children) * child_count);

    if(tree == NULL || children == NULL) {
        free(tree);
        return NULL;
    }

    tree->variant = PGS_TREE_NONTERMINAL;
    tree->tree_data.nonterminal.nonterminal = nonterminal;
    tree->tree_data.nonterminal.child_count = child_count;
    tree->tree_data.nonterminal.children = children;

    return tree;
}

pgs_tree* pgs_create_tree_terminal(pgs_token* t) {
    pgs_tree* tree = malloc(sizeof(*tree));
    if(tree == NULL) return NULL;

    tree->tree_data.terminal.token = *t;

    return tree;
}

void pgs_free_tree_nonterminal(pgs_tree* t) {
    size_t i;
    for(i = 0; i < t->tree_data.nonterminal.child_count; i++) {
        pgs_free_tree(t->tree_data.nonterminal.children[i]);
    }
    free(t->tree_data.nonterminal.children);
    free(t);
}

void pgs_free_tree_terminal(pgs_tree* t) {
    free(t);
}

long int pgs_tree_table_index(pgs_tree* t) {
    switch(t->variant) {
        case PGS_TREE_TERMINAL:
            return t->tree_data.terminal.token.terminal;
        case PGS_TREE_NONTERMINAL: 
            return t->tree_data.nonterminal.nonterminal + 2 + PGS_MAX_TERMINAL;
    }
}

void pgs_free_tree(pgs_tree* t) {
    switch(t->variant) {
        case PGS_TREE_TERMINAL: pgs_free_tree_terminal(t); break;
        case PGS_TREE_NONTERMINAL: pgs_free_tree_nonterminal(t); break;
    }
}

pgs_error pgs_parse_stack_init(pgs_parse_stack* s) {
    s->capacity = 8;
    s->size = 1;
    s->data = malloc(sizeof(*(s->data)) * s->capacity);

    if(s->data == NULL) return PGS_MALLOC;
    s->data[0].tree = NULL;
    s->data[0].state = 1;

    return PGS_NONE;
}

pgs_error pgs_parse_stack_append(pgs_parse_stack* s, pgs_tree* tree, long int state) {
    if(s->capacity == s->size) {
        pgs_parse_stack_element* new_elements = realloc(s->data, sizeof(*new_elements) * s->capacity * 2);
        if(new_elements == NULL) return PGS_MALLOC;
        s->capacity *= 2;
        s->data = new_elements;
    }

    s->data[s->size].tree = tree;
    s->data[s->size].state = state;
    s->size++;

    return PGS_NONE;
}

pgs_error pgs_parse_stack_append_terminal(pgs_parse_stack* s, pgs_token* t) {
    pgs_error error;
    long int state;
    pgs_tree* tree = pgs_create_tree_terminal(t);
    if(tree == NULL) return PGS_MALLOC;
    state = parse_state_table[pgs_parse_stack_top_state(s)][t->terminal];
    error = pgs_parse_stack_append(s, tree, state);
    if(error) {
        pgs_free_tree_terminal(tree);
        return error;
    }
    return PGS_NONE;
}

pgs_error pgs_parse_stack_append_nonterminal(pgs_parse_stack* s, long int id, size_t count) {
    size_t i;
    long int state;
    pgs_tree** child_array;
    pgs_tree* new_tree;
    
    child_array = malloc(sizeof(*child_array) * count);
    new_tree = pgs_create_tree_nonterminal(id, 0);
    if(child_array == NULL || new_tree == NULL) {
        free(child_array);
        return PGS_MALLOC;
    }
    state = parse_state_table[pgs_parse_stack_top_state(s)][id];
    for(i = 0; i < count; i++) {
        child_array[i] = s->data[s->size - count + i].tree;
    }

    new_tree->tree_data.nonterminal.nonterminal = id;
    new_tree->tree_data.nonterminal.child_count = count;
    new_tree->tree_data.nonterminal.children = child_array;

    s->size -= count;
    s->data[s->size - 1].tree = new_tree;
    s->data[s->size - 1].state = state;

    return PGS_NONE;
}

void pgs_parse_stack_free(pgs_parse_stack* s) {
    size_t i;
    for(i = 0; i < s->size; i++) {
        free(s->data[i].tree);
    }
    free(s->data);
}

long int pgs_parse_stack_top_state(pgs_parse_stack* s) {
    return s->data[s->size - 1].state;
}

pgs_tree* pgs_parse_stack_top_tree(pgs_parse_stack* s) {
    return s->data[s->size - 1].tree;
}

#define PGS_PARSE_ERROR(label_name, error_name, code, text) \
    error_name = code; \
    pgs_state_error(s, error_name, text); \
    goto label_name;

pgs_error pgs_do_parse(pgs_state* s, pgs_token_list* list, pgs_tree** into) {
    pgs_error error;
    pgs_parse_stack stack;
    pgs_tree* top_tree;
    long int top_state;
    long int tree_table_index;
    long int action;
    struct pgs_item_s* item;
    pgs_token* current_token; 
    size_t index = 0;
    
    if((error = pgs_parse_stack_init(&stack))) return error;
    while(1) {
        current_token = &list->tokens[index];
        top_tree = pgs_parse_stack_top_tree(&stack);
        top_state = pgs_parse_stack_top_state(&stack);
        tree_table_index = pgs_tree_table_index(top_tree);
        if(tree_table_index == PGS_MAX_TERMINAL + 2) break;
        action = parse_action_table[top_state][pgs_token_list_top_id(list)];

        if(action == -1) {
            PGS_PARSE_ERROR(error_label, error, PGS_BAD_TOKEN, "Unexpecte token at position");
        } else if(action == 0) {
            if(index >= (list->token_count)) {
                PGS_PARSE_ERROR(error_label, error, PGS_EOF_SHIFT, "Unexpected end of file");
            }

            error = pgs_parse_stack_append_terminal(&stack, current_token);
            if(error) goto error_label;
        } else {
            item = &items[action - 1];
            error = pgs_parse_stack_append_nonterminal(&stack, item->left_id, item->right_count);
        }
    }

    *into = stack.data[stack.size - 1].tree;
    stack.size -= 1;

    error_label:
    pgs_parse_stack_free(&stack);
    return error;
}
