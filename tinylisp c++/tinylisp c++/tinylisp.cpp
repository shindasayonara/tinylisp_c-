// tinylisp.cpp
#include "tinylisp.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <iomanip>
#include <cctype>

static std::istringstream* input_stream = nullptr;

TinyLisp::TinyLisp(size_t memory_size)
    : memory(memory_size * 2, 0.0f),
    heap_ptr(0),
    stack_ptr((Index)memory_size),
    lookahead(' '),
    error_occurred(false) {

    nil_value = box(NIL_TAG, 0);
    string_heap.push_back('\0');
    true_value = allocate_atom("#t");
    error_value = allocate_atom("error");
    environment = nil_value;

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

    for (size_t i = 0; i < primitives.size(); ++i) {
        Value prim_symbol = allocate_atom(primitives[i].name);
        Value prim_value = allocate_primitive((Index)i);
        environment = env_define(prim_symbol, prim_value, environment);
    }

    environment = env_define(true_value, true_value, environment);
}

// ========== —»—“≈ÃÕ€≈ ‘”Õ ÷»» ==========

TinyLisp::Value TinyLisp::box(Index tag, Index index) const {
    Value result;
    uint32_t packed = (tag << 20) | (index & 0xFFFFF);
    std::memcpy(&result, &packed, sizeof(result));
    return result;
}

TinyLisp::Index TinyLisp::get_tag(Value value) const {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits >> 20;
}

TinyLisp::Index TinyLisp::get_index(Value value) const {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits & 0xFFFFF;
}

bool TinyLisp::is_type(Value value, Index tag) const {
    return get_tag(value) == tag;
}

// ========== œ¿Ãﬂ“‹ ==========

TinyLisp::Value TinyLisp::allocate_atom(const std::string& name) {
    for (size_t i = 0; i < string_heap.size(); ) {
        if (string_heap[i] == '\0') { i++; continue; }
        if (name == &string_heap[i]) {
            return box(ATOM_TAG, (Index)i);
        }
        i += std::strlen(&string_heap[i]) + 1;
    }

    Index index = (Index)string_heap.size();
    for (char c : name) string_heap.push_back(c);
    string_heap.push_back('\0');
    return box(ATOM_TAG, index);
}

TinyLisp::Value TinyLisp::allocate_cons(Value car_val, Value cdr_val) {
    if (stack_ptr < 2) throw std::runtime_error("Out of memory");
    stack_ptr -= 2;
    memory[stack_ptr] = car_val;
    memory[stack_ptr + 1] = cdr_val;
    return box(CONS_TAG, stack_ptr);
}

TinyLisp::Value TinyLisp::allocate_closure(Value vars, Value body, Value env) {
    Value closure_data = allocate_cons(vars, allocate_cons(body, env));
    return box(CLOS_TAG, get_index(closure_data));
}

TinyLisp::Value TinyLisp::allocate_primitive(Index index) {
    return box(PRIM_TAG, index);
}

void TinyLisp::check_memory() {}
void TinyLisp::garbage_collect() {}
void TinyLisp::expand_memory() {}

// ========== œ¿–€ ==========

TinyLisp::Value TinyLisp::car(Value pair) const {
    if (is_nil(pair)) return nil_value;
    if (!is_cons(pair)) throw std::runtime_error("car: expected pair");
    // Explicit cast to size_t to avoid lnt-arithmetic-overflow
    return memory[(size_t)get_index(pair)];
}

TinyLisp::Value TinyLisp::cdr(Value pair) const {
    if (is_nil(pair)) return nil_value;
    if (!is_cons(pair)) throw std::runtime_error("cdr: expected pair");
    return memory[(size_t)get_index(pair) + 1];
}

void TinyLisp::set_car(Value pair, Value value) {
    if (is_cons(pair)) memory[(size_t)get_index(pair)] = value;
}

void TinyLisp::set_cdr(Value pair, Value value) {
    if (is_cons(pair)) memory[(size_t)get_index(pair) + 1] = value;
}

// ========== Œ –”∆≈Õ»≈ ==========

TinyLisp::Value TinyLisp::make_env(Value vars, Value values, Value parent_env) {
    return env_define(vars, values, parent_env);
}

TinyLisp::Value TinyLisp::env_lookup(Value symbol, Value env) const {
    Value curr = env;
    while (!is_nil(curr)) {
        Value pair = car(curr);
        if (get_index(car(pair)) == get_index(symbol)) {
            return cdr(pair);
        }
        curr = cdr(curr);
    }
    return nil_value;
}

TinyLisp::Value TinyLisp::env_define(Value symbol, Value value, Value env) {
    Value pair = allocate_cons(symbol, value);
    return allocate_cons(pair, env);
}

// ========== ¬€◊»—À≈Õ»≈ ==========

TinyLisp::Value TinyLisp::eval_list(Value list, Value env) {
    if (is_nil(list)) return nil_value;
    Value head = evaluate(car(list), env);
    Value tail = eval_list(cdr(list), env);
    return allocate_cons(head, tail);
}

TinyLisp::Value TinyLisp::evaluate(Value expression, Value env) {
    if (is_atom(expression)) {
        Value res = env_lookup(expression, env);
        if (is_nil(res)) return expression;
        return res;
    }
    else if (is_number(expression)) {
        return expression;
    }
    else if (is_cons(expression)) {
        Value func = evaluate(car(expression), env);
        Value args = cdr(expression);
        return apply(func, args, env);
    }
    return expression;
}

TinyLisp::Value TinyLisp::apply(Value function, Value args, Value env) {
    if (is_primitive(function)) {
        return (this->*primitives[get_index(function)].function)(args, env);
    }
    return error_value;
}

// ========== œ–»Ã»“»¬€ ==========

TinyLisp::Value TinyLisp::prim_add(Value args, Value env) {
    float sum = 0;
    Value curr = args;
    while (!is_nil(curr)) {
        Value v = evaluate(car(curr), env);
        // œÓÒÚÓ Í‡ÒÚËÏ Í float, Ú‡Í Í‡Í Value = float
        sum += (float)v;
        curr = cdr(curr);
    }
    return sum;
}

TinyLisp::Value TinyLisp::prim_mul(Value args, Value env) {
    float prod = 1;
    Value curr = args;
    while (!is_nil(curr)) {
        Value v = evaluate(car(curr), env);
        prod *= (float)v;
        curr = cdr(curr);
    }
    return prod;
}

TinyLisp::Value TinyLisp::prim_cons(Value args, Value env) {
    Value v1 = evaluate(car(args), env);
    Value v2 = evaluate(car(cdr(args)), env);
    return allocate_cons(v1, v2);
}

// «‡„ÎÛ¯ÍË
TinyLisp::Value TinyLisp::prim_eval(Value args, Value env) { return nil_value; }
TinyLisp::Value TinyLisp::prim_quote(Value args, Value env) { return car(args); }
TinyLisp::Value TinyLisp::prim_car(Value args, Value env) { return car(evaluate(car(args), env)); }
TinyLisp::Value TinyLisp::prim_cdr(Value args, Value env) { return cdr(evaluate(car(args), env)); }
TinyLisp::Value TinyLisp::prim_sub(Value args, Value env) { return nil_value; }
TinyLisp::Value TinyLisp::prim_div(Value args, Value env) { return nil_value; }
TinyLisp::Value TinyLisp::prim_lt(Value args, Value env) { return nil_value; }
TinyLisp::Value TinyLisp::prim_eq(Value args, Value env) { return nil_value; }
TinyLisp::Value TinyLisp::prim_pair(Value args, Value env) { return nil_value; }
TinyLisp::Value TinyLisp::prim_or(Value args, Value env) { return nil_value; }
TinyLisp::Value TinyLisp::prim_and(Value args, Value env) { return nil_value; }
TinyLisp::Value TinyLisp::prim_not(Value args, Value env) { return nil_value; }
TinyLisp::Value TinyLisp::prim_cond(Value args, Value env) { return nil_value; }
TinyLisp::Value TinyLisp::prim_if(Value args, Value env) { return nil_value; }
TinyLisp::Value TinyLisp::prim_define(Value args, Value env) { return nil_value; }
TinyLisp::Value TinyLisp::prim_lambda(Value args, Value env) { return nil_value; }
TinyLisp::Value TinyLisp::prim_let(Value args, Value env) { return nil_value; }

// ========== œ¿–—≈– ==========

void TinyLisp::initialize_parser() {
    lookahead = ' ';
}

char TinyLisp::get_char() {
    if (input_stream && input_stream->get(lookahead)) {
        return lookahead;
    }
    return lookahead = EOF;
}

void TinyLisp::skip_whitespace() {
    while (isspace(lookahead)) get_char();
}

TinyLisp::Value TinyLisp::parse_expression() {
    skip_whitespace();

    if (lookahead == '(') {
        get_char();
        return parse_list();
    }
    else if (lookahead == '\'') {
        get_char();
        return allocate_cons(allocate_atom("quote"),
            allocate_cons(parse_expression(), nil_value));
    }
    else {
        return parse_atom();
    }
}

TinyLisp::Value TinyLisp::parse_list() {
    skip_whitespace();
    if (lookahead == ')') {
        get_char();
        return nil_value;
    }

    Value head = parse_expression();
    Value tail = parse_list();
    return allocate_cons(head, tail);
}

TinyLisp::Value TinyLisp::parse_atom() {
    std::string token;
    while (lookahead != EOF && !isspace(lookahead) && lookahead != ')' && lookahead != '(') {
        token += lookahead;
        get_char();
    }
    try {
        size_t pos;
        float val = std::stof(token, &pos);
        if (pos == token.length()) return val;
    }
    catch (...) {}
    return allocate_atom(token);
}

// ========== ¬€¬Œƒ ==========

std::string TinyLisp::value_to_string(Value value) const {
    if (is_nil(value)) return "nil";
    if (is_number(value)) {
        std::ostringstream ss;
        ss << (float)value;
        return ss.str();
    }
    if (is_atom(value)) return &string_heap[get_index(value)];
    if (is_primitive(value)) return "<primitive>";
    if (is_closure(value)) return "<closure>";
    if (is_cons(value)) return "(" + list_to_string(value) + ")";
    return "?";
}

std::string TinyLisp::list_to_string(Value list) const {
    std::string s;
    Value curr = list;
    bool first = true;
    while (!is_nil(curr)) {
        if (is_cons(curr)) {
            if (!first) s += " ";
            s += value_to_string(car(curr));
            curr = cdr(curr);
            first = false;
        }
        else {
            s += " . " + value_to_string(curr);
            break;
        }
    }
    return s;
}

void TinyLisp::error(const std::string& message) {
    std::cerr << "Runtime Error: " << message << std::endl;
    error_occurred = true;
}

void TinyLisp::reset() {
    error_occurred = false;
}

std::string TinyLisp::eval(const std::string& code) {
    error_occurred = false;
    try {
        std::istringstream ss(code);
        input_stream = &ss;
        get_char();

        Value expr = parse_expression();
        input_stream = nullptr;

        Value res = evaluate(expr, environment);
        return value_to_string(res);
    }
    catch (const std::exception& e) {
        return std::string("Error: ") + e.what();
    }
}

void TinyLisp::repl() {
    std::cout << "TinyLisp REPL (Ctrl+C to exit)" << std::endl;
    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        std::cout << eval(line) << std::endl;
    }
}