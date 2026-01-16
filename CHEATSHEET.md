# Puncta v1.0.8 Cheatsheet

## 命令行参数

`--version`: 查看版本

`--log-level=LEVEL`: 设置最低日志等级(`DEBUG/INFO/WARNING/ERROR/FATAL`)

`--log-file=PATH`: 设置日志输出文件

## 基本数据类型

```c
typedef struct Number {
    union {
        long long int_value;
        double    float_value;
    };
    char extra[EXTRA_LEN];
    bool is_extra;
    bool is_float;
} Number;
```

基本类型为整数和浮点数，而字符串实质是以小端序存储在64位整数中的二进制序列，加上由于字节对齐多出来的padding，最大可用长度为14字节

## 标准库 Built-in Actions

动作调用形式：`var, action!`

`inc`: 变量自增1

`dec`: 变量自减1

`double`: 变量翻倍

`halve`: 变量减半

`neg`: 变量符号翻转

`abs`: 变量取绝对值

`not`: 变量逻辑否

`isodd`: 判断是否为奇数，返回整数true=1/false=0

`isneg`: 判断是否为负数，返回整数true=1/false=0

`toint`: 强制转化变量为整数

`input`: 从命令行输入一个数

`print`: 输出变量数值，换行

`putn`: 输出变量数值，不换行

`putc`: 输出变量为字符，不换行

`puts`: 输出变量为字符串，不换行

`putl`: 输出变量为字符串，换行

`putx`: 输出变量为十六进制整数，不换行

`getn`: 输入变量数值

`getc`: 输入变量为字符

`gets`: 输入变量为字符串

`getx`: 输入变量为十六进制整数

`eval`: 接收字符串变量（一个表达式，长度最长为14），返回浮点数计算结果到变量

### `eval` 语法

常规的中缀表达式计算，使用单字符运算符、单字符变量（a-zA-Z）、单字符数字（0-9），计算结果为`double`类型

#### 算术运算

`+-*/`: 标准浮点数加减乘除运算

`%`: 取模，先将两个操作数转化为整数再计算，效果等同于C语言的`%`

`^`: 乘方运算，要求底数和指数均为正数，右结合

#### 比较运算

返回true=1.0/false=0.0

`>`: 大于

`<`: 小于

`=`: 等于

`#`: 不等于

#### 逻辑运算

返回true=1.0/false=0.0

`&|!`: 与、或、非，优先级为`!` > `&` > `or`

#### 三目运算符

返回`double`值

`cond ? a : b`: 用法类似C语言的三目运算符，右结合

## Action C API

可以用C语言编写act扩展函数并注册到虚拟机中

函数签名：

```c
typedef void (Action *)(VM *vm, Number *n);

void register_act(VM *vm, const char *name, Action act);
```

使用方法：

1. 先`#define COC_IMPLEMENTATION`，再`#include "puncta.h"`

2. 使用`coc_log_init()`初始化日志系统

3. 按照约定的格式定义一个action，建议使用`act_xxx`的命名方式，然后定义一个`void register_user_actions(VM *vm)`函数，在其中使用`register_act()`注册自定义的action

4. 使用`run_file(const char *filename, void (*register_user_actions)(VM *))`时，将自定义的`void register_user_actions(VM *vm)`传入第二个参数
