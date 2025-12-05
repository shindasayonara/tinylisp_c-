// tinylisp.cpp
#include "tinylisp.hpp"
#include <iostream>
#include <sstream>
#include <cctype>

TinyLisp::TinyLisp(size_t memory_size) 
    : memory(memory_size * 2, 0.0f),  // Удваиваем для запаса
      heap_ptr(0),
      stack_ptr(memory_size),
      lookahead(' '),
      error_occurred(false) {
    
    // Инициализация системных значений
    nil_value = box(NIL_TAG, 0);
    true_value = allocate_atom("#t");
    error_value = allocate_atom("error");
    environment = make_env(nil_value, nil_value, nil_value);
    
    // Инициализация примитивных функций
    initialize_primitives();
    initialize_parser();
}

void TinyLisp::initialize_primitives() {
    primitives = {
        {"eval", &TinyLisp::prim_eval},
        {"quote", &TinyLisp::prim_quote},
        {"cons", &TinyLisp::prim_cons},
        {"car", &TinyLisp::prim_car},
        {"cdr", &TinyLisp::prim_cdr},
        {"+", &TinyLisp::prim_add},
        {"-", &TinyLisp::prim_sub},
        {"*", &TinyLisp::prim_mul},
        {"/", &TinyLisp::prim_div},
        {"<", &TinyLisp::prim_lt},
        {"eq?", &TinyLisp::prim_eq},
        {"pair?", &TinyLisp::prim_pair},
        {"or", &TinyLisp::prim_or},
        {"and", &TinyLisp::prim_and},
        {"not", &TinyLisp::prim_not},
        {"cond", &TinyLisp::prim_cond},
        {"if", &TinyLisp::prim_if},
        {"define", &TinyLisp::prim_define},
        {"lambda", &TinyLisp::prim_lambda},
        {"let*", &TinyLisp::prim_let}
    };
    
    // Добавление примитивов в окружение
    for (size_t i = 0; i < primitives.size(); ++i) {
        Value prim_symbol = allocate_atom(primitives[i].name);
        Value prim_value = allocate_primitive(i);
        environment = env_define(prim_symbol, prim_value, environment);
    }
    
    // Определение #t
    environment = env_define(true_value, true_value, environment);
}

// ========== NaN-БОКСИРОВАНИЕ ==========
Value TinyLisp::box(Index tag, Index index) const {
    Value result;
    uint32_t packed = (tag << 20) | (index & 0xFFFFF);
    std::memcpy(&result, &packed, sizeof(result));
    return result;
}

Index TinyLisp::get_tag(Value value) const {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits >> 20;
}

Index TinyLisp::get_index(Value value) const {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits & 0xFFFFF;
}

bool TinyLisp::is_type(Value value, Index tag) const {
    return get_tag(value) == tag;
}

// ========== УПРАВЛЕНИЕ ПАМЯТЬЮ ==========
Value TinyLisp::allocate_atom(const std::string& name) {
    // Поиск существующего атома
    for (size_t i = 0; i < string_heap.size(); ) {
        size_t len = std::strlen(&string_heap[i]);
        if (name == &string_heap[i]) {
            return box(ATOM_TAG, i);
        }
        i += len + 1;
    }
    
    // Выделение нового атома
    check_memory();
    Index index = string_heap.size();
    string_heap.insert(string_heap.end(), name.begin(), name.end());
    string_heap.push_back('\0');
    
    return box(ATOM_TAG, index);
}

Value TinyLisp::allocate_cons(Value car_val, Value cdr_val) {
    check_memory();
    
    if (stack_ptr < 2) {
        garbage_collect();
        if (stack_ptr < 2) {
            expand_memory();
        }
    }
    
    stack_ptr -= 2;
    memory[stack_ptr] = car_val;
    memory[stack_ptr + 1] = cdr_val;
    
    return box(CONS_TAG, stack_ptr);
}

Value TinyLisp::allocate_closure(Value vars, Value body, Value env) {
    Value closure_data = allocate_cons(vars, allocate_cons(body, env));
    return box(CLOS_TAG, get_index(closure_data));
}

Value TinyLisp::allocate_primitive(Index index) {
    return box(PRIM_TAG, index);
}

// ========== ОПЕРАЦИИ С ПАРАМИ ==========
Value TinyLisp::car(Value pair) const {
    if (!is_cons(pair)) {
        throw std::runtime_error("car: expected pair");
    }
    Index idx = get_index(pair);
    if (idx >= memory.size()) {
        throw std::runtime_error("car: invalid memory index");
    }
    return memory[idx];
}

Value TinyLisp::cdr(Value pair) const {
    if (!is_cons(pair)) {
        throw std::runtime_error("cdr: expected pair");
    }
    Index idx = get_index(pair);
    if (idx + 1 >= memory.size()) {
        throw std::runtime_error("cdr: invalid memory index");
    }
    return memory[idx + 1];
}

// ========== ВЫЧИСЛЕНИЕ ==========
Value TinyLisp::evaluate(Value expression, Value env) {
    try {
        if (is_atom(expression)) {
            // Переменная - поиск в окружении
            Value result = env_lookup(expression, env);
            if (is_nil(result)) {
                error("undefined variable");
                return error_value;
            }
            return result;
        }
        else if (is_cons(expression)) {
            // Применение функции
            Value function = evaluate(car(expression), env);
            Value args = cdr(expression);
            
            return apply(function, args, env);
        }
        else {
            // Самовычисляемые значения
            return expression;
        }
    }
    catch (const std::exception& e) {
        error(e.what());
        return error_value;
    }
}

Value TinyLisp::apply(Value function, Value args, Value env) {
    if (is_primitive(function)) {
        Index prim_index = get_index(function);
        if (prim_index >= primitives.size()) {
            error("invalid primitive function index");
            return error_value;
        }
        return (this->*primitives[prim_index].function)(args, env);
    }
    else if (is_closure(function)) {
        Value closure_data = box(CONS_TAG, get_index(function));
        Value vars = car(closure_data);
        Value body_env = cdr(closure_data);
        Value body = car(body_env);
        Value closure_env = cdr(body_env);
        
        // Вычисление аргументов
        Value arg_values = eval_list(args, env);
        
        // Создание нового окружения
        Value new_env = make_env(vars, arg_values, closure_env);
        
        return evaluate(body, new_env);
    }
    else {
        error("application: expected function");
        return error_value;
    }
}

// ========== ПРИМИТИВНЫЕ ФУНКЦИИ ==========
Value TinyLisp::prim_quote(Value args, Value env) {
    return car(args);
}

Value TinyLisp::prim_cons(Value args, Value env) {
    Value first = evaluate(car(args), env);
    Value second = evaluate(car(cdr(args)), env);
    return allocate_cons(first, second);
}

Value TinyLisp::prim_car(Value args, Value env) {
    Value pair = evaluate(car(args), env);
    return car(pair);
}

// ... остальные примитивные функции

// ========== ИНТЕРФЕЙС ==========
std::string TinyLisp::eval(const std::string& code) {
    error_occurred = false;
    
    try {
        // Парсинг
        std::istringstream stream(code);
        // ... парсинг выражения
        
        Value result = evaluate(parsed_expression, environment);
        return value_to_string(result);
    }
    catch (const std::exception& e) {
        error_occurred = true;
        return "Error: " + std::string(e.what());
    }
}

void TinyLisp::repl() {
    std::string line;
    std::cout << "TinyLisp C++ Version" << std::endl;
    
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        
        if (line.empty()) continue;
        
        std::string result = eval(line);
        std::cout << result << std::endl;
        
        if (had_error()) {
            reset();
        }
    }
}