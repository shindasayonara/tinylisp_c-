// main.cpp
#include "tinylisp.hpp"
#include <iostream>

int main() {
    TinyLisp lisp;
    
    // Тестирование базовых выражений
    std::cout << "Testing TinyLisp..." << std::endl;
    
    // Простые арифметические операции
    std::cout << lisp.eval("(+ 2 3)") << std::endl;
    std::cout << lisp.eval("(* 4 5)") << std::endl;
    
    // Списки
    std::cout << lisp.eval("(cons 1 (cons 2 nil))") << std::endl;
    
    // Запуск REPL
    lisp.repl();
    
    return 0;
}