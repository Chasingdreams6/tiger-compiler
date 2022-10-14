## 如何处理注释？

1. 首先，识别"/*"，begin COMMENT start condition，进入mini-scanner.
2. 注意到注释是可以嵌套的，所以需要对start condition用一个栈。查询manual3.6找到关于使用 d_scStack来保存start condition的方法。
3. 在scanner.h中定义 push(StartCondition__) 和 popStartCondition() 函数，在lex中调用。
4. 在COMMENT中，凡是遇到 /* 则入栈，遇到 */ 则出栈。

## 如何处理字符串？

### 处理基本字符串（不带转义字符，控制字符）
1. 在INITIAL状态，识别 " 进入 STR。
2. 在STR状态，遇到 " 表明字符串终止，但是如果直接返回，字符串会多一个 " 符号。查询manual3.7发现一个redo的函数。使用redo(1)，把最后一个"退回input stream，再进入 EAT1 状态，把这个字符“吃掉”。EAT1状态中，读一个字符，调用adjust更新pos，不返回该串。

### 处理转义字符串

1. 对于 \\", \\\\,  \\n,  \\t, 等转义字符。Lex需要把他们从raw string的格式翻译成有意义的转义字符。查询manual3.7发现matched(), setMatched()函数配合字符串操作函数可以很方便地对match文本进行修改。于是手动把matched的后两个字符删掉，加转义字符。注意，由于是两个字符变成一个字符，需要调用adjustX(1)函数，手动将pos后移一位。

```cpp
"\\n" {
    setMatched(matched().substr(0, matched().length() - 2) + '\n');
    more();
    adjustX(1);
}
```

2. 对于 \\xxx 风格的转义字符，查询Appendix A得知，语义是ASCII码为xxx（十进制下）的字符。所以截取后三个数字，使用strtol函数转换为十进制，再用(char)强制转换为字符，加到matched串的后面。同样的，由于是4个字符变成1个字符，调用adjustX(3)函数，手动补3位。

```cpp
  /* \xxx style's char*/
  \\([0-9]{3}) {
    int tmp = (int)std::strtol(matched().substr(matched().length() - 3, 3).c_str(), nullptr, 10);
    if (tmp > 127) {
      errormsg_->Error(errormsg_->tok_pos_, "\\xxx style out of range");
      return ;
    }
    setMatched(matched().substr(0, matched().length() - 4) + (char)tmp);
    more();
    adjustX(3);
  }
```

3. 对于 \\^X 风格的控制字符，查询ASCII码表得知它们是连续的。因此从matched字符串中截取X，将其与A做差获得偏移量bios，将bios强制转为字符即为对应控制字符。同样调用adjustX(2)补两位。

```cpp
  \\\^[A-Z] {
    char lastC = matched().substr(matched().length() - 1, 1)[0];
    int bios = lastC - 'A' + 1;
    setMatched(matched().substr(0, matched().length() - 3) + (char)bios);
    more();
    adjustX(2);
  }
```


4. 对于 Appendix A中提到的 \\ f___ f\\ 序列。首先识别 \\ blank，进入IGNORE 中，识别 \\时，返回到STR sc。 这里的问题主要是，matched()中保存的字符串会根据每次的匹配而刷新。那么在匹配到中间被忽略字符时，之前匹配的字符会丢失。所以在进入IGNORE之前，用 storeString()将字符串保存。在离开IGNORE时，用 restoreString()与setMatched()恢复。

```cpp
/* start ignore */
  \\[ \t\n] {
    storeString(matched().substr(0, matched().length() - 2));
    begin(StartCondition__::IGNORE);
    adjustX(2);
  }
```

```cpp
<IGNORE>{
  \\ {
    adjustX(1);
    setMatched(recoverString());
    begin(StartCondition__::STR);
    more();
  }
  .|[ \t\n] {more();adjustX(1);}
}
```
## 错误处理

1. 首先，在INITIAL sc，无法被任何一个rule识别的串会报错。
```cpp
 /* illegal input */
. {adjust(); errormsg_->Error(errormsg_->tok_pos_, "illegal token");}
```

2. 在各种强制转换时，需要检查被转换的数的范围。
```cpp
/* \xxx style's char*/
  \\([0-9]{3}) {
    int tmp = (int)std::strtol(matched().substr(matched().length() - 3, 3).c_str(), nullptr, 10);
    if (tmp > 127) {
      errormsg_->Error(errormsg_->tok_pos_, "\\xxx style out of range");
      return ;
    }
    setMatched(matched().substr(0, matched().length() - 4) + (char)tmp);
    more();
    adjustX(3);
  }
```

3. tiger语言中一些转义字符的使用是非法的，如 \r, \a等。
```cpp
  "\\a" {
    errormsg_->Error(errormsg_->tok_pos_, "illegal input \\a");
    return ;
  }
```

## EOF

查询manual3.4得知，使用<\<EOF\>>来匹配EOF，在INNITIAL sc中，直接返回。因为正常情况下，EOF并不会产生什么问题，表示源文件正常结束，但是如果在STR，COMMENT语义下遇到EOF，则代表源文件没有写完就意外终止了，应该报错。

1. INITIAL sc情况下遇到EOF，无其他处理。
```cpp
<<EOF>> {
  adjust();
  return ;
}
```
2. COMMENT, STR, EAT1, EAT2  sc中遇到EOF，报 "unclosed xxx"的错误，表示语句不封闭。
```cpp
<IGNORE> {
    .... /*others statement*/
    <<EOF>> {
        errormsg_->Error(errormsg_->tok_pos_, "unclosed trans-meaning ");
        return ;
    }
}
```
```cpp
<COMMENT> {
    .... /*others statement*/
    <<EOF>> {
        errormsg_->Error(errormsg_->tok_pos_, "unclosed comment");
        return ;
    }
}
```
```cpp
<EAT1>{
  . {adjust(); begin(StartCondition__::INITIAL);}
  <<EOF>> {
    errormsg_->Error(errormsg_->tok_pos_, "unclosed STR");
    return ;
  }
}
```
## 特性

1. adjustX(int )函数可以强制将pos移动X，主要用于转义字符解析时，字符数目的增补。
```cpp
inline void Scanner::adjustX(int t) { char_pos_ += t; }
```

2. push(StartCondition__) 与 popStartCondition() 可以以栈的形式保存start condition，主要用于COMMENT的嵌套。
```cpp
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
```

3. storeString(std::string), recoverString() 可以暂时记录一个字符串，主要用于 STR sc中，被忽略字符前后串的处理。
```cpp
inline void Scanner::storeString(std::string x) {store_tmp = x;}

inline std::string Scanner::recoverString() {return store_tmp;}
```

4. EATX start condition，可以“吃掉”input stream的一个字符并不返回。主要和redo配合使用。
```cpp
<EAT1>{
  . {adjust(); begin(StartCondition__::INITIAL);}
}

<EAT2>{
  [.]{2} {adjust(); begin(StartCondition__::STR);}
}
```