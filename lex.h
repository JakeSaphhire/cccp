#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define HASHSIZE 10

#define IN 0
#define OUT 1

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))
static const char* arr_key[4] = {"if", "else", "while", "return"};
static const char* arr_types[2] = {"int", "char"};
static const char* arr_delim[7] = { "(", ")", "{", "}", "[", "]", ";"};

static struct hash_entry* h_typetable[HASHSIZE];
static struct hash_entry* h_keywtable[HASHSIZE];

typedef struct hash_entry{
    char* value;
    //Will be used for preprocessor macros
    //Currently unused
    char* defn;
    struct hash_entry* next;
} entry;

// djb2 hash function for strings
static unsigned long int hash(const char* str){
    unsigned long int hashvalue = 5831;
    int c;
    while( (c = *str++) )
        hashvalue = (hashvalue * 33) ^ c;
    return hashvalue;
}

static entry* hfind( const char* str , entry* htable[]){
    for(entry* head = htable[hash(str)]; head != NULL; head = head->next)
        if(strcmp(str, head->value) == 0)
            return head;
    return NULL;
}

// Hash Table Install Function
//      Function :  Installs the passed value + defintion 
//                  in the global hash table
//      Returns :   Returns the newly created table entry
//                  in case of success and NULL in case of
//                  failure
static entry* hinstall(const char* val, const char* defn, entry* htable[]){
    entry* hdef;
    unsigned long int hashvalue;
    if( (hdef = hfind(val, htable)) == NULL){
        hdef = (entry*)malloc(sizeof(entry));
        // Returns NULL if malloc fails to allocate memory
        // or if no valid name is passed to the function
        if(hdef == NULL || (hdef->value = strdup(val)) == NULL)
            return NULL;
        hashvalue = hash(val);
        hdef->next = htable[hashvalue];
        htable[hashvalue] = hdef;
    } else
        free((void*) hdef->defn);
    hdef->defn = strdup(defn);
    return hdef;
}

static void htableinit(void){
    // Puts basic supported keywords in the hash table
    unsigned long int ia = 0;
    for(ia = 0; ia < ARRAY_SIZE(arr_key); ia++)
        hinstall(arr_key[ia], NULL, h_keywtable);
    // Puts basic types in the hash table
    for(ia = 0; ia < ARRAY_SIZE(arr_types); ia++)
        hinstall(arr_types[ia], NULL, h_typetable);
}

// ******************************************
// *        LEXER CODE STARTS HERE          *
// ******************************************

enum token_type{ IDENT = 1, INTLIT, STRLIT, OPER, KEYWD, DTYPE, STORAGE_CLASS, TQUAL };
enum flags { FIRST = 01, STRING = 02, INTEGER = 04 };

#define addtoword(s, c) strcat(s, to_str(c))

static const char operators[19] = {'=', '+', '-', '*', '/', '&', '|', '^', '!', '{', '}', '[', ']', '(', ')', '<', '>', '%', ';'};

typedef struct token_s{
    union {
        char*           strval;
        long long int   intval;
    };
    int ypos;
    int xpos;
    int type;
    int uid;
    struct token_s* next;
} token;

inline bool checkop(char c){
    for(unsigned int i = 0; i < ARRAY_SIZE(operators); i++)
        if(operators[i] == c)
            return true;
    else return false;
}

inline char* to_str(char c){
    char* str = (char*)malloc(2*sizeof(char));
    str[0] = c; str[1] = '\0';
    return str;
}

token* addtoken(char* svalue, int ivalue, token* last, int type, int uid, int xpos, int ypos){
    token* tk = (token*)malloc(sizeof(token));
    if(tk == NULL)
        return NULL;
    // Only in the case of an int literal
    // is the int value used for token
    // initalization. Otherwise, string 
    // representation makes more sense
    if(type == INTLIT)
        tk->intval = ivalue;
    else 
        tk->strval = svalue;
    tk->type = type;
    tk->uid = uid;
    tk->next = NULL;
    if( last != NULL)
        last->next = tk;
    return tk;
}

token* lex(char* source_code){
    FILE* fsource = fopen(source_code, "r");
    char c;
    char next;
    char* word = (char*)malloc(7);
    char* integer = (char*)malloc(7);
    char* op = (char*)malloc(2);
    
    // Column or charcter number of read charcter within a line
    int col = 0;
    int line = 1;
    int inside = 0;
    
    // Defines values for max and current lengths
    // of arrays that lexically represent
    // integers and strings in order to implement
    // a tiny version of a dynamically allocated array
    // that only grows to a max size of 32 sdasd
    int stcurlen = 0;
    int stmlen = 7;
    int intcurlen = 0;
    int intmlen = 7;

    int uid = 0;
    token* first = NULL;
    token* prev = NULL;

    while((c = fgetc(fsource))){
        col++;
        if(c == '\n')
            line++;
        else if(checkop(c)){
            if( addtoword(op, c) == NULL)
                printf("Error in op string concat");
            if( checkop(next = fgetc(fsource)) ){
                if(first == NULL)
                    first = prev = addtoken(op, 0, prev, OPER, uid, col, line);
                else
                    prev = addtoken(op, 0, prev, OPER, uid, col, line);
                uid++;
                memset(op, 0, sizeof(char)*2);
            }
            ungetc(next, fsource);
        } else if (inside || isalpha(c)) {
            // Dynamic allocation of a memory block to
            // contain a string. Uses the variable
            // stcurlen to measure the current length
            // of the string and to realloc when such
            // length becomes too small
            if(stcurlen >= stmlen){
                word = (char*)realloc(word, (stmlen = (int)(stmlen*1.5*sizeof(char))) );
                stmlen = (int)(stmlen*1.5);
            }
            addtoword(word, c);
            stcurlen++;
            if( checkop(next = fgetc(fsource)) || c == ' ' || !isprint(next) ){
                unsigned short token_class;
                if(hfind(word, h_typetable) != NULL)
                    token_class = DTYPE;
                else if(hfind(word, h_keywtable) != NULL)
                    token_class = KEYWD;
                else
                    token_class = IDENT;
                if(first == NULL)
                    first = prev = addtoken(word, 0, prev, token_class, uid, col, line);
                else
                    prev = addtoken(word, 0, prev, token_class, uid, col, line);
                uid++;
                memset(word, 0, stmlen);
            } else
                inside = 1;
            ungetc(next, fsource);
        } else if (isdigit(c)){
            if(intcurlen >= intmlen){
                integer = (char*)realloc(word, (stmlen = (int)(intmlen*1.5*sizeof(char))) );
                intmlen = (int)(intmlen*1.5);
            }
            addtoword(integer, c);
            intcurlen++;
            if(!isdigit(next = fgetc(fsource))){
                if(first == NULL)
                    first = prev = addtoken(integer, atoll(integer), prev, INTLIT, uid, col, line);
                else
                    prev = addtoken(integer, atoll(integer), prev, INTLIT, uid, col, line);
                uid++;
                memset(integer, 0, intmlen);
            }
            ungetc(next, fsource);
        }
        return first;
    }
}
