Fang Compiler (Fang 语言编译器)
这是一个基于 C++ 开发的高级语言编译器，针对自定义的 Fang 语言。它实现了从源码到 x86-64 汇编代码的完整编译链路，支持整数与浮点数混合运算、变量管理、以及输入输出交互。

📖 项目简介
本项目旨在演示编译原理的核心流程。编译器前端使用 Flex 和 Bison 进行词法与语法分析，后端生成 AT&T 格式的 x86-64 汇编代码，并调用 GCC 进行链接生成最终可执行文件。

主要特性
混合运算支持：支持 int 和 real (double) 类型的变量声明与混合算术运算，支持自动类型提升。

交互式 I/O：

print(...): 支持打印字符串常量、数值表达式（自动识别格式）。

input(...): 支持带提示语的用户输入。

中间代码生成：生成三地址码 (Three-Address Code, TAC) 用于中间表示。

可视化调试：编译过程中会输出抽象语法树 (AST) 结构和符号表信息。

底层汇编生成：生成标准的 .s 汇编文件，利用栈式内存管理和 SSE 指令集处理浮点运算。

📂 项目结构
Plaintext

.
├── lexical.l           # [前端] Flex 词法定义，处理 Token 识别
├── syntax.y            # [前端] Bison 语法定义，构建 AST
├── node.h              # [AST]  抽象语法树节点类定义 (多态结构)
├── symbol.h            # [语义] 符号表管理，处理变量类型与作用域
├── asm_generator.cpp   # [后端] 汇编代码生成器 (x86-64 AT&T)
├── main.cpp            # [驱动] 主程序入口，负责 TAC 生成和 GCC 调用
└── README.md           # 项目说明文档
🛠️ 构建与运行
环境依赖
g++ (支持 C++17)

Flex

Bison

Make (可选，建议使用)

方式一：使用 Makefile (推荐)
在项目根目录下创建一个名为 Makefile 的文件，填入以下内容：

Makefile

all: compiler

compiler: lexical.l syntax.y main.cpp asm_generator.cpp
	flex lexical.l
	bison -d syntax.y
	g++ -o compiler main.cpp lex.yy.c syntax.tab.c asm_generator.cpp -std=c++17 -Wno-register

clean:
	rm -f lex.yy.c syntax.tab.c syntax.tab.h compiler out.s out
然后在终端执行：

Bash

make
方式二：手动编译
如果不想使用 Makefile，请按顺序执行以下命令：

Bash

flex lexical.l
bison -d syntax.y
g++ -o compiler main.cpp lex.yy.c syntax.tab.c asm_generator.cpp -std=c++17
🚀 使用指南
1. 编写测试代码
创建一个名为 test.fang 的文件：

C

// test.fang
fang {
    int a;
    real b;
    
    // 基础赋值
    a = 10;
    
    // 输入测试
    b = input("请输入一个小数: ");
    
    // 混合运算与输出
    print("计算结果:", a + b * 2.0);
}
2. 运行编译器
Bash

./compiler test.fang
3. 查看结果
编译器运行成功后，会在当前目录生成：

out.s: 生成的汇编源代码。

out: 最终的可执行二进制文件。

直接运行生成的程序：

Bash

./out
🏗️ 编译器架构与设计
本项目遵循经典的编译器设计模式，数据流向如下：

1. 词法分析 (Lexical Analysis)
工具: Flex (lexical.l)

功能: 将源代码字符流转换为 Token 流。

处理: 识别关键字 (int, real, print)、标识符、数字字面量，并过滤注释 (//)。

2. 语法分析 (Syntax Analysis)
工具: Bison (syntax.y)

功能: 根据上下文无关文法 (CFG) 验证语句结构。

产物: 构建抽象语法树 (AST)。node.h 定义了 Binary (二元运算)、AssignStmt (赋值) 等节点结构。

3. 语义分析 (Semantic Analysis)
模块: symbol.h

功能: 维护全局符号表，记录变量名称与类型 (value_type)。在解析过程中进行类型检查，确保赋值和运算的合法性。

4. 中间代码生成 (IR Generation)
模块: main.cpp

形式: 三地址码 (TAC)。

示例: t0 = a + b。这层抽象解耦了具体的机器指令，便于优化和调试。

5. 目标代码生成 (Code Generation)
模块: asm_generator.cpp

策略:

栈式分配: 所有局部变量存储在 .data 段（简化实现）或栈中。

指令选择:

整数运算使用通用寄存器 (%rax, addq).

浮点运算使用 SSE 寄存器 (%xmm0, addsd).

I/O 实现: 调用 C 标准库的 printf 和 scanf。

📝 待办事项 / 已知限制
[ ] 增加 if/else 控制流支持。

[ ] 增加 while 循环支持。

[ ] 优化寄存器分配算法（当前主要依赖内存）。

👨‍💻 作者
Date: 2025

Project: Compiler Construction Coursework