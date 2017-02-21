/*
 * main.c
 *
 * Assuming compilation is on a 64-bit system, with the following sizes:
 * char : 1 : 8-bit
 * int : 4 : 32-bit
 * long int : 8 : 64-bit
 * long long int : 8 : 64-bit
 * float : 4 : 32-bit (single-precision)
 * double : 8 : 64-bit (double-precision)
 * long double 16 : 128-bit (quadruple-precision)
 * void* : 8 : 64-bit
 *
 * The following sizes hold:
 * token_t::payload : 16
 * token_t : 32
 *
 *  Created on: Feb 14, 2017
 *      Author: Duncan
 */

#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef DEBUG
#undef DEBUG
#define DEBUG 1
#else
#define DEBUG 0
#endif

#define TKN_KEYWD 0
#define TKN_ID 1
#define TKN_INT 2
#define TKN_FLOAT 3
#define TKN_CHAR 4
#define TKN_STR 5
#define TKN_OPER 6
#define TKN_GROUP 7
#define TKN_TERM 8
#define TKN_MAX 9

#define TKN_INT_STD 0
#define TKN_INT_U 1
#define TKN_INT_L 2
#define TKN_INT_UL 3
#define TKN_INT_LL 4
#define TKN_INT_ULL 5

#define TKN_FLOAT_F 0
#define TKN_FLOAT_D 1
#define TKN_FLOAT_LD 2

#define TKN_ALNUM_EMB 0
#define TKN_ALNUM_PTR 1

#define NOERR 0
#define ERR_IO 1
#define ERR_PARSE_ERR 4

#define REGEX_FLAGS (REG_EXTENDED | REG_ICASE | REG_NEWLINE)
#define MAX_ID_LEN 32
#define LN_BUFSIZ 1024

typedef int errr;
typedef int bool;
typedef unsigned int uint;

typedef struct string_list {
    char* str;
    struct string_list * next;
} string_list;

typedef struct {
    int type;
    int subtype;
    union {
        int kwid;
        char* aid_ptr;
        char aid_emb[16];
        int i;
        unsigned int ui;
        long int li;
        unsigned long int uli;
        long long int lli;
        unsigned long long int ulli;
        float f;
        double d;
        long double ld;
        char c;
        char* str_ptr;
        char str_emb[16];
        char op[3];
        char gr;
    } payload;
} token_t;

struct token_list {
    token_t token;
    struct token_list * next;
};
typedef struct token_list token_list;

token_list * head = NULL;
token_list * tail = NULL;

const char* patterns[TKN_MAX] =
        {
                /*                       4                                  8                             12                          16                             20                                 24                                  28                                    32  */
                "(auto)|(break)|(case)|(char)|(const)|(continue)|(default)|(do)|(double)|(else)|(enum)|(extern)|(float)|(for)|(goto)|(if)|(int)|(long)|(register)|(return)|(short)|(signed)|(sizeof)|(static)|(struct)|(switch)|(typedef)|(union)|(unsigned)|(void)|(volatile)|(while)",
                "[a-zA-Z_][a-zA-Z_0-9]*", /* valid identifier */
                "(([1-9][0-9]*)|(0[0-7]*)|(0[Xx][0-9A-Fa-f]+)|(0[Bb][01]+))[Uu]?([Ll]|(ll)|(LL))?", /* integer literal */
                "(([0-9]+\\.[0-9]*|\\.[0-9]+)([eE][+-]?[0-9]+)?)|[0-9]+[eE][+-]?[0-9]+[FfLl]", /* floating-point literal */
                "'([^'\\\\]|(\\\\([abfnrtv\\'\"?]|([0-7]{1,3})|(x[0-9A-Fa-f]+))))'", /* character literal */
                "\"([^\"\\\\]|(\\\\([abfnrtv\\'\"?]|([0-7]{1,3})|(x[0-9A-Fa-f]+))))*\"", /* string literal */
                "([=+*/%><!~&|^]=?)|(-=?)|(\\+\\+)|(--)|(&&)|(\\|\\|)|(<<=?)|(>>=?)|(->)|[?:]", /* operator */
                "[(),{}]|\\[|\\]", /* grouping symbols */
                ";" /* statement terminator */
        };
regex_t regexen[TKN_MAX];

errr init_regex(void);
errr lex(FILE *, FILE *);
errr make_token(char*, int);
int get_kwid(char*);
void printhlp(void);

int main(int argc, char** argv) {
    FILE * input = stdin;
    FILE * output = stdout;

    switch (argc) {
        case 3:
            output = fopen(argv[2], "wb");
        case 2:
            if (!strcmp(argv[1], "--help")) {
                printhlp();
                return NOERR;
            }
            input = fopen(argv[1], "r");
        case 1:
        case 0:
            break;
        default:
            printhlp();
            return NOERR;
    }
    errr err = init_regex();
    if (err) {
        printf("Error: %d\n", err);
        return err;
    }
    if(DEBUG) printf("Init\n");
    err = lex(input, output);
    if (err) {
        printf("Error: %d\n", err);
        return err;
    }
    fclose(input);
    fclose(output);

    return err;
}

errr lex(FILE * in, FILE * out) {
    char buf[LN_BUFSIZ];
    regmatch_t pmatch;
    errr err = NOERR;
    while (!feof(in)) {
        if(DEBUG) printf("Loop start\n");
        /* Read line */
        fgets(buf, LN_BUFSIZ, in);
        if (ferror(in)) return ERR_IO;
        if(DEBUG) printf("Read line\n");
        char *line = buf;
        while (*line) {
            while (*line && isspace(*line)) {
                line++;
            }
            if (!*line) break;
            int curkind = -1;
            int curlen = 0;
            if(DEBUG) printf("Found token start\n");
            for (int i = 0; i < TKN_MAX; i++) {
                if(DEBUG) printf("Regex loop\n");
                if ((!regexec(&regexen[i], line, 1, &pmatch, 0))
                        && (pmatch.rm_so == 0) && (pmatch.rm_eo > curlen)) {
                    curkind = i;
                    curlen = pmatch.rm_eo;
                }
                if(DEBUG) printf(
                        "End Regex loop: matched %d:%d for token pattern %d. Current token ID is %d, length %d\n",
                        pmatch.rm_so, pmatch.rm_eo, i, curkind, curlen);
            }
            if (curkind == -1) return ERR_PARSE_ERR;
            if(DEBUG) printf("Identified token\n");
            char* tok = malloc((curlen + 1) * sizeof(char));
            strncpy(tok, line, curlen);
            tok[curlen] = '\0';
            line += curlen;
            err = make_token(tok, curkind);
            if (err) return err;
        }
    }
    if(DEBUG) printf("End tokenizer loop\n");
    string_list * str_hack_head = NULL;
    string_list * str_hack_tail = NULL;
    uint str_hack_idx = 0;
    while (head != NULL) {
        if (((head->token.type == TKN_ID) || (head->token.type == TKN_STR))
                && (head->token.subtype == TKN_ALNUM_PTR)) {
            /* Hack -- payload string too long for standard entry. Store strings in list to be written later... */
            if(DEBUG) printf("Hacking...");
            if (!str_hack_idx) {
                str_hack_head = (string_list*) malloc(sizeof(string_list));
                str_hack_tail = str_hack_head;
            } else {
                str_hack_tail->next = (string_list*) malloc(
                        sizeof(string_list));
                str_hack_tail = str_hack_tail->next;
            }
            if(DEBUG) printf("For string '%s'", head->token.payload.aid_ptr);
            str_hack_tail->str = head->token.payload.aid_ptr;
            /* ... and use fake pointers for writing */
            head->token.payload.aid_ptr = str_hack_idx;
            str_hack_idx++;
        } else if(DEBUG) printf("No hack necessary\n");
        fwrite(&(head->token), sizeof(token_t), 1, out);
        if (ferror(out)) return ERR_IO;
        token_list * del = head;
        head = head->next;
        free(del);
        if(DEBUG) printf("Wrote token. Next at %p\n", head);
    }
    if(DEBUG) printf("Written tokens\n");
    token_t sentinel = { .type = TKN_MAX };
    fwrite(&sentinel, sizeof(token_t), 1, out);
    if(DEBUG) printf("Written sentinel \n");
    fflush(out);
    /* Hack -- now actually store the hacked strings */
    while (str_hack_head != NULL) {
        if(DEBUG) printf("Writing string %p\n"/* '%s'\n", str_hack_head->str*/,
                str_hack_head->str);
        fwrite(str_hack_head->str, sizeof(char), strlen(str_hack_head->str) + 1,
                out);
        if (ferror(out)) {
            printf("IOError\n");
            return ERR_IO;
        }
        string_list * del = str_hack_head;
        str_hack_head = str_hack_head->next;
        free(del->str);
        free(del);
        if(DEBUG) printf("Written string_hack. Next at %p\n", str_hack_head);
    }
    return NOERR;
}

errr make_token(char* tok, int type) {
    if (tail) {
        tail->next = (token_list*) malloc(sizeof(token_list));
        tail = tail->next;
    } else {
        head = (token_list*) malloc(sizeof(token_list));
        tail = head;
    }
    tail->next = NULL;
    tail->token.type = type;
    switch (type) {
        case TKN_KEYWD:
            tail->token.payload.kwid = get_kwid(tok);
            free(tok);
            break;
        case TKN_ID:
            if (strlen(tok) < 16) {
                strncpy(tail->token.payload.aid_emb, tok, 16);
                tail->token.subtype = TKN_ALNUM_EMB;
                free(tok);
            } else {
                tail->token.payload.aid_ptr = tok;
                tail->token.subtype = TKN_ALNUM_PTR;
            }
            break;
        case TKN_INT:
            tail->token.subtype = TKN_INT_STD;
            for (char* cp = tok; *cp != '\0'; cp++) {
                switch (*cp) {
                    case 'U':
                    case 'u':
                        tail->token.subtype++;
                        *cp = '\0';
                        continue;
                    case 'L':
                    case 'l':
                        tail->token.subtype += 2;
                        *cp = '\0';
                        continue;
                    default:
                        continue;
                }
            }
            char* dump;
            switch (tail->token.subtype) {
                case TKN_INT_STD:
                    tail->token.payload.i = (int) strtol(tok, &dump, 0);
                    break;
                case TKN_INT_U:
                    tail->token.payload.ui = (unsigned int) strtoul(tok, &dump,
                            0);
                    break;
                case TKN_INT_L:
                    tail->token.payload.li = strtol(tok, &dump, 0);
                    break;
                case TKN_INT_UL:
                    tail->token.payload.uli = strtoul(tok, &dump, 0);
                    break;
                case TKN_INT_LL:
                    tail->token.payload.lli = strtoll(tok, &dump, 0);
                    break;
                case TKN_INT_ULL:
                    tail->token.payload.ulli = strtoull(tok, &dump, 0);
                    break;
                default:
                    return ERR_PARSE_ERR;
            }
            free(tok);
            break;
        case TKN_FLOAT:
            tail->token.subtype = TKN_FLOAT_D;
            for (char* cp = tok; *cp != '\0'; cp++) {
                switch (*cp) {
                    case 'F':
                    case 'f':
                        tail->token.subtype = TKN_FLOAT_F;
                        *cp = '\0';
                        break;
                    case 'L':
                    case 'l':
                        tail->token.subtype = TKN_FLOAT_LD;
                        *cp = '\0';
                        break;
                    default:
                        continue;
                }
                break;
            }
            char* dump1;
            switch (tail->token.subtype) {
                case TKN_FLOAT_F:
                    tail->token.payload.f = strtof(tok, &dump1);
                    break;
                case TKN_FLOAT_D:
                    tail->token.payload.d = strtod(tok, &dump1);
                    break;
                case TKN_FLOAT_LD:
                    tail->token.payload.ld = strtold(tok, &dump1);
                    break;
                default:
                    return ERR_PARSE_ERR;
            }
            free(tok);
            break;
        case TKN_CHAR:
            if (tok[1] == '\\') {
                char * pEnd;
                switch (tok[2]) {
                    case 'a':
                        tail->token.payload.c = '\a';
                        break;
                    case 'b':
                        tail->token.payload.c = '\b';
                        break;
                    case 'f':
                        tail->token.payload.c = '\f';
                        break;
                    case 'n':
                        tail->token.payload.c = '\n';
                        break;
                    case 'r':
                        tail->token.payload.c = '\r';
                        break;
                    case 't':
                        tail->token.payload.c = '\t';
                        break;
                    case 'v':
                        tail->token.payload.c = '\v';
                        break;
                    case '\\':
                        tail->token.payload.c = '\\';
                        break;
                    case '\'':
                        tail->token.payload.c = '\'';
                        break;
                    case '"':
                        tail->token.payload.c = '"';
                        break;
                    case '?':
                        tail->token.payload.c = '?';
                        break;
                    case 'x':
                        tail->token.payload.c = (char) strtol(tok + 3, &pEnd,
                                16);
                        break;
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                        tail->token.payload.c = (char) strtol(tok + 2, &pEnd,
                                8);
                        break;
                    default:
                        return ERR_PARSE_ERR;
                }
            } else {
                tail->token.payload.c = tok[1];
            }
            free(tok);
            break;
        case TKN_STR:
            tail->token.payload.str_ptr = (char*) malloc(
                    (strlen(tok) - 1) * sizeof(char));
            char* cur = tok + 1;
            int idx = 0;
            while (*cur != '\0') {
                if (*cur == '\\') {
                    cur++;
                    char * pEnd;
                    char buf[4];
                    switch (*cur) {
                        case 'a':
                            tail->token.payload.str_ptr[idx] = '\a';
                            break;
                        case 'b':
                            tail->token.payload.str_ptr[idx] = '\b';
                            break;
                        case 'f':
                            tail->token.payload.str_ptr[idx] = '\f';
                            break;
                        case 'n':
                            tail->token.payload.str_ptr[idx] = '\n';
                            break;
                        case 'r':
                            tail->token.payload.str_ptr[idx] = '\r';
                            break;
                        case 't':
                            tail->token.payload.str_ptr[idx] = '\t';
                            break;
                        case 'v':
                            tail->token.payload.str_ptr[idx] = '\v';
                            break;
                        case '\\':
                            tail->token.payload.str_ptr[idx] = '\\';
                            break;
                        case '\'':
                            tail->token.payload.str_ptr[idx] = '\'';
                            break;
                        case '"':
                            tail->token.payload.str_ptr[idx] = '"';
                            break;
                        case '?':
                            tail->token.payload.str_ptr[idx] = '?';
                            break;
                        case 'x':
                            tail->token.payload.str_ptr[idx] = (char) strtol(
                                    cur + 1, &pEnd, 16);
                            cur = pEnd - 1;
                            break;
                        case '0':
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7':

                            strncpy(buf, cur, 3);
                            buf[3] = '\0';
                            tail->token.payload.str_ptr[idx] = (char) strtol(
                                    buf, &cur, 8);
                            cur--;
                            break;
                        default:
                            return ERR_PARSE_ERR;
                    }
                } else if (*cur == '"') {
                    break;
                } else {
                    tail->token.payload.str_ptr[idx] = *cur;
                }
                idx++;
                cur++;
            }
            tail->token.payload.str_ptr[idx] = '\0';
            free(tok);
            if (strlen(tail->token.payload.str_ptr) < 16) {
                char* tmp = tail->token.payload.str_ptr;
                strncpy(tail->token.payload.aid_emb, tmp, 16);
                free(tmp);
                tail->token.subtype = TKN_ALNUM_EMB;
                if(DEBUG) printf("Embedded string\n");
            } else {
                tail->token.subtype = TKN_ALNUM_PTR;
                if(DEBUG) printf("Referenced string\n");
            }
            break;
        case TKN_OPER:
            strncpy(tail->token.payload.op, tok, 3);
            free(tok);
            break;
        case TKN_GROUP:
            tail->token.payload.gr = *tok;
            free(tok);
            break;
        case TKN_TERM:
            break;
        default:
            return ERR_PARSE_ERR;
    }
    return NOERR;
}

#define KW_AUTO 0
#define KW_BREAK 1
#define KW_CASE 2
#define KW_CHAR 3
#define KW_CONST 4
#define KW_CONTINUE 5
#define KW_DEFAULT 6
#define KW_DO 7
#define KW_DOUBLE 8
#define KW_ELSE 9
#define KW_ENUM 10
#define KW_EXTERN 11
#define KW_FLOAT 12
#define KW_FOR 13
#define KW_GOTO 14
#define KW_IF 15
#define KW_INT 16
#define KW_LONG 17
#define KW_REGISTER 18
#define KW_RETURN 19
#define KW_SHORT 20
#define KW_SIGNED 21
#define KW_SIZEOF 22
#define KW_STATIC 23
#define KW_STRUCT 24
#define KW_SWITCH 25
#define KW_TYPEDEF 26
#define KW_UNION 27
#define KW_UNSIGNED 28
#define KW_VOID 29
#define KW_VOLATILE 30
#define KW_WHILE 31

int get_kwid(char* tok) {
    switch (tok[0]) {
        case 'a':
            /* auto */
            return KW_AUTO;
        case 'b':
            /* break */
            return KW_BREAK;
        case 'c':
            /* case, char, const, continue */
            switch (tok[1]) {
                case 'a':
                    return KW_CASE;
                case 'h':
                    return KW_CHAR;
                case 'o':
                    switch (tok[3]) {
                        case 's':
                            return KW_CONST;
                        case 't':
                            return KW_CONTINUE;
                        default:
                            break;
                    }
                default:
                    return -1;
            }
        case 'd':
            /* default, do, double */
            switch (tok[1]) {
                case 'e':
                    return KW_DEFAULT;
                case 'o':
                    switch (tok[2]) {
                        case '\0':
                            return KW_DO;
                        case 'u':
                            return KW_DOUBLE;
                        default:
                            break;
                    }
                default:
                    return -1;
            }
        case 'e':
            /* else, enum, extern */
            switch (tok[1]) {
                case 'l':
                    return KW_ELSE;
                case 'n':
                    return KW_ENUM;
                case 'x':
                    return KW_EXTERN;
                default:
                    return -1;
            }
        case 'f':
            /* float, for */
            switch (tok[1]) {
                case 'l':
                    return KW_FLOAT;
                case 'o':
                    return KW_FOR;
                default:
                    return -1;
            }
        case 'g':
            /* goto */
            return KW_GOTO;
        case 'i':
            /* if, int */
            switch (tok[1]) {
                case 'f':
                    return KW_IF;
                case 'n':
                    return KW_INT;
                default:
                    return -1;
            }
        case 'l':
            /* long */
            return KW_LONG;
        case 'r':
            /* register, return */
            switch (tok[2]) {
                case 'g':
                    return KW_REGISTER;
                case 't':
                    return KW_RETURN;
                default:
                    return -1;
            }
        case 's':
            /* short, signed, sizeof, static, struct, switch */
            switch (tok[1]) {
                case 'h':
                    return KW_SHORT;
                case 'i':
                    switch (tok[2]) {
                        case 'g':
                            return KW_SIGNED;
                        case 'z':
                            return KW_SIZEOF;
                        default:
                            return -1;
                    }
                case 't':
                    switch (tok[2]) {
                        case 'a':
                            return KW_STATIC;
                        case 'r':
                            return KW_STRUCT;
                        default:
                            return -1;
                    }
                case 'w':
                    return KW_SWITCH;
                default:
                    return -1;
            }
        case 't':
            /* typedef */
            return KW_TYPEDEF;
        case 'u':
            /* union, unsigned */
            switch (tok[2]) {
                case 'i':
                    return KW_UNION;
                case 's':
                    return KW_UNSIGNED;
                default:
                    return -1;
            }
        case 'v':
            /* void, volatile */
            switch (tok[2]) {
                case 'i':
                    return KW_VOID;
                case 'l':
                    return KW_VOLATILE;
                default:
                    return -1;
            }
        case 'w':
            /* while */
            return KW_WHILE;
        default:
            return -1;
    }

}

void printhlp() {
    printf("Usage: dcc-lex [source file] [output file]\n");
}

errr init_regex() {
    errr err;
    char errstr[1024];
    for (int i = 0; i < TKN_MAX; i++) {
        err = regcomp(&regexen[i], patterns[i], REGEX_FLAGS);
        if (err) {
            regerror(err, &regexen[i], errstr, 1024);
            printf("Error in regex %d: %s\n", i, errstr);
            return err;
        }
    }
    return NOERR;
}
