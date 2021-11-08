// Common Expression Language grammar for C++
// Based on Java grammar with the following changes:
// - rename grammar from CEL to Cel to generate C++ style compatible names.

grammar Cel;

// Grammar Rules
// =============

start
    : e=expr EOF
    ;

expr
    : e=conditionalOr (op='?' e1=conditionalOr ':' e2=expr)?
    ;

conditionalOr
    : e=conditionalAnd (ops+='||' e1+=conditionalAnd)*
    ;

conditionalAnd
    : e=relation (ops+='&&' e1+=relation)*
    ;

relation
    : calc
    | relation op=('<'|'<='|'>='|'>'|'=='|'!='|'in') relation
    ;

calc
    : unary
    | calc op=('*'|'/'|'%') calc
    | calc op=('+'|'-') calc
    ;

unary
    : member                                                      # MemberExpr
    | (ops+='!')+ member                                          # LogicalNot
    | (ops+='-')+ member                                          # Negate
    ;

member
    : primary                                                     # PrimaryExpr
    | member op='.' id=IDENTIFIER (open='(' args=exprList? ')')?  # SelectOrCall
    | member op='[' index=expr ']'                                # Index
    | member op='{' entries=fieldInitializerList? ','? '}'        # CreateMessage
    ;

primary
    : leadingDot='.'? id=IDENTIFIER (op='(' args=exprList? ')')?  # IdentOrGlobalCall
    | '(' e=expr ')'                                              # Nested
    | op='[' elems=exprList? ','? ']'                             # CreateList
    | op='{' entries=mapInitializerList? ','? '}'                 # CreateStruct
    | literal                                                     # ConstantLiteral
    ;

exprList
    : e+=expr (',' e+=expr)*
    ;

fieldInitializerList
    : fields+=IDENTIFIER cols+=':' values+=expr (',' fields+=IDENTIFIER cols+=':' values+=expr)*
    ;

mapInitializerList
    : keys+=expr cols+=':' values+=expr (',' keys+=expr cols+=':' values+=expr)*
    ;

literal
    : sign=MINUS? tok=NUM_INT   # Int
    | tok=NUM_UINT  # Uint
    | sign=MINUS? tok=NUM_FLOAT # Double
    | tok=STRING    # String
    | tok=BYTES     # Bytes
    | tok=CEL_TRUE  # BoolTrue
    | tok=CEL_FALSE # BoolFalse
    | tok=NUL       # Null
    ;

// Lexer Rules
// ===========

EQUALS : '==';
NOT_EQUALS : '!=';
LESS : '<';
LESS_EQUALS : '<=';
GREATER_EQUALS : '>=';
GREATER : '>';
LOGICAL_AND : '&&';
LOGICAL_OR : '||';

LBRACKET : '[';
RPRACKET : ']';
LBRACE : '{';
RBRACE : '}';
LPAREN : '(';
RPAREN : ')';
DOT : '.';
COMMA : ',';
MINUS : '-';
EXCLAM : '!';
QUESTIONMARK : '?';
COLON : ':';
PLUS : '+';
STAR : '*';
SLASH : '/';
PERCENT : '%';
CEL_TRUE : 'true';
CEL_FALSE : 'false';
NUL : 'null';

fragment BACKSLASH : '\\';
fragment LETTER : 'A'..'Z' | 'a'..'z' ;
fragment DIGIT  : '0'..'9' ;
fragment EXPONENT : ('e' | 'E') ( '+' | '-' )? DIGIT+ ;
fragment HEXDIGIT : ('0'..'9'|'a'..'f'|'A'..'F') ;
fragment RAW : 'r' | 'R';

fragment ESC_SEQ
    : ESC_CHAR_SEQ
    | ESC_BYTE_SEQ
    | ESC_UNI_SEQ
    | ESC_OCT_SEQ
    ;

fragment ESC_CHAR_SEQ
    : BACKSLASH ('a'|'b'|'f'|'n'|'r'|'t'|'v'|'"'|'\''|'\\'|'?'|'`')
    ;

fragment ESC_OCT_SEQ
    : BACKSLASH ('0'..'3') ('0'..'7') ('0'..'7')
    ;

fragment ESC_BYTE_SEQ
    : BACKSLASH ( 'x' | 'X' ) HEXDIGIT HEXDIGIT
    ;

fragment ESC_UNI_SEQ
    : BACKSLASH 'u' HEXDIGIT HEXDIGIT HEXDIGIT HEXDIGIT
    | BACKSLASH 'U' HEXDIGIT HEXDIGIT HEXDIGIT HEXDIGIT HEXDIGIT HEXDIGIT HEXDIGIT HEXDIGIT
    ;

WHITESPACE : ( '\t' | ' ' | '\r' | '\n'| '\u000C' )+ -> channel(HIDDEN) ;
COMMENT : '//' (~'\n')* -> channel(HIDDEN) ;

NUM_FLOAT
  : ( DIGIT+ ('.' DIGIT+) EXPONENT?
    | DIGIT+ EXPONENT
    | '.' DIGIT+ EXPONENT?
    )
  ;

NUM_INT
  : ( DIGIT+ | '0x' HEXDIGIT+ );

NUM_UINT
   : DIGIT+ ( 'u' | 'U' )
   | '0x' HEXDIGIT+ ( 'u' | 'U' )
   ;

STRING
  : '"' (ESC_SEQ | ~('\\'|'"'|'\n'|'\r'))* '"'
  | '\'' (ESC_SEQ | ~('\\'|'\''|'\n'|'\r'))* '\''
  | '"""' (ESC_SEQ | ~('\\'))*? '"""'
  | '\'\'\'' (ESC_SEQ | ~('\\'))*? '\'\'\''
  | RAW '"' ~('"'|'\n'|'\r')* '"'
  | RAW '\'' ~('\''|'\n'|'\r')* '\''
  | RAW '"""' .*? '"""'
  | RAW '\'\'\'' .*? '\'\'\''
  ;

BYTES : ('b' | 'B') STRING;

IDENTIFIER : (LETTER | '_') ( LETTER | DIGIT | '_')*;
