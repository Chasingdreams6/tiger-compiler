%filenames = "scanner"

 /*
  * Please don't modify the lines above.
  */

 /* You can add lex definitions here. */
 /* TODO: Put your lab2 code here */

%x COMMENT STR IGNORE EAT1 EAT2

%%

 /*
  * Below is examples, which you can wipe out
  * and write regular expressions and actions of your own.
  *
  * All the tokens:
  *   Parser::ID
  *   Parser::STRING
  *   Parser::INT
  *   Parser::COMMA
  *   Parser::COLON
  *   Parser::SEMICOLON
  *   Parser::LPAREN
  *   Parser::RPAREN
  *   Parser::LBRACK
  *   Parser::RBRACK
  *   Parser::LBRACE
  *   Parser::RBRACE
  *   Parser::DOT
  *   Parser::PLUS
  *   Parser::MINUS
  *   Parser::TIMES
  *   Parser::DIVIDE
  *   Parser::EQ
  *   Parser::NEQ
  *   Parser::LT
  *   Parser::LE
  *   Parser::GT
  *   Parser::GE
  *   Parser::AND
  *   Parser::OR
  *   Parser::ASSIGN
  *   Parser::ARRAY
  *   Parser::IF
  *   Parser::THEN
  *   Parser::ELSE
  *   Parser::WHILE
  *   Parser::FOR
  *   Parser::TO
  *   Parser::DO
  *   Parser::LET
  *   Parser::IN
  *   Parser::END
  *   Parser::OF
  *   Parser::BREAK
  *   Parser::NIL
  *   Parser::FUNCTION
  *   Parser::VAR
  *   Parser::TYPE
  */

 /* vars */

"/*" {
  adjust();
  push(StartCondition__::COMMENT);
}

\" {
  adjust();
  begin(StartCondition__::STR);
}

<COMMENT>{
  "/*" {
    adjust();
    push(StartCondition__::COMMENT);
  }
  "*/" {
    adjust();
    popStartCondition();
  }
  [ \t]+ {adjust();}
  \n {adjust();}
  . {adjust();}
  <<EOF>> {
    errormsg_->Error(errormsg_->tok_pos_, "unclosed comment");
  }
}

<STR>{
  /* find \", treat as a real " */
  [\\][\"] {
    setMatched(matched().substr(0, matched().length() - 2) + '\"');
    more();
    adjustX(1);
  }
  /* find \\ */
  "\\\\" {
    setMatched(matched().substr(0, matched().length() - 2) + '\\');
    more();
    adjustX(1);
  }
  /* find a " */
  \" {
    redo(1);
    adjustStr();
    begin(StartCondition__::EAT1);
    return Parser::STRING;
  }
  /* \xxx style's char*/
  \\([0-9]{3}) {
    int tmp = (int)std::strtol(matched().substr(matched().length() - 3, 3).c_str(), nullptr, 10);
    if (tmp > 127) {
      errormsg_->Error(errormsg_->tok_pos_, "\\xxx style out of range");
    }
    setMatched(matched().substr(0, matched().length() - 4) + (char)tmp);
    more();
    adjustX(3);
  }
  \\\^[A-Z] {
    char lastC = matched().substr(matched().length() - 1, 1)[0];
    int bios = lastC - 'A' + 1;
    setMatched(matched().substr(0, matched().length() - 3) + (char)bios);
    more();
    adjustX(2);
  }
  /* start ignore */
  \\[ \t\n] {
    storeString(matched().substr(0, matched().length() - 2));
    begin(StartCondition__::IGNORE);
    adjustX(2);
  }
  "\\a" {
    errormsg_->Error(errormsg_->tok_pos_, "illegal input \\a");
  }
  "\\b" {
    errormsg_->Error(errormsg_->tok_pos_, "illegal input \\b");
  }
  "\\f" {
    errormsg_->Error(errormsg_->tok_pos_, "illegal input \\f");
  }
  "\\n" {
    setMatched(matched().substr(0, matched().length() - 2) + '\n');
    more();
    adjustX(1);
  }
  "\\r" {
    errormsg_->Error(errormsg_->tok_pos_, "illegal input \\r");
  }
  "\\t" {
    setMatched(matched().substr(0, matched().length() - 2) + '\t');
    more();
    adjustX(1);
  }
  "\\v" {
    errormsg_->Error(errormsg_->tok_pos_, "illegal input \\v");
  }
  . {more();}
  <<EOF>> {
    errormsg_->Error(errormsg_->tok_pos_, "unclosed STR");
  }
}

<EAT1>{
  . {adjust(); begin(StartCondition__::INITIAL);}
  <<EOF>> {
    errormsg_->Error(errormsg_->tok_pos_, "unclosed STR");
  }
}

<EAT2>{
  [.]{2} {adjust(); begin(StartCondition__::STR);}
  <<EOF>> {
    errormsg_->Error(errormsg_->tok_pos_, "unclosed STR");
  }
}

<IGNORE>{
  \\ {
    adjustX(1);
    setMatched(recoverString());
    begin(StartCondition__::STR);
    more();
  }
  .|[ \t\n] {more();adjustX(1);}
  <<EOF>> {
    errormsg_->Error(errormsg_->tok_pos_, "unclosed trans-meaning ");
  }
}

<<EOF>> {
  adjust();
}

 /* reserved words */
"array"/[ ] {adjust(); return Parser::ARRAY;}

"if" {adjust(); return Parser::IF;}

"then" {adjust(); return Parser::THEN;}

"else" {adjust(); return Parser::ELSE;}

"while" {adjust(); return Parser::WHILE;}

"for" {adjust(); return Parser::FOR;}

"to" {adjust(); return Parser::TO;}

"do" {adjust(); return Parser::DO;}

"let" {adjust(); return Parser::LET;}

"in" {adjust(); return Parser::IN;}

"end" {adjust(); return Parser::END;}

"of" {adjust(); return Parser::OF;}

"break" {adjust(); return Parser::BREAK;}

"nil" {adjust(); return Parser::NIL;}

"function" {adjust(); return Parser::FUNCTION;}

"var" {adjust(); return Parser::VAR;}

"type" {adjust(); return Parser::TYPE;}

[a-zA-Z][a-zA-Z0-9_]* {adjust(); return Parser::ID;}

[0-9]+ {adjust(); return Parser::INT;}
 /*operators */
"," {adjust(); return Parser::COMMA;}

":" {adjust(); return Parser::COLON;}

";" {adjust(); return Parser::SEMICOLON;}

"(" {adjust(); return Parser::LPAREN;}

")" {adjust(); return Parser::RPAREN;}

"[" {adjust(); return Parser::LBRACK;}

"]" {adjust(); return Parser::RBRACK;}

"{" {adjust(); return Parser::LBRACE;}

"}" {adjust(); return Parser::RBRACE;}

"." {adjust(); return Parser::DOT;}

"+" {adjust(); return Parser::PLUS;}

"-" {adjust(); return Parser::MINUS;}

"*" {adjust(); return Parser::TIMES;}

"/" {adjust(); return Parser::DIVIDE;}

"=" {adjust(); return Parser::EQ;}

"<>" {adjust(); return Parser::NEQ;}

"<" {adjust(); return Parser::LT;}

"<=" {adjust(); return Parser::LE;}

">" {adjust(); return Parser::GT;}

">=" {adjust(); return Parser::GE;}

"&" {adjust(); return Parser::AND;}

"|" {adjust(); return Parser::OR;}

":=" {adjust(); return Parser::ASSIGN;}


 /*
  * skip white space chars.
  * space, tabs and LF
  */
[ \t]+ {adjust();}
\n {adjust(); errormsg_->Newline();}

 /* illegal input */
. {adjust(); errormsg_->Error(errormsg_->tok_pos_, "illegal token");}
