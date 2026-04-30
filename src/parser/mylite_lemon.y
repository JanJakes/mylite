%name MyLiteLemon
%token_prefix ML_
%token_type {MyliteToken}
%default_type {MyliteToken}
%extra_argument {MyliteParseContext *ctx}
%token_destructor { (void)ctx; (void)yypminor; }
%default_destructor { (void)ctx; (void)yypminor; }

%include {
#include "mylite_parser_internal.h"
}

%syntax_error {
  mylite_parser_syntax_error(ctx, yymajor, TOKEN);
}

%parse_failure {
  mylite_parser_failure(ctx);
}

%parse_accept {
  mylite_parser_accept(ctx);
}

input ::= . { (void)ctx; }
input ::= token_list. { (void)ctx; }

token_list ::= token_item.
token_list ::= token_list token_item.

token_item ::= ATOM.
token_item ::= SEMI.
token_item ::= LP.
token_item ::= RP.
token_item ::= LB.
token_item ::= RB.
token_item ::= LC.
token_item ::= RC.
