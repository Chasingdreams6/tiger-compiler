#ifndef TIGER_LEX_SCANNER_H_
#define TIGER_LEX_SCANNER_H_

#include <algorithm>
#include <cstdarg>
#include <iostream>
#include <string>
#include <stack>

#include "scannerbase.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/parse/parserbase.h"

class Scanner : public ScannerBase {
public:
  Scanner() = delete;
  explicit Scanner(std::string_view fname, std::ostream &out = std::cout)
      : ScannerBase(std::cin, out), comment_level_(1), char_pos_(1),
        errormsg_(std::make_unique<err::ErrorMsg>(fname)) {
    switchStreams(errormsg_->infile_, out);
  }

  /**
   * Output an error
   * @param message error message
   */
  void Error(int pos, std::string message, ...) {
    va_list ap;
    va_start(ap, message);
    errormsg_->Error(pos, message, ap);
    va_end(ap);
  }

  /**
   * Getter for `tok_pos_`
   */
  [[nodiscard]] int GetTokPos() const { return errormsg_->GetTokPos(); }

  /**
   * Transfer the ownership of `errormsg_` to the outer scope
   * @return unique pointer to errormsg
   */
  [[nodiscard]] std::unique_ptr<err::ErrorMsg> TransferErrormsg() {
    return std::move(errormsg_);
  }

  int lex();

private:
  int comment_level_;
  std::string string_buf_;
  std::string store_tmp;
  int char_pos_;
  std::unique_ptr<err::ErrorMsg> errormsg_;
  std::stack<StartCondition__> d_scStack;
  /**
   * NOTE: do not change all the funtion signature below, which is used by
   * flexc++ internally
   */
  int lex__();
  int executeAction__(size_t ruleNr);

  void push(StartCondition__ );
  void popStartCondition();
  void print();
  void preCode();
  void postCode(PostEnum__ type);
  void adjust();
  void adjustStr();
  void adjustX(int );
  void storeString(std::string);
  std::string recoverString();
};

inline int Scanner::lex() { return lex__(); }

inline void Scanner::storeString(std::string x) {store_tmp = x;}

inline std::string Scanner::recoverString() {return store_tmp;}

inline void Scanner::push(StartCondition__ next) {
  //errormsg_->Error(errormsg_->tok_pos_, "push");
  d_scStack.push(startCondition());
  begin(next);
}
inline void Scanner::popStartCondition() {
  //errormsg_->Error(errormsg_->tok_pos_, "pop");
  begin(d_scStack.top());
  d_scStack.pop();
}
inline void Scanner::preCode() {
  // Optionally replace by your own code
}

inline void Scanner::postCode(PostEnum__ type) {
  // Optionally replace by your own code
}

inline void Scanner::print() { print__(); }

inline void Scanner::adjust() {
  errormsg_->tok_pos_ = char_pos_;
  char_pos_ += length();
}

inline void Scanner::adjustX(int t) { char_pos_ += t; }
inline void Scanner::adjustStr() { char_pos_ += length(); }

#endif // TIGER_LEX_SCANNER_H_
