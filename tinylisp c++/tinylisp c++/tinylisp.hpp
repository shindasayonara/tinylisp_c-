// tinylisp.hpp
#ifndef TINYLISP_HPP
#define TINYLISP_HPP

#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <stdexcept>

class TinyLisp {
public:
    // Конструктор: выделяет память под стек и кучу
    TinyLisp(size_t memory_size = 1024 * 1024);

    // Главный метод: вычисляет код Lisp и возвращает строку-результат
    std::string eval(const std::string& code);

    // Запуск интерактивной оболочки
    void repl();

private:
    // --- Типы данных ---
    using Value = float;
    using Index = uint32_t;

    // Теги типов (как в оригинальном C-файле)
    static constexpr Index ATOM_TAG = 0x7fc;
    static constexpr Index PRIM_TAG = 0x7fd;
    static constexpr Index CONS_TAG = 0x7fe;
    static constexpr Index CLOS_TAG = 0x7ff;
    static constexpr Index NIL_TAG = 0xfff;

    // --- Состояние интерпретатора ---
    std::vector<Value> memory;    // Основная память (cell)
    std::vector<char> string_heap; // Куча для строк (имен атомов)
    Index sp; // Stack Pointer
    Index hp; // Heap Pointer (индекс в string_heap)

    // Глобальные регистры (теперь члены класса)
    Value nil_value;
    Value true_value;
    Value error_value;
    Value env; // Текущее окружение

    // --- Состояние Парсера ---
    std::string input_buffer;
    size_t input_pos;
    char lookahead;
    char token_buffer[128];

    // --- Структура примитива ---
    struct Primitive {
        const char* name;
        Value(TinyLisp::* func)(Value, Value);
    };
    std::vector<Primitive> primitives;

    // --- NaN Boxing / Unboxing ---
    Value box(Index tag, Index index) const;
    Index get_tag(Value val) const;
    Index get_index(Value val) const; // Он же ord()
    Index get_type_bits(Value val) const; // T(x)

    // --- Базовые операции (аналоги C-макросов и функций) ---
    Value num(float n);
    bool equ(Value x, Value y);
    bool not_val(Value x);
    Value atom(const char* s);
    Value cons(Value x, Value y);
    Value car(Value p);
    Value cdr(Value p);
    Value pair(Value v, Value x, Value e);
    Value closure(Value v, Value x, Value e);
    Value assoc(Value v, Value e);

    // --- Ядро Lisp ---
    Value eval_expr(Value x, Value e); // eval
    Value evlis(Value t, Value e);
    Value apply(Value f, Value t, Value e);
    Value bind(Value v, Value t, Value e);
    Value reduce(Value f, Value t, Value e);

    // --- Примитивные функции (реализация f_*) ---
    Value p_eval(Value t, Value e);
    Value p_quote(Value t, Value e);
    Value p_cons(Value t, Value e);
    Value p_car(Value t, Value e);
    Value p_cdr(Value t, Value e);
    Value p_add(Value t, Value e);
    Value p_sub(Value t, Value e);
    Value p_mul(Value t, Value e);
    Value p_div(Value t, Value e);
    Value p_int(Value t, Value e);
    Value p_lt(Value t, Value e);
    Value p_eq(Value t, Value e);
    Value p_pair(Value t, Value e);
    Value p_or(Value t, Value e);
    Value p_and(Value t, Value e);
    Value p_not(Value t, Value e);
    Value p_cond(Value t, Value e);
    Value p_if(Value t, Value e);
    Value p_leta(Value t, Value e); // let*
    Value p_lambda(Value t, Value e);
    Value p_define(Value t, Value e);

    // --- Парсер и Принтер ---
    void setup_input(const std::string& code);
    void get_char();
    void skip_whitespace();
    char scan();
    Value read_one();
    Value parse_list();
    Value parse_quote();
    Value parse_atomic();

    std::string print(Value x);
    std::string print_list(Value t);

    // --- Управление памятью ---
    void gc();
};

#endif // TINYLISP_HPP