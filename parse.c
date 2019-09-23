#include "lex.h"

#define RHS 0
#define LHS 1
#define OPT 2

#define DEF_NODE_ALLOC 3

#define IS_SPECIFER(c) (c >= 6 && c <= 8)
typedef enum e_node_type { GBL_DEF, FUNC_DEF, DECL, STAT, NOP, LIST, SPECS } node_type;

typedef struct parser_state_s {
    enum e_state { ERROR, FAILED, ABORTED, SUCCESS } state;
    int uidcounter = 0;
} parser_t;
parser_t parser;

typedef struct node_s {
    int uid;
    int maxchilds = DEF_NODE_ALLOC;
    int nchilds = 0;
    // NOP creates a functionally empty node for linking 
    // lists of lower-level grammar
    node_type type;
    struct node_s*          parent;
    const struct token_s*   tkn;
    struct node_s**         childs; 
} node;

// Allocates an node
node* mknode(const token* tkn){
    node* nd = (node*)malloc(sizeof(node));
        if(nd == NULL)
            return NULL;
    nd->childs = (node**)malloc(DEF_NODE_ALLOC*sizeof(node*));
    nd->uid = parser.uidcounter++;
    nd->tkn = tkn;
    nd->nchilds = 0;
    if((nd->childs = (node**)memset(nd->childs, (int)NULL, DEF_NODE_ALLOC*(sizeof(node*)))) == NULL )
       return NULL;
    return nd;
}

static inline node* mknode_typed(const token* tkn, const node_type t){
    node* tmp_node = mknode(tkn);
    if( tmp_node == NULL)
        return NULL;
    tmp_node->type = t;
    return tmp_node;
}

static inline token** pop(token** stack){
    if(*stack == NULL || stack == NULL || (*stack)->next == NULL)
        return NULL;
    else
        (*stack) = (*stack)->next;
    return stack;
}

static inline token** peek(token** first){
    if(*first == NULL || first == NULL || (*first)->next == NULL)
        return NULL;
    else
        return &((*first)->next);
}

token* expect_type(int tkn_type, token** first ){
    token* current_token = *first;
    if( (current_token)->type != tkn_type ){
        // Indicates an error in the parser state machine
        parser.state = ERROR;
        // TODO : Error message
        printf("ERROR : Expected ");
        switch(tkn_type){
            case IDENT:
                printf("identifier");
                break;
            case INTLIT:
                printf("integer literal");
                break;
            case STRLIT:
                printf("string literal");
                break;
            case OPER:
                printf("operator");
                break;
            case KEYWD:
                printf("keyword");
                break;
            case DTYPE:
                printf("type specifier");
                break;
            default:
        }
        printf(" at character %d of line %d\n", (*current_token)->xpos, (current_token)->ypos );
        // OPTIONAL !! TO DEBUG !! : Creates a fictional token that corresponds to what is expected
        //                           in order to continue compilation
        token* return_token = addtoken(NULL, NULL, NULL, tkn_type, parser.uidcounter++, NULL, NULL );
        return_token->next = current_token->next;
        return return_token;
    } else
        return current_token;
}

token* expect_char(char c, token** first){
    token* current_token = *first;
    if(current_token->strval[0] != c){
        parser.state = ERROR;
        // See expect_type function for explanation
        printf("ERROR : Expected %c at character %d on line %d", c, current_token->xpos, current_token->ypos);
        token* return_token = addtoken(to_str(c), NULL, NULL, OPER, parser.uidcounter++, NULL, NULL);
        return_token->next = current_token->next;
        return return_token;
    } else 
        // The token is expected and valid
        return current_token;
}

// addnode function :
//      Adds appended node to parent's node list which is defined as
//      a malloc'ed memory bloc filled with node* (see mknode)
//      Uses internal node-specific maxchild and nchilds ints to 
//      emulate dynamically growing arrays
//      Returns parent, can be used with NULL values
node* addnode(node* parent, node* append){
    if(parent->nchilds >= parent->maxchilds){
        parent->maxchilds*=2;
        parent->childs = (node**)realloc(parent->childs, parent->maxchilds*sizeof(node*));
        if(parent->childs == NULL) return NULL;
    }
    parent->childs[parent->nchilds++] = append;
    return parent;
}

// buildast function :
//      Sets treeroots childs to treeleft, treeright and (optionally) treeopt
//      in order to build a tree
//      Returns treeroot, can be used with NULL values
node* buildast( node* treeroot, node* treeleft, node* treeright, node* treeopt){
    (treeroot)->childs[LHS] = treeleft;
    (treeroot)->childs[RHS] = treeright;
    (treeroot)->nchilds = 2;
    if(treeopt != NULL){
        (treeroot)->childs[OPT] = treeopt;
        (treeroot)->nchilds = 3;
    }
    return treeroot;
}

// Function prototypes for parsing function

node* parse_file(token** first);
node* parse_translation_unit(token** first);
node* parse_global_defn(token** first);
node* parse_func_def(token** first);
node* parse_decl(token** first);
node* parse_specifiers(token** first);
node* parse_declarator(token** first);
node* parse_compound_statement(token** first);
node* parse_direct_declarator(token** first);
node* parse_direct_declarator_prime(token** first);

// parse : file parsing function
// Returns : pointer to node of the top-level structure
// Args : pointer to first token of the token list linked list

// parse_file function :
//      Implements the following grammar
//      program ::= <translation-unit>
//      Note : this function is a stub but (TODO :) it will let us
//      implement the translation of multiple translation-units from
//      a single file using the #include directive
node* parse_file(token** first){
    return parse_translation_unit(first);
}

// parse_translation_unit function:
//      Implements the following grammar
//      translation-unit ::= <global-decl> { <global-decl> }
//      Returns an abstract top-level node, root of the 
//      translation unit tree
node* parse_translation_unit(token** first){
    // Top level data-structure for program representation in AST
    // Element is abstract : a node that holds no token
    node* top_node = mknode_typed(NULL, LIST);
    node* last_node = top_node;
    node* check_node;
    do{
        // Translation unit parsing loop
        last_node = addnode(last_node, check_node = parse_global_defn(first));
    } while (check_node->type == GBL_DEF);
    return top_node;
}

// parse_global_defn function : 
//      Implements the following grammar:
//      global_defn ::= <variable-declaration> | <function-definition>
//      Returns an abstract top-level node, root of the tree
node* parse_global_defn(token** first){
    token* base_token = *first;
    node* top_node = parse_func_def(first);
    if(top_node->type != FUNC_DEF){
        if(*first != base_token)
            *first = base_token;
        top_node = parse_decl(first);
    }
    else if(top_node->type != DECL){
        if(*first != base_token)
            *first = base_token;
        printf("Expected global definition or declaration\n");
        return NULL;
    }
}

// parse_func_def function :
//      Implements the following grammar:
//      <function-definition> ::= <specifiers> <declarator> <compound-statement>
//      Returns an abstract top-level node,
//      root of the tree that represents the function

node* parse_func_def(token** first){
    token* last_token;
    node* root_node = mknode_typed(NULL, FUNC_DEF);
    root_node = addnode(root_node, parse_specifiers(first));
    root_node = addnode(root_node, parse_declarator(first));
    root_node = addnode(root_node, parse_compound_statement(first));
    return root_node;
}

// parse_specifiers function :
//      Implements the following grammar:
//      <specifiers> ::= <data-type> <specifers> | <storage-class-specifiers> <specifiers>
//      Returns an abstract node that represents the root of the tree that contains all specifiers
node* parse_specifiers(token** first){
    token* lookahead = *(peek(first));
    node* root_node = mknode_typed(NULL, SPECS);
    do {
        if((*first)->type == DTYPE){
            root_node = addnode(root_node, mknode(*first));
            pop(first);
        }
        else if ((*first)->type == STORAGE_CLASS){
            root_node = addnode(root_node, mknode(*first));
            pop(first);
        } else{
            printf("Expected type or storage class specifier on line %d\n", (*first)->ypos);
            parser.state = ERROR;
            break;
        }
    } while( (lookahead = *peek((first)))->type == STORAGE_CLASS || lookahead->type == DTYPE );
    return root_node;
}

node* parse_declarator(token** first){
    token* next_token = *(first);
    token* lookahead = *(peek(first));
    node* root_node = mknode_typed(NULL, DCL);
    
    if(strcmp((next_token->strval), "*") == 0 || (next_token)->type == ){
        root_node = addnode(root_node, mknode(next_token));
        while(strcmp((lookahead->strval), "*") == 0 || (lookahead)->type == TQUAL ){
            pop(first);
            root_node = addnode(root_node, mknode(lookahead));
            lookahead = *(peek(first));
        }
    }
    root_node = addnode(root_node, parse_direct_declarator(first));
    return root_node;
}

node* parse_direct_declarator(token** first){
    token* lookahead = *(peek(first));
    node* root_node = mknode_typed(NULL, DIR_DCL);

    if(strcmp((*first)->strval, "(") == 0)
        root_node = addnode(root_node, parse_declarator(pop(first)));
    else if( (*first)->type == IDENT )
        root_node = addnode(root_node, mknode(*first));
    root_node = addnode(root_node, parse_direct_declarator_prime(pop(first)));
    return root_node;
}

node* parse_direct_declarator_prime(token** first){
      token* lookahead = *(peek(first));
      node* root_node = mknode_typed(DIR_DCL, NULL);
      
      do{
          if(strcmp((*first)->strval, "[") == 0)
              root_node = addnode(root_node, parse_constant_expression(pop(first)));
          else if (strcmp((*first)->strval, "(") == 0){
              if (lookahead->type == DTYPE || lookahead->type == TQUAL || lookahead->type == STORAGE_CLASS)
                      root_node = addnode(root_node, parse_parameter_list(pop(first)));
              else if (lookahead->type == IDENT)
                  root_node = addnode(root_node, parse_identifier_list(pop(first)));
          } else
              break; // Adds nothing to the node, dir-dcl' is allowed to be an empty strinG
          lookahead = *(peek(first));
      } while(IS_SPECIFER(lookahead->type) || lookahead->type == IDENT );
      return root_node;
}

node* parse_identifier_list(token** first){
    token* lookahead = *(peek(first));
    node* root_node = mknode_typed(LIST);

    // We know from last step this is the correct parsing route
    // if the token *first points to is not an identifer, we are in
    // prescence of a user-created grammatical mistake
    root_node = addnode(root_node, mknode(expect_type(IDENT, first)));
    for(; lookahead->strval[0] == ','; lookahead = *(peek(first)))
        root_node = addnode(root_node, mknode(expect_type(pop(first))));
    return root_node;
}

node* parse_parameter_list(token** first){

    

node* parse_constant_expression(token** first){
