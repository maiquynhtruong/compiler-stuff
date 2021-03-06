#include <stdlib.h>
#include <stdio.h>

#include "debug.h"
#include "error.h"
#include "reader.h"
#include "parser.h"
#include "semantics.h"
#include "scanner.h"

Token* look_ahead;
Token* current_token;

// from symbol_table.c
extern SymbolTable *symbolTable;

void match_token(TokenType type) {
    printf("In match_token. Expected type: ");
    printf("%s\n", print_token_type(type));

    if (look_ahead->type != type) {
        printf("Not Matched. look_ahead is: ");
        print_token(look_ahead);
        missing_token(type, look_ahead->lineNo, look_ahead->columnNo);
    } else {
        printf("Matched. look_ahead is: ");
        print_token(look_ahead);

        Token *temp = current_token;
        current_token = look_ahead;
        look_ahead = next_valid_token();

        free(temp);

        printf("Next parsed token : ");
        print_token(look_ahead);
        printf("\n");
    }
}

// holy entry point
int parse(char *file_name) {
    if (open_input_stream(file_name) == IO_ERROR)
        return IO_ERROR;

    current_token = NULL;
    look_ahead = next_token();

    init_symbol_table();

    parse_program();
    print_entry(symbolTable->program);

    clear_symbol_table();

    free(current_token);
    free(look_ahead);
    close_input_stream();
    return IO_SUCCESS;
}

void parse_body_block() {
    if (look_ahead->type != K_BEGIN) parse_declarations();
    match_token(K_BEGIN);

    if (look_ahead->type != K_END) parse_statement_list();
    match_token(K_END);
}

void parse_program() {
    assert("Parsing the program");

    Entry *entry = NULL; // create, enter and exit a program scope

    // program header
    match_token(K_PROGRAM);
    match_token(T_IDENTIFIER);
    match_token(K_IS);
    entry = create_program_entry(current_token->val.stringVal);

    enter_scope(entry->progAttrs->scope);
    parse_body_block(); // program body
    match_token(K_PROGRAM);
    if (look_ahead->type == T_END_OF_FILE) match_token(T_END_OF_FILE);
    exit_scope();

    assert("Done parsing the program");
}

void parse_declarations() {
    assert("Parsing declarations");

    Entry *entry = NULL;
    int global = 0;

    if (look_ahead->type == K_GLOBAL) {
        match_token(K_GLOBAL);
        global = 1;
    }

    switch (look_ahead->type) {
        case K_PROCEDURE:
            parse_proc_declaration(&entry, global);
            match_token(T_SEMI_COLON);
            parse_declarations();
            break;
        // FOLLOW set
        case K_BEGIN: // from program_body, procedure_body
            break;
        default:
            parse_var_declaration(&entry, global);
            declare_entry(entry);
            match_token(T_SEMI_COLON);
            parse_declarations();
            break;
    }
    assert("Done parsing declarations");
}

void parse_proc_declaration(Entry **entry, int global) {
    assert("Parsing a procedure declaration");

    // procedure header
    match_token(K_PROCEDURE);
    match_token(T_IDENTIFIER);
    check_new_identifier(current_token->val.stringVal);
    *entry = create_procedure_entry(current_token->val.stringVal, global);
    declare_entry(*entry);
    enter_scope((*entry)->procAttrs->scope);

    parse_param_list();
    parse_body_block(); // procedure body
    match_token(K_PROCEDURE);
    exit_scope();

    assert("Done parsing a procedure declaration");
}

void parse_var_declaration(Entry **entry, int global) {
    assert("Parsing a variable declaration");

    Type *typeMark = parse_type_mark();
    match_token(T_IDENTIFIER);
    check_new_identifier(current_token->val.stringVal);
    *entry = create_variable_entry(current_token->val.stringVal, global);
    (*entry)->varAttrs->type = typeMark;

    if (look_ahead->type == T_LBRACKET) { // an array
        match_token(T_LBRACKET);
//         //TODO: Is there upper and lower bound or just a number for array size?
//         // if (look_ahead->type == T_MINUS) match_token(T_MINUS);
        match_token(T_NUMBER_INT); // lower bound
//
//         // match_token(T_COLON);
        // if (look_ahead->type == T_MINUS) match_token(T_MINUS);
        // match_token(T_NUMBER_INT); // uppper bound
        (*entry)->varAttrs->type->elementType = typeMark;
        (*entry)->varAttrs->type->typeClass = TC_ARRAY;
        (*entry)->varAttrs->type->arraySize = current_token->val.intVal;
        match_token(T_RBRACKET);
    }
    // declare_entry(entry); // in parse_declarations() and parse_param()
    assert("Done parsing a variable declaration");
}

Type* parse_type_mark() {
    assert("Parsing a type mark");

    Type *typeMark = (Type *) malloc(sizeof(Type));
    switch(look_ahead->type) {
        case K_INT:
            match_token(K_INT);
            typeMark->typeClass = TC_INT; break;
        case K_FLOAT:
            match_token(K_FLOAT);
            typeMark->typeClass = TC_FLOAT; break;
        case K_BOOL:
            match_token(K_BOOL);
            typeMark->typeClass = TC_BOOL; break;
        case K_CHAR:
            match_token(K_CHAR);
            typeMark->typeClass = TC_CHAR; break;
        case K_STRING:
            match_token(K_STRING);
            typeMark->typeClass = TC_STRING; break;
        default:
            throw_error(E_INVALID_TYPE, look_ahead->lineNo, look_ahead->columnNo); break;
    }

    assert("Done parsing a type mark");
    return typeMark;
}

void parse_statement_list() {
    parse_statement();
    while (look_ahead->type == T_SEMI_COLON) {
        match_token(T_SEMI_COLON);
        parse_statement();
    }
}

void parse_statement() {
    assert("Parsing a statement");
    switch (look_ahead->type) {
        case K_IF: parse_if_statement(); break;
        case K_FOR: parse_loop_statement(); break;
        case K_RETURN: parse_return_statement(); break;
        case T_IDENTIFIER:
            match_token(T_IDENTIFIER);
            if (look_ahead->type == T_LPAREN) parse_procedure_call();
            else parse_assignment_statement();
        // FOLLOW set
        case K_END: case K_ELSE: case T_SEMI_COLON: // if statement, for loop
            break;
        default: throw_error(E_INVALID_STATEMENT, look_ahead->lineNo, look_ahead->columnNo); break;
    }
    assert("Done parsing a statement");
}

void parse_indexes() {
    while (look_ahead->type == T_LBRACKET) {
        match_token(T_LBRACKET);
        parse_expression();
        match_token(T_RBRACKET);
    }
}

void parse_destination() {
    assert("Parsing a destination");
    Entry *entry;
    entry = check_declared_destination(current_token->val.stringVal);
    if (entry->entryType == ET_VARIABLE) parse_indexes();

    assert("Done parsing a destination");
}

void parse_assignment_statement() {
    assert("Parsing an assignment statement");
    parse_destination();
    if (look_ahead->type == T_LPAREN) return; // backtrack to parse procedure call
    match_token(T_ASSIGNMENT);
    parse_expression();
    assert("Done parsing an assignment statement");
}

void parse_if_statement() {
    assert("Parsing an if statement");
    match_token(K_IF);
    match_token(T_LPAREN);
    parse_expression();
    match_token(T_RPAREN);
    match_token(K_THEN);
    parse_statement_list();
    if (look_ahead->type == K_ELSE) {
        match_token(K_ELSE);
        parse_statement_list();
    }
    match_token(K_END);
    match_token(K_IF);
    assert("Done parsing an if statement");
}

void parse_loop_statement() {
    assert("Parsing a for loop");
    match_token(K_FOR);
    match_token(T_LPAREN);
    parse_assignment_statement();
    match_token(T_SEMI_COLON);
    parse_expression();
    match_token(T_RPAREN);
    if (look_ahead->type != K_END) {
        parse_statement_list();
    }
    match_token(K_END);
    match_token(K_FOR);
    assert("Done parsing a for loop");
}

void parse_return_statement() {
    match_token(K_RETURN);
}

void parse_procedure_call() {
    assert("Parsing a procedure call");
    Entry *entry = check_declared_procedure(current_token->val.stringVal);
    match_token(T_LPAREN);
    parse_argument_list();
    match_token(T_RPAREN);
    assert("Done parsing a procedure call");
}

void parse_param_list() {
    if (look_ahead->type == T_LPAREN) {
        match_token(T_LPAREN);
        parse_param();
        while (look_ahead->type == T_COMMA) {
            match_token(T_COMMA);
            parse_param();
        }
        match_token(T_RPAREN);
    }
}

void parse_param() {
    assert("Parsing a parameter");
    Entry *entry = NULL;
    parse_var_declaration(&entry, 0);
    // TODO: delete the declaration of the variable in the outerscope
    entry = create_parameter_entry(entry->name, symbolTable->currentScope->parent);
    switch (look_ahead->type) {
        case K_IN:
            entry->paramAttrs->paramType = PT_IN;
            match_token(K_IN); break;
        case K_OUT:
            entry->paramAttrs->paramType = PT_OUT;
            match_token(K_OUT); break;
        case K_INOUT:
            entry->paramAttrs->paramType = PT_INOUT;
            match_token(K_INOUT); break;
        default:
            throw_error(E_INVALID_PARAM_TYPE, look_ahead->lineNo, look_ahead->columnNo); break;
    }
    declare_entry(entry);

    assert("Done parsing a parameter");
}

void parse_argument_list() {
    assert("Parsing an argument list");
    parse_expression();
    while (look_ahead->type == T_COMMA) {
        match_token(T_COMMA);
        parse_expression();
    }
    assert("Done parsing an argument list");
}

void parse_expression() {
    assert("Parsing an expression");
    if (look_ahead->type == K_NOT) match_token(K_NOT);
    parse_arith_op();
    parse_expression_arith_op();
    assert("Done parsing an expression");
}

void parse_expression_arith_op() {
    switch(look_ahead->type) {
        case T_AND:
            match_token(T_AND);
            parse_arith_op();
            parse_expression_arith_op();
            break;
        case T_OR:
            match_token(T_OR);
            parse_arith_op();
            parse_expression_arith_op();
            break;
        // FOLLOW set
        case T_COMMA: // argument list
        case T_RPAREN: // for loop, if statement
        case T_RBRACKET: // assignment statement
        case T_SEMI_COLON: // statements
            break;
        default: throw_error(E_INVALID_EXPRESSION, look_ahead->lineNo, look_ahead->columnNo);
    }
}

void parse_arith_op() {
    assert("Parsing an arithmetic operation");
    parse_relation();
    parse_arith_op_relation();
    assert("Done parsing an arithmetic operation");
}

void parse_arith_op_relation() {
    switch(look_ahead->type) {
        case T_PLUS:
            match_token(T_PLUS);
            parse_relation();
            parse_arith_op_relation();
            break;
        case T_MINUS:
            match_token(T_MINUS);
            parse_relation();
            parse_arith_op_relation();
            break;
        // FOLLOW set
        case T_AND: case T_OR: case T_COMMA: // expression
        case T_RPAREN: // for loop, if statement
        case T_RBRACKET: // assignment statement
        case T_SEMI_COLON: // statements
            break;
        default: throw_error(E_INVALID_ARITH_OPERATOR, look_ahead->lineNo, look_ahead->columnNo); break;
    }
}

void parse_relation() {
    assert("Parsing a relation");
    parse_term();
    parse_relation_term();
    assert("Done parsing a relation");
}

void parse_relation_term() {
    switch(look_ahead->type) {
        case T_LT:
            match_token(T_LT);
            parse_term();
            parse_relation_term();
            break;
        case T_GTEQ:
            match_token(T_GTEQ);
            parse_term();
            parse_relation_term();
            break;
        case T_LTEQ:
            match_token(T_LTEQ);
            parse_term();
            parse_relation_term();
            break;
        case T_GT:
            match_token(T_GT);
            parse_term();
            parse_relation_term();
            break;
        case T_EQ:
            match_token(T_EQ);
            parse_term();
            parse_relation_term();
            break;
        case T_NEQ:
            match_token(T_NEQ);
            parse_term();
            parse_relation_term();
            break;
        // FOLLOW set
        case T_PLUS: case T_MINUS: // arith op
        case T_AND: case T_OR: case T_COMMA: // expression
        case T_RPAREN: // for loop, if statement
        case T_RBRACKET: // assignment statement
        case T_SEMI_COLON: // statements
            break;
        default: throw_error(E_INVALID_RELATION, look_ahead->lineNo, look_ahead->columnNo); break;
    }
}

void parse_term() {
    assert("Parsing a term");
    parse_factor();
    parse_term_factor();
    assert("Done parsing a term");
}

void parse_term_factor() {
    switch(look_ahead->type) {
        case T_MULTIPLY:
            match_token(T_MULTIPLY);
            parse_factor();
            parse_term_factor();
            break;
        case T_DIVIDE:
            match_token(T_DIVIDE);
            parse_factor();
            parse_term_factor();
            break;
        // FOLLOW set
        case T_LT: case T_LTEQ: case T_GT: case T_GTEQ: case T_EQ: case T_NEQ: // relation
        case T_PLUS: case T_MINUS: // arith op
        case T_AND: case T_OR: case T_COMMA: // expression
        case T_RPAREN: // for loop, if statement
        case T_RBRACKET: // assignment statement
        case T_SEMI_COLON: // statements
            break;
        default: throw_error(E_INVALID_TERM, look_ahead->lineNo, look_ahead->columnNo); break;
    }
}

void parse_factor() {
    assert("Parsing a factor");
    Entry *entry;
    switch (look_ahead->type) {
        case T_STRING:
            match_token(T_STRING); break;
        case T_CHAR:
            match_token(T_CHAR); break;
        case T_NUMBER_INT:
            match_token(T_NUMBER_INT); break;
        case T_NUMBER_FLOAT:
            match_token(T_NUMBER_FLOAT); break;
        case T_LPAREN: // ( <expression> )
            match_token(T_LPAREN);
            parse_expression();
            match_token(T_RPAREN);
            break;
        case T_MINUS: // [-] <name> | [-] <number>
            match_token(T_MINUS);
            assert("A negative number of a negative name");
            parse_factor();
            break;
        case T_IDENTIFIER: // <name> ::= <identifier> [ [ <expression> ] ]. Same as <destination> ::= <identifier> [ [ <expression> ] ]?
            match_token(T_IDENTIFIER);
            entry = check_declared_identifier(current_token->val.stringVal);
            parse_destination();
            break;
        case K_TRUE:
            match_token(K_TRUE); break;
        case K_FALSE:
            match_token(K_FALSE); break;
        // FOLLOW set
        case T_AND: case T_OR: case T_COMMA: // expression
        case T_RPAREN: // for loop, if statement
        case T_RBRACKET: // assignment statement
        case T_SEMI_COLON: // statements
            break;
        default: throw_error(E_INVALID_FACTOR, look_ahead->lineNo, look_ahead->columnNo); break;
    }
    assert("Done parsing a factor");
}
