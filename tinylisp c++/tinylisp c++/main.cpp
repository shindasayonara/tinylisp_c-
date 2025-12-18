// main.cpp
#include "tinylisp.hpp"
#include <iostream>
#include <vector>

int main() {
    TinyLisp lisp;

    std::cout << "=== TinyLisp C++ Translator ===\n" << std::endl;

    // Набор тестов (основан на dotcall.lisp)
    std::vector<std::string> tests = {
        "(define equal? (lambda (x y) (or (eq? x y) (and (pair? x) (pair? y) (equal? (car x) (car y)) (equal? (cdr x) (cdr y))))))",
        "(define list (lambda args args))",

        // Тест 1: + с точечной нотацией
        "(if (equal? ((lambda (l) (+ . l)) '(1 2 3)) 6) 'passed 'failed)",

        // Тест 2: -
        "(if (equal? ((lambda (l) (- . l)) '(1 2 3)) -4) 'passed 'failed)",

        // Тест 3: *
        "(if (equal? ((lambda (l) (* . l)) '(1 2 3)) 6) 'passed 'failed)",

        // Тест 4: let*
        "(if (equal? (let* (x 1) (y (+ 1 x)) (let* (z (+ x y)) z)) 3) 'passed 'failed)",

        // Тест 5: Currying
        "(if (equal? (((lambda (f x) (lambda args (f x . args))) + 1) 2 3) 6) 'passed 'failed)",

        // Тест 6: Caller dot
        "(if (equal? ((lambda (l) ((lambda (x y z) (list x y z)) '(1) '(2) . l)) '((3))) '((1) (2) (3))) 'passed 'failed)"
    };

    std::cout << "Running automated tests...\n";
    for (const auto& test : tests) {
        std::cout << "Test: " << test << "\nResult: ";
        try {
            std::cout << lisp.eval(test) << std::endl;
        }
        catch (const std::exception& e) {
            std::cout << "Runtime Error: " << e.what() << std::endl;
        }
        std::cout << "------------------------\n";
    }

    // Запуск REPL для ручного ввода
    std::cout << "\nStarting REPL (enter expressions):\n";
    lisp.repl();

    return 0;
}