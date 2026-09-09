# Design Document

## Call graph

    st_t * minica_parse(FILE *fp)
    minica_parse() --> yyparse() --> st_t
    
    compiler_t * compile_code(st_t *)

