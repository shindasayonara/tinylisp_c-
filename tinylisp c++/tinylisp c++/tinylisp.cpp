// tinylisp.cpp
#define _CRT_SECURE_NO_WARNINGS
#include "tinylisp.hpp"
#include <iostream>
#include <cstdio>
#include <cmath>
#include <sstream>

// === Реализация NaN Boxing ===
TinyLisp::Value TinyLisp::box(Index tag, Index index) const {
    Value res;
    uint32_t val = (tag << 20) | (index & 0xFFFFF);
    std::memcpy(&res, &val, sizeof(val));
    return res;
}

TinyLisp::Index TinyLisp::get_type_bits(Value val) const {
    uint32_t bits;
    std::memcpy(&bits, &val, sizeof(bits));
    return bits >> 20;
}

TinyLisp::Index TinyLisp::get_tag(Value val) const {
    return get_type_bits(val);
}

TinyLisp::Index TinyLisp::get_index(Value val) const {
    uint32_t bits;
    std::memcpy(&bits, &val, sizeof(bits));
    return bits & 0xFFFFF;
}

TinyLisp::Value TinyLisp::num(float n) { return n; }

bool TinyLisp::equ(Value x, Value y) {
    uint32_t bx, by;
    std::memcpy(&bx, &x, sizeof(bx));
    std::memcpy(&by, &y, sizeof(by));
    return bx == by;
}

// === Конструктор и Инициализация ===
TinyLisp::TinyLisp(size_t memory_size)
    : memory(memory_size), sp(memory_size), hp(0), string_heap(1024 * 1024)
{
    // Инициализация примитивов (порядок важен, соответствует массиву prim[] в C)
    primitives = {
        {"eval", &TinyLisp::p_eval}, {"car", &TinyLisp::p_car},
        {"-", &TinyLisp::p_sub},     {"<", &TinyLisp::p_lt},
        {"or", &TinyLisp::p_or},     {"cond", &TinyLisp::p_cond},
        {"lambda", &TinyLisp::p_lambda}, {"quote", &TinyLisp::p_quote},
        {"cdr", &TinyLisp::p_cdr},   {"*", &TinyLisp::p_mul},
        {"int", &TinyLisp::p_int},   {"and", &TinyLisp::p_and},
        {"if", &TinyLisp::p_if},     {"define", &TinyLisp::p_define},
        {"cons", &TinyLisp::p_cons}, {"+", &TinyLisp::p_add},
        {"/", &TinyLisp::p_div},     {"eq?", &TinyLisp::p_eq},
        {"not", &TinyLisp::p_not},   {"let*", &TinyLisp::p_leta},
        {"pair?", &TinyLisp::p_pair}
    };

    nil_value = box(NIL_TAG, 0);
    error_value = atom("ERR");
    true_value = atom("#t");
    env = pair(true_value, true_value, nil_value);

    // Регистрация примитивов в окружении
    for (size_t i = 0; i < primitives.size(); ++i) {
        env = pair(atom(primitives[i].name), box(PRIM_TAG, i), env);
    }
}

// === Управление Памятью и Списками ===
TinyLisp::Value TinyLisp::atom(const char* s) {
    size_t i = 0;
    while (i < hp) {
        if (std::strcmp(&string_heap[i], s) == 0) return box(ATOM_TAG, i);
        i += std::strlen(&string_heap[i]) + 1;
    }
    size_t len = std::strlen(s);
    if (hp + len + 1 > string_heap.size()) throw std::runtime_error("Heap overflow");
    std::strcpy(&string_heap[hp], s);
    Value res = box(ATOM_TAG, hp);
    hp += len + 1;
    return res;
}

TinyLisp::Value TinyLisp::cons(Value x, Value y) {
    if (sp < 2) throw std::runtime_error("Stack overflow");
    memory[--sp] = x;
    memory[--sp] = y;
    return box(CONS_TAG, sp);
}

TinyLisp::Value TinyLisp::car(Value p) {
    return (get_type_bits(p) & ~(CONS_TAG ^ CLOS_TAG)) == CONS_TAG ? memory[get_index(p) + 1] : error_value;
}

TinyLisp::Value TinyLisp::cdr(Value p) {
    return (get_type_bits(p) & ~(CONS_TAG ^ CLOS_TAG)) == CONS_TAG ? memory[get_index(p)] : error_value;
}

TinyLisp::Value TinyLisp::pair(Value v, Value x, Value e) {
    return cons(cons(v, x), e);
}

TinyLisp::Value TinyLisp::closure(Value v, Value x, Value e) {
    return box(CLOS_TAG, get_index(pair(v, x, equ(e, env) ? nil_value : e)));
}

TinyLisp::Value TinyLisp::assoc(Value v, Value e) {
    while (get_type_bits(e) == CONS_TAG && !equ(v, car(car(e)))) {
        e = cdr(e);
    }
    return get_type_bits(e) == CONS_TAG ? cdr(car(e)) : error_value;
}

bool TinyLisp::not_val(Value x) {
    return get_type_bits(x) == NIL_TAG;
}

// === Ядро Вычислений ===
TinyLisp::Value TinyLisp::evlis(Value t, Value e) {
    if (get_type_bits(t) == CONS_TAG) {
        return cons(eval_expr(car(t), e), evlis(cdr(t), e));
    }
    return get_type_bits(t) == ATOM_TAG ? assoc(t, e) : nil_value;
}

TinyLisp::Value TinyLisp::bind(Value v, Value t, Value e) {
    if (not_val(v)) return e;
    if (get_type_bits(v) == CONS_TAG) {
        return bind(cdr(v), cdr(t), pair(car(v), car(t), e));
    }
    return pair(v, t, e);
}

TinyLisp::Value TinyLisp::reduce(Value f, Value t, Value e) {
    return eval_expr(cdr(car(f)), bind(car(car(f)), evlis(t, e), not_val(cdr(f)) ? env : cdr(f)));
}

TinyLisp::Value TinyLisp::apply(Value f, Value t, Value e) {
    if (get_type_bits(f) == PRIM_TAG) {
        return (this->*primitives[get_index(f)].func)(t, e);
    }
    if (get_type_bits(f) == CLOS_TAG) {
        return reduce(f, t, e);
    }
    return error_value;
}

TinyLisp::Value TinyLisp::eval_expr(Value x, Value e) {
    if (get_type_bits(x) == ATOM_TAG) return assoc(x, e);
    if (get_type_bits(x) == CONS_TAG) return apply(eval_expr(car(x), e), cdr(x), e);
    return x;
}

// === Примитивные Функции ===
TinyLisp::Value TinyLisp::p_eval(Value t, Value e) { return eval_expr(car(evlis(t, e)), e); }
TinyLisp::Value TinyLisp::p_quote(Value t, Value e) { return car(t); }
TinyLisp::Value TinyLisp::p_cons(Value t, Value e) { t = evlis(t, e); return cons(car(t), car(cdr(t))); }
TinyLisp::Value TinyLisp::p_car(Value t, Value e) { return car(car(evlis(t, e))); }
TinyLisp::Value TinyLisp::p_cdr(Value t, Value e) { return cdr(car(evlis(t, e))); }
TinyLisp::Value TinyLisp::p_add(Value t, Value e) { Value n = car(t = evlis(t, e)); while (!not_val(t = cdr(t))) n += car(t); return num(n); }
TinyLisp::Value TinyLisp::p_sub(Value t, Value e) { Value n = car(t = evlis(t, e)); while (!not_val(t = cdr(t))) n -= car(t); return num(n); }
TinyLisp::Value TinyLisp::p_mul(Value t, Value e) { Value n = car(t = evlis(t, e)); while (!not_val(t = cdr(t))) n *= car(t); return num(n); }
TinyLisp::Value TinyLisp::p_div(Value t, Value e) { Value n = car(t = evlis(t, e)); while (!not_val(t = cdr(t))) n /= car(t); return num(n); }
TinyLisp::Value TinyLisp::p_int(Value t, Value e) { Value n = car(evlis(t, e)); return (n < 1e7 && n > -1e7) ? (long long)n : n; }
TinyLisp::Value TinyLisp::p_lt(Value t, Value e) { t = evlis(t, e); return (car(t) - car(cdr(t)) < 0) ? true_value : nil_value; }
TinyLisp::Value TinyLisp::p_eq(Value t, Value e) { t = evlis(t, e); return equ(car(t), car(cdr(t))) ? true_value : nil_value; }
TinyLisp::Value TinyLisp::p_pair(Value t, Value e) { Value x = car(evlis(t, e)); return get_type_bits(x) == CONS_TAG ? true_value : nil_value; }
TinyLisp::Value TinyLisp::p_or(Value t, Value e) { Value x = nil_value; while (!not_val(t) && not_val(x = eval_expr(car(t), e))) t = cdr(t); return x; }
TinyLisp::Value TinyLisp::p_and(Value t, Value e) { Value x = true_value; while (!not_val(t) && !not_val(x = eval_expr(car(t), e))) t = cdr(t); return x; }
TinyLisp::Value TinyLisp::p_not(Value t, Value e) { return not_val(car(evlis(t, e))) ? true_value : nil_value; }
TinyLisp::Value TinyLisp::p_cond(Value t, Value e) { while (not_val(eval_expr(car(car(t)), e))) t = cdr(t); return eval_expr(car(cdr(car(t))), e); }
TinyLisp::Value TinyLisp::p_if(Value t, Value e) { return eval_expr(car(cdr(not_val(eval_expr(car(t), e)) ? cdr(t) : t)), e); }
TinyLisp::Value TinyLisp::p_leta(Value t, Value e) { for (; !not_val(t) && !not_val(cdr(t)); t = cdr(t)) e = pair(car(car(t)), eval_expr(car(cdr(car(t))), e), e); return eval_expr(car(t), e); }
TinyLisp::Value TinyLisp::p_lambda(Value t, Value e) { return closure(car(t), car(cdr(t)), e); }
TinyLisp::Value TinyLisp::p_define(Value t, Value e) { env = pair(car(t), eval_expr(car(cdr(t)), e), env); return car(t); }

// === Парсер ===
void TinyLisp::setup_input(const std::string& code) {
    input_buffer = code;
    input_pos = 0;
    lookahead = ' ';
    get_char(); // Первый символ
}

void TinyLisp::get_char() {
    if (input_pos < input_buffer.length()) {
        lookahead = input_buffer[input_pos++];
    }
    else {
        lookahead = 0; // EOF
    }
}

void TinyLisp::skip_whitespace() {
    while (lookahead && isspace(lookahead)) get_char();
}

char TinyLisp::scan() {
    int i = 0;
    skip_whitespace();
    if (lookahead == '(' || lookahead == ')' || lookahead == '\'') {
        token_buffer[i++] = lookahead;
        get_char();
    }
    else {
        do {
            if (i < 127) token_buffer[i++] = lookahead;
            get_char();
        } while (lookahead && lookahead != '(' && lookahead != ')' && !isspace(lookahead));
    }
    token_buffer[i] = 0;
    return token_buffer[0];
}

TinyLisp::Value TinyLisp::read_one() {
    scan();
    const char* token = token_buffer;
    if (*token == '(') return parse_list();
    if (*token == '\'') return parse_quote();
    return parse_atomic();
}

TinyLisp::Value TinyLisp::parse_list() {
    scan(); // Проверяем следующий токен (не сдвигая scan буфер, но scan() внутри делает работу)
    // Хитрость: scan() в C-коде tinylisp читает в глобальный буфер. 
    // Нам нужно знать, закрывающая ли это скобка.
    if (token_buffer[0] == ')') return nil_value;
    if (std::strcmp(token_buffer, ".") == 0) {
        Value x = read_one();
        scan(); // Пропустить ')'
        return x;
    }

    // Откат назад не нужен, потому что read_one снова вызовет scan/parse_atomic если это не список
    // Но scan() уже прочитал токен. 
    // В tinylisp рекурсия list() вызывает parse().

    // Переопределим логику list() из C более чисто:
    // Мы уже внутри '(', предыдущий скан вернул '('. 
    // Текущий scan() выше проверил на ')' или '.'.
    // Если это атомарное значение или начало списка:

    // Восстанавливаем токен для parse_atomic если это не спецсимвол
    // В оригинале list() вызывает scan() == ')' ? ... : parse().
    // parse() смотрит на *buf.

    // Реализация:
    Value x = (*token_buffer == '(') ? parse_list() :
        (*token_buffer == '\'') ? parse_quote() : parse_atomic();

    return cons(x, parse_list());
}

TinyLisp::Value TinyLisp::parse_quote() {
    return cons(atom("quote"), cons(read_one(), nil_value));
}

TinyLisp::Value TinyLisp::parse_atomic() {
    float n;
    // Попытка парсинга числа
    try {
        std::string s(token_buffer);
        size_t idx;
        n = std::stof(s, &idx);
        if (idx == s.length()) return num(n);
    }
    catch (...) {}
    return atom(token_buffer);
}

// === Принтер ===
std::string TinyLisp::print(Value x) {
    if (get_type_bits(x) == NIL_TAG) return "()";
    if (get_type_bits(x) == ATOM_TAG) return &string_heap[get_index(x)];
    if (get_type_bits(x) == PRIM_TAG) return std::string("<") + primitives[get_index(x)].name + ">";
    if (get_type_bits(x) == CONS_TAG) return print_list(x);
    if (get_type_bits(x) == CLOS_TAG) return "{closure}";
    // Число
    std::ostringstream ss;
    ss << x;
    return ss.str();
}

std::string TinyLisp::print_list(Value t) {
    std::string s = "(";
    while (true) {
        s += print(car(t));
        if (not_val(t = cdr(t))) break;
        if (get_type_bits(t) != CONS_TAG) {
            s += " . " + print(t);
            break;
        }
        s += " ";
    }
    s += ")";
    return s;
}

void TinyLisp::gc() {
    sp = get_index(env);
}

// === Публичный интерфейс ===
std::string TinyLisp::eval(const std::string& code) {
    setup_input(code);
    Value val = read_one();
    Value res = eval_expr(val, env);
    gc();
    return print(res);
}

void TinyLisp::repl() {
    std::string line;
    std::cout << "TinyLisp C++ REPL. Press Ctrl+C to exit.\n";
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        try {
            std::cout << eval(line) << std::endl;
        }
        catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << std::endl;
        }
    }
}