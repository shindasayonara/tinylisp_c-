// tinylisp.hpp
#ifndef TINYLISP_HPP
#define TINYLISP_HPP

#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <cstdint>
#include <unordered_map>

class TinyLisp {
public:
    TinyLisp(size_t memory_size = 1024);
    
    // Основной интерфейс
    std::string eval(const std::string& code);
    void repl();
    
    // Состояние интерпретатора
    bool had_error() const { return error_occurred; }
    void reset();
    
private:
    // ========== ТИПЫ И КОНСТАНТЫ ==========
    using Value = float;
    using Index = uint32_t;
    
    // Теги типов (соответствуют оригинальным)
    static constexpr Index ATOM_TAG  = 0x7fc;
    static constexpr Index PRIM_TAG  = 0x7fd; 
    static constexpr Index CONS_TAG  = 0x7fe;
    static constexpr Index CLOS_TAG  = 0x7ff;
    static constexpr Index NIL_TAG   = 0xfff;
    
    // ========== ВНУТРЕННЕЕ СОСТОЯНИЕ ==========
    // Память (замена глобальному массиву cell)
    std::vector<Value> memory;
    Index heap_ptr;    // замена hp
    Index stack_ptr;   // замена sp
    
    // Системные константы
    Value nil_value;
    Value true_value;
    Value error_value;
    Value environment; // замена env
    
    // Строковая куча для атомов
    std::vector<char> string_heap;
    
    // Состояние парсера
    char lookahead;
    char scan_buffer[40];
    bool error_occurred;
    
    // Таблица примитивных функций
    struct Primitive {
        const char* name;
        Value (TinyLisp::*function)(Value args, Value env);
    };
    std::vector<Primitive> primitives;
    
    // ========== СИСТЕМНЫЕ ФУНКЦИИ ==========
    // NaN-боксирование
    Value box(Index tag, Index index) const;
    Index get_tag(Value value) const;
    Index get_index(Value value) const;
    bool is_type(Value value, Index tag) const;
    
    // Базовые проверки типов
    bool is_atom(Value value) const { return is_type(value, ATOM_TAG); }
    bool is_cons(Value value) const { return is_type(value, CONS_TAG); }
    bool is_closure(Value value) const { return is_type(value, CLOS_TAG); }
    bool is_primitive(Value value) const { return is_type(value, PRIM_TAG); }
    bool is_nil(Value value) const { return get_tag(value) == NIL_TAG; }
    bool is_number(Value value) const { 
        return get_tag(value) != ATOM_TAG && 
               get_tag(value) != PRIM_TAG && 
               get_tag(value) != CONS_TAG && 
               get_tag(value) != CLOS_TAG && 
               get_tag(value) != NIL_TAG;
    }
    
    // Операции с памятью
    Value allocate_atom(const std::string& name);
    Value allocate_cons(Value car, Value cdr);
    Value allocate_closure(Value vars, Value body, Value env);
    Value allocate_primitive(Index index);
    
    // Доступ к данным
    Value car(Value pair) const;
    Value cdr(Value pair) const;
    void set_car(Value pair, Value value);
    void set_cdr(Value pair, Value value);
    
    // Управление окружением
    Value make_env(Value vars, Value values, Value parent_env);
    Value env_lookup(Value symbol, Value env) const;
    Value env_define(Value symbol, Value value, Value env);
    
    // Вычисление
    Value evaluate(Value expression, Value env);
    Value apply(Value function, Value args, Value env);
    Value eval_list(Value list, Value env);
    
    // Примитивные функции
    Value prim_eval(Value args, Value env);
    Value prim_quote(Value args, Value env);
    Value prim_cons(Value args, Value env);
    Value prim_car(Value args, Value env);
    Value prim_cdr(Value args, Value env);
    Value prim_add(Value args, Value env);
    Value prim_sub(Value args, Value env);
    Value prim_mul(Value args, Value env);
    Value prim_div(Value args, Value env);
    Value prim_lt(Value args, Value env);
    Value prim_eq(Value args, Value env);
    Value prim_pair(Value args, Value env);
    Value prim_or(Value args, Value env);
    Value prim_and(Value args, Value env);
    Value prim_not(Value args, Value env);
    Value prim_cond(Value args, Value env);
    Value prim_if(Value args, Value env);
    Value prim_define(Value args, Value env);
    Value prim_lambda(Value args, Value env);
    Value prim_let(Value args, Value env);
    
    // Парсинг
    void initialize_parser();
    char get_char();
    void skip_whitespace();
    char scan_token();
    Value parse_expression();
    Value parse_list();
    Value parse_quote();
    Value parse_atom();
    
    // Вывод
    std::string value_to_string(Value value) const;
    std::string list_to_string(Value list) const;
    
    // Управление памятью
    void garbage_collect();
    void check_memory();
    void expand_memory();
    
    // Ошибки
    void error(const std::string& message);
    Value type_error(const std::string& expected, Value actual) const;
};

#endif