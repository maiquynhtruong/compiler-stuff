#include "token.h"

TokenType next_token(Token *token);
void init_scanner(char *fileName);
TokenType check_reserved_word(char *tokenStr);
void print_token(Token *token);
void skip_star_comment();
void reset_token(Token *token);

