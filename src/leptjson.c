#include "leptjson.h"
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif

#define EXPECT(c, ch)   do { assert(*c->json == (ch)); c->json++; } while(0)//判断json字符串首字母是不是ch, 若不是，断言出错
#define ISDIGIT(ch)         ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch)     ((ch) >= '1' && (ch) <= '9')
#define PUTC(c, ch)     do { *(char*)lept_context_push(c, sizeof(char)) = (ch); } while(0);
#define STRING_ERROR(ret) do { c->top = head; return ret; } while(0)
#define PUTS(c, s, len)     memcpy(lept_context_push(c, len), s, len)


typedef struct {
    const char *json;
    char* stack;//栈
    size_t size, top;
}lept_context;



static void* lept_context_push(lept_context* c, size_t size) {
    void *ret;
    assert(size > 0);
    if (c->top + size >= c->size) {
        if (c->size == 0) {
            c->size = LEPT_PARSE_STACK_INIT_SIZE;
        }
        while (c->top + size >= c->size) {
            c->size += c->size >> 1;
        }
        c->stack = (char *)realloc(c->stack, c->size);
    }
    ret = c->stack + c->top;
    c->top += size;
    return ret;
}

static void* lept_context_pop(lept_context* c, size_t size) {
    assert(c->top >= size);
    c->top -= size;
    return c->stack + c->top;
}


static int lept_parse_whitespace(lept_context* c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    c->json = p;
}

static int lept_parse_literal(lept_context* c, lept_value* v, const char* literal, lept_type type) {
    size_t i;
    EXPECT(c, literal[0]);
    for (i = 0; literal[i + 1]; ++i) {
        if (c->json[i] != literal[i + 1]) {
            return LEPT_PARSE_INVALID_VALUE;
        }
    }
    c->json += i;
    v->type = type;
    return LEPT_PARSE_OK;
}

static int lept_parse_number(lept_context* c, lept_value* v) {
    const char *p = c->json;
    if (*p == '-') {
        p++;
    }
    
    if (*p == '0') {
        p++;
    } else {
        if (!ISDIGIT1TO9(*p)) {
            return LEPT_PARSE_INVALID_VALUE;
        }
        for (p++; ISDIGIT(*p); p++);
    }

    if (*p == '.') {
        p++;
        if (!ISDIGIT(*p)) {
            return LEPT_PARSE_INVALID_VALUE;
        }
        for (p++; ISDIGIT(*p); p++);
    }

    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') {
            p++;
        }
        if (!ISDIGIT(*p)) {
            return LEPT_PARSE_INVALID_VALUE;
        }
        for (p++; ISDIGIT(*p); p++);
    }

    errno = 0;
    v->u.n = strtod(c->json, NULL);
    if (errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL)) {
        return LEPT_PARSE_NUMBER_TOO_BIG;
    }
    c->json = p;
    v->type = LEPT_NUMBER;
    return LEPT_PARSE_OK;
}

static const char* lept_parse_hex4(const char* p, unsigned* u) {
    int i;
    *u = 0;
    for (i = 0; i < 4; ++i) {
        char ch = *p++;
        *u <<= 4;
        if      (ch >= '0' && ch <= '9')  *u |= ch - '0';
        else if (ch >= 'A' && ch <= 'F')  *u |= ch - ('A' - 10);
        else if (ch >= 'a' && ch <= 'f')  *u |= ch - ('a' - 10);
        else return NULL;//说明出错
    }
    return p;
}

static void lept_encode_utf8(lept_context* c, unsigned u) {
    if (u <= 0x7F) {
        PUTC(c, u & 0xFF);
    } else if (u <= 0x7FF) {
        PUTC(c, 0xC0 | ((u >> 6) & 0xFF));
        PUTC(c, 0x80 | ( u       & 0x3F));
    }
    else if (u <= 0xFFFF) {
        PUTC(c, 0xE0 | ((u >> 12) & 0xFF));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
    else {
        assert(u <= 0x10FFFF);
        PUTC(c, 0xF0 | ((u >> 18) & 0xFF));
        PUTC(c, 0x80 | ((u >> 12) & 0x3F));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
}

static int lept_parse_string_raw(lept_context* c, char** str, size_t* len) {
    size_t head = c->top;
    const char *p;
    unsigned u, u2;
    EXPECT(c, '\"');
    p = c->json;
    while (1) {
        char ch = *p++;
        switch (ch) {
            case '\"':
                *len = c->top - head;
                *str = lept_context_pop(c, *len);
                c->json = p;
                return LEPT_PARSE_OK;
            case '\\':
                switch(*p++) {
                    case '\"': PUTC(c, '\"'); break;
                    case '\\': PUTC(c, '\\'); break;
                    case '/': PUTC(c, '/'); break;
                    case 'b': PUTC(c, '\b'); break;
                    case 'f': PUTC(c, '\f'); break;
                    case 'n': PUTC(c, '\n'); break;
                    case 'r': PUTC(c, '\r'); break;
                    case 't': PUTC(c, '\t'); break;
                    case 'u':
                        if (!(p = lept_parse_hex4(p, &u))) {
                            STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                        }
                        if (u >= 0xD800 && u <= 0xDBFF) { /* surrogate pair */
                            if (*p++ != '\\')
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            if (*p++ != 'u')
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            if (!(p = lept_parse_hex4(p, &u2)))
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                            if (u2 < 0xDC00 || u2 > 0xDFFF)
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
                        }
                        lept_encode_utf8(c, u);
                        break;
                    default:
                        STRING_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE);
                }
                break;
            case '\0':
                STRING_ERROR(LEPT_PARSE_MISS_QUOTATION_MARK);
            default:
                if ((unsigned char)ch < 0x20) { 
                    STRING_ERROR(LEPT_PARSE_INVALID_STRING_CHAR);
                }
                PUTC(c, ch);
        }
    }
}

static int lept_parse_string(lept_context* c, lept_value* v) {
    int ret;
    char *s;
    size_t len;
    if ((ret = lept_parse_string_raw(c, &s, &len)) == LEPT_PARSE_OK) {
        lept_set_string(v, s, len);
    }
    return ret;
}


static int lept_parse_value(lept_context* c, lept_value* v);

static int lept_parse_array(lept_context* c, lept_value* v) {
    size_t size = 0, i;
    int ret;
    EXPECT(c, '[');
    lept_parse_whitespace(c);
    if (*c->json == ']') {
        c->json++;
        lept_set_array(v, 0);//capacity = 0
        return LEPT_PARSE_OK;
    }
    while (1) {
        lept_value e;
        lept_init(&e);
        if ((ret = lept_parse_value(c, &e)) != LEPT_PARSE_OK) {
            break;//表示该元素解析出错
        }
        memcpy(lept_context_push(c, sizeof(lept_value)), &e, sizeof(lept_value));//lept_value压栈（c）
        size++;
        
        lept_parse_whitespace(c);
        if (*c->json == ',') {
            c->json++;
            lept_parse_whitespace(c);
        } else if (*c->json == ']') {
            c->json++;
            lept_set_array(v, size);//capacity = size
            memcpy(v->u.a.e, lept_context_pop(c, size * sizeof(lept_value)), size * sizeof(lept_value));
            v->u.a.size = size;
            return LEPT_PARSE_OK;
        } else {
            ret = LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
            break;
        }
    }
    for (i = 0; i < size; ++i) {
        lept_free((lept_value *)lept_context_pop(c, sizeof(lept_value)));
    }
    return ret;
}

static int lept_parse_object(lept_context* c, lept_value* v) {
    size_t i, size;
    lept_member m;
    int ret;
    EXPECT(c, '{');
    lept_parse_whitespace(c);
    if (*c->json == '}') {
        c->json++;
        lept_set_object(v, 0);//capacity = 0
        return LEPT_PARSE_OK;
    }

    m.k = NULL;
    size = 0;//对象中元素个数
    while (1) {
        char *str;
        lept_init(&m.v);
        if (*c->json != '\"') {//没有key值
            ret = LEPT_PARSE_MISS_KEY;
            break;
        }
        if ((ret = lept_parse_string_raw(c, &str, &m.klen)) != LEPT_PARSE_OK) {
            break;
        }
        memcpy(m.k = (char *)malloc(m.klen + 1), str, m.klen);
        m.k[m.klen] = '\0';
        lept_parse_whitespace(c);
        if (*c->json != ':') {
            ret = LEPT_PARSE_MISS_COLON;
            break;
        }
        c->json++;
        lept_parse_whitespace(c);
        if ((ret = lept_parse_value(c, &m.v)) != LEPT_PARSE_OK) {
            break;
        }

        memcpy(lept_context_push(c, sizeof(lept_member)), &m , sizeof(lept_member));//将该member放入context的栈中
        
        size++;//对象中的元素个数加1
        m.k = NULL;
        lept_parse_whitespace(c);
        if (*c->json == ',') {//解析下一个成员
            c->json++;
            lept_parse_whitespace(c);
        } else if (*c->json == '}') {//解析结束
            c->json++;
            lept_set_object(v, size);//capacity = size
            memcpy(v->u.o.m, lept_context_pop(c, sizeof(lept_member) * size), sizeof(lept_member) * size);
            v->u.o.size = size;
            return LEPT_PARSE_OK;
        } else {
            ret = LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET;
            break;
        }
    }

    free(m.k);
    for (i = 0; i < size; ++i) {
        lept_member* m = (lept_member *)lept_context_pop(c, sizeof(lept_member));
        free(m->k);
        lept_free(&m->v);
    }
    v->type = LEPT_NULL;
    return ret;
}

static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 'n': return lept_parse_literal(c, v, "null", LEPT_NULL);
        case 'f': return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case 't': return lept_parse_literal(c, v, "true", LEPT_TRUE);
        default: return lept_parse_number(c, v);
        case '\"': return lept_parse_string(c, v);
        case '[': return lept_parse_array(c, v);
        case '{': return lept_parse_object(c, v);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
    }
}

int lept_parse(lept_value* v, const char* json) {
    assert(v != NULL);
    lept_context c;
    int ret;
    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
    lept_init(v);
    lept_parse_whitespace(&c);
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if (*(c.json) != '\0') {
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    assert(c.top == 0);
    free(c.stack);
    return ret;
}

//bool操作函数
lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}

int lept_get_boolean(const lept_value* v) {
    assert(v != NULL && (v->type == LEPT_TRUE || v->type == LEPT_FALSE));
    return v->type == LEPT_TRUE;
}

void lept_set_boolean(lept_value* v, int b) {
    assert(v != NULL);
    lept_free(v);
    v->type = b ? LEPT_TRUE : LEPT_FALSE;
}

//number操作函数
double lept_get_number(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->u.n;
}

void lept_set_number(lept_value* v, double n) {
    assert(v != NULL);
    lept_free(v);
    v->u.n = n;
    v->type = LEPT_NUMBER;
}

//string操作函数
const char* lept_get_string(const lept_value* v) {
    assert(v != NULL);
    return v->u.s.s;
}

size_t lept_get_string_length(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.len;
}

void lept_set_string(lept_value* v, const char* s, size_t len) {
    assert(v != NULL && (s != NULL || len == 0));
    lept_free(v);
    v->u.s.s = (char *)malloc(len + 1);
    memcpy(v->u.s.s, s, len);
    v->u.s.s[len] = '\0';//字符串末尾
    v->u.s.len = len;
    v->type = LEPT_STRING;
}

//array操作函数
void lept_set_array(lept_value* v, size_t capacity) {
    assert(v != NULL);
    lept_free(v);
    v->type = LEPT_ARRAY;
    v->u.a.size = 0;
    v->u.a.capacity = capacity;
    v->u.a.e = capacity > 0 ? (lept_value *)malloc(capacity * sizeof(lept_value)) : NULL;
}

size_t lept_get_array_size(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    return v->u.a.size;
}

size_t lept_get_array_capacity(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    return v->u.a.capacity;
}

void lept_reserve_array(lept_value* v, size_t capacity) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    if (v->u.a.capacity < capacity) {
        v->u.a.capacity = capacity;
        v->u.a.e = (lept_value*)realloc(v->u.a.e, capacity * sizeof(lept_value));
    }
}

void lept_shrink_array(lept_value* v) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    if (v->u.a.capacity > v->u.a.size) {
        v->u.a.capacity = v->u.a.size;
        v->u.a.e = (lept_value*)realloc(v->u.a.e, v->u.a.capacity * sizeof(lept_value));
    }
}

void lept_clear_array(lept_value* v) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    lept_erase_array_element(v, 0, v->u.a.size);
}

lept_value* lept_get_array_element(lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    assert(index < v->u.a.size);
    return &v->u.a.e[index];
}

lept_value* lept_pushback_array_element(lept_value* v) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    if (v->u.a.size == v->u.a.capacity)
        lept_reserve_array(v, v->u.a.capacity == 0 ? 1 : v->u.a.capacity * 2);
    lept_init(&v->u.a.e[v->u.a.size]);
    return &v->u.a.e[v->u.a.size++];
}

void lept_popback_array_element(lept_value* v) {
    assert(v != NULL && v->type == LEPT_ARRAY && v->u.a.size > 0);
    lept_free(&v->u.a.e[--v->u.a.size]);
}

lept_value* lept_insert_array_element(lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_ARRAY && index <= v->u.a.size);
    if (v->u.a.size == v->u.a.capacity) {
        lept_reserve_array(v, v->u.a.capacity == 0 ? 1 : (v->u.a.size << 1));
    }
    memcpy(&v->u.a.e[index + 1], &v->u.a.e[index], (v->u.a.size - index) * sizeof(lept_value));
    lept_init(&v->u.a.e[index]);
    v->u.a.size++;
    return &v->u.a.e[index];
}

void lept_erase_array_element(lept_value* v, size_t index, size_t count) {
    assert(v != NULL && v->type == LEPT_ARRAY && index + count <= v->u.a.size);
    size_t i;
    for (i = index; i < index + count; ++i) {
        lept_free(&v->u.a.e[i]);
    }
    memcpy(v->u.a.e + index, v->u.a.e + index + count, (v->u.a.size - index - count) * sizeof(lept_value));
    for (i = v->u.a.size - count; i < v->u.a.size; ++i) {
        lept_init(&v->u.a.e[i]);
    }
    v->u.a.size -= count;
}

//object操作函数
void lept_set_object(lept_value* v, size_t capacity) {
    assert(v != NULL);
    lept_free(v);
    v->type = LEPT_OBJECT;
    v->u.o.size = 0;
    v->u.o.capacity = capacity;
    v->u.o.m = capacity > 0 ? (lept_member*)malloc(capacity * sizeof(lept_member)) : NULL;
}

size_t lept_get_object_size(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    return v->u.o.size;
}

size_t lept_get_object_capacity(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    return v->u.o.capacity;
}

void lept_reserve_object(lept_value* v, size_t capacity) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    if(v->u.o.capacity < capacity){
        v->u.o.capacity = capacity;
        v->u.o.m = (lept_member *) realloc(v->u.o.m, capacity * sizeof(lept_member));
    }
}

void lept_shrink_object(lept_value* v) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    if(v->u.o.capacity > v->u.o.size) {
        v->u.o.capacity = v->u.o.size;
        v->u.o.m = (lept_member *)realloc(v->u.o.m, v->u.o.capacity * sizeof(lept_member));
    }
}

void lept_clear_object(lept_value* v) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    size_t i;
    for (i = 0; i < v->u.o.size; ++i) {
        //回收k和v
        free(v->u.o.m[i].k);
        v->u.o.m[i].k = NULL;
        v->u.o.m[i].klen = 0;
        lept_free(&v->u.o.m[i].v);
    }
    v->u.o.size = 0;
}

const char* lept_get_object_key(const lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    assert(index < v->u.o.size);
    lept_member* member = &v->u.o.m[index];
    return member->k;
}

size_t lept_get_object_key_length(const lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    assert(index < v->u.o.size);
    lept_member* member = &v->u.o.m[index];
    return member->klen;
}

lept_value* lept_get_object_value(lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    assert(index < v->u.o.size);
    lept_member* member = &v->u.o.m[index];
    return &member->v;
}

size_t lept_find_object_index(const lept_value* v, const char* key, size_t klen) {
    size_t i;
    assert(v != NULL && v->type == LEPT_OBJECT && key != NULL);
    for (i = 0; i < v->u.o.size; i++)
        if (v->u.o.m[i].klen == klen && memcmp(v->u.o.m[i].k, key, klen) == 0)
            return i;
    return LEPT_KEY_NOT_EXIST;
}

lept_value* lept_find_object_value(lept_value* v, const char* key, size_t klen) {
    size_t index = lept_find_object_index(v, key, klen);
    return index != LEPT_KEY_NOT_EXIST ? &v->u.o.m[index].v : NULL;
}

lept_value* lept_set_object_value(lept_value* v, const char* key, size_t klen) {
    assert(v != NULL && v->type == LEPT_OBJECT && key != NULL);
    size_t i, index;
    index = lept_find_object_index(v, key, klen);
    if(index != LEPT_KEY_NOT_EXIST)
        return &v->u.o.m[index].v;
    //key not exist, then we make room and init
    if(v->u.o.size == v->u.o.capacity){
        lept_reserve_object(v, v->u.o.capacity == 0? 1: (v->u.o.capacity << 1));
    }
    i = v->u.o.size;
    v->u.o.m[i].k = (char *)malloc((klen + 1));
    memcpy(v->u.o.m[i].k, key, klen);
    v->u.o.m[i].k[klen] = '\0';
    v->u.o.m[i].klen = klen;
    lept_init(&v->u.o.m[i].v);
    v->u.o.size++;
    return &v->u.o.m[i].v;
}

void lept_remove_object_value(lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_OBJECT && index < v->u.o.size);
    free(v->u.o.m[index].k);
    lept_free(&v->u.o.m[index].v);
    //think like a list
    memcpy(v->u.o.m + index, v->u.o.m + index + 1, (v->u.o.size - index - 1) * sizeof(lept_member));
    v->u.o.m[--v->u.o.size].k = NULL;
    v->u.o.m[v->u.o.size].klen = 0;
    lept_init(&v->u.o.m[v->u.o.size].v);
}

int lept_is_equal(const lept_value* lhs, const lept_value* rhs) {
    size_t i, index;
    assert(lhs != NULL && rhs != NULL);
    if (lhs->type != rhs->type)
        return 0;
    switch (lhs->type) {
        case LEPT_STRING:
            return lhs->u.s.len == rhs->u.s.len && 
                memcmp(lhs->u.s.s, rhs->u.s.s, lhs->u.s.len) == 0;
        case LEPT_NUMBER:
            return lhs->u.n == rhs->u.n;
        case LEPT_ARRAY:
            if (lhs->u.a.size != rhs->u.a.size)
                return 0;
            for (i = 0; i < lhs->u.a.size; i++)
                if (!lept_is_equal(&lhs->u.a.e[i], &rhs->u.a.e[i]))
                    return 0;
            return 1;
        case LEPT_OBJECT:
            if (lhs->u.o.size != rhs->u.o.size) {
                return 0;
            }
            for (i = 0; i < lhs->u.o.size; ++i) {
                index = lept_find_object_index(rhs, lhs->u.o.m[i].k, lhs->u.o.m[i].klen);
                if (index == LEPT_KEY_NOT_EXIST) {
                    return 0;//key not exist
                }
                if (!lept_is_equal(&lhs->u.o.m[i].v, &rhs->u.o.m[index].v)) {
                    return 0;
                }
            }
            return 1;
        default:
            return 1;
    }
}

void lept_copy(lept_value* dst, lept_value* src) {
    size_t i;
    assert(src != NULL && dst != NULL && src != dst);
    switch (src->type) {
        case LEPT_STRING:
            lept_set_string(dst, src->u.s.s, src->u.s.len);
            break;
        case LEPT_ARRAY:
            lept_set_array(dst, src->u.a.size);
            for (i = 0; i < src->u.a.size; ++i) {
                lept_copy(&dst->u.a.e[i], &src->u.a.e[i]);
            }
            dst->u.a.size = src->u.a.size;
            break;
        case LEPT_OBJECT:
            lept_set_object(dst, src->u.o.size);
            for (i = 0; i < src->u.o.size; ++i) {
                lept_value* val = lept_set_object_value(dst, src->u.o.m[i].k, src->u.o.m[i].klen);
                lept_copy(val, &src->u.o.m[i].v);
            }
            dst->u.o.size = src->u.o.size;
            break;
        default:
            lept_free(dst);
            memcpy(dst, src, sizeof(lept_value));
            break;
    }
}

void lept_move(lept_value* dst, lept_value* src) {
    assert(dst != NULL && src != NULL && src != dst);
    lept_free(dst);
    memcpy(dst, src, sizeof(lept_value));
    lept_init(src);
}

void lept_swap(lept_value* lhs, lept_value* rhs) {
    assert(lhs != NULL && rhs != NULL);
    if (lhs != rhs) {
        lept_value temp;
        memcpy(&temp, lhs, sizeof(lept_value));
        memcpy(lhs,   rhs, sizeof(lept_value));
        memcpy(rhs, &temp, sizeof(lept_value));
    }
}

void lept_free(lept_value* v) {
    size_t i;
    assert(v != NULL);
    switch (v->type) {
        case LEPT_STRING:
            free(v->u.s.s);
            break;
        case LEPT_ARRAY:
            for (i = 0; i < v->u.a.size; ++i) {
                lept_free(&v->u.a.e[i]);
            }
            free(v->u.a.e);//释放所有capacity
            break;
        case LEPT_OBJECT:
            for (i = 0; i < v->u.o.size; ++i) {
                free(v->u.o.m[i].k);
                lept_free(&(v->u.o.m[i].v));
            }
            free(v->u.o.m);//释放所有capacity
            break;
    }
    v->type = LEPT_NULL;
}

//////////////////////////json生成器部分
#ifndef LEPT_PARSE_STRINGIFY_INIT_SIZE
#define LEPT_PARSE_STRINGIFY_INIT_SIZE 256
#endif

static void lept_stringify_string(lept_context* c, const char* s, size_t len) {
    static const char hex_digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    size_t i, size;
    char* head, *p;
    assert(s != NULL);
    p = head = lept_context_push(c, size = len * 6 + 2); /* "\u00xx..." */
    *p++ = '\"';
    for (i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)s[i];
        switch (ch) {
            case '\"': *p++ = '\\'; *p++ = '\"'; break;
            case '\\': *p++ = '\\'; *p++ = '\\'; break;
            case '\b': *p++ = '\\'; *p++ = 'b';  break;
            case '\f': *p++ = '\\'; *p++ = 'f';  break;
            case '\n': *p++ = '\\'; *p++ = 'n';  break;
            case '\r': *p++ = '\\'; *p++ = 'r';  break;
            case '\t': *p++ = '\\'; *p++ = 't';  break;
            default:
                if (ch < 0x20) {//说明s是从unicode解析过来的，并且码点小于0x20，表示的是不可见字符
                    *p++ = '\\'; *p++ = 'u'; *p++ = '0'; *p++ = '0';
                    *p++ = hex_digits[ch >> 4];
                    *p++ = hex_digits[ch & 15];
                }
                else
                    *p++ = s[i];
        }
    }
    *p++ = '\"';
    c->top -= size - (p - head);
}

static void lept_stringify_value(lept_context* c, const lept_value* v) {
    size_t i;
    int ret;
    switch (v->type) {
        case LEPT_NULL:   PUTS(c, "null",  4); break;
        case LEPT_FALSE:  PUTS(c, "false", 5); break;
        case LEPT_TRUE:   PUTS(c, "true",  4); break;
        case LEPT_NUMBER:
            {
                char* buffer = lept_context_push(c, 32);
                int length = sprintf(buffer, "%.17g", v->u.n);
                c->top -= 32 - length;
            }
            break;
        case LEPT_STRING: lept_stringify_string(c, v->u.s.s, v->u.s.len); break;
        case LEPT_ARRAY:
            PUTC(c, '[');
            for (i = 0; i < v->u.a.size; ++i) {
                if (i > 0) {
                    PUTC(c, ',');
                }
                lept_stringify_value(c, &v->u.a.e[i]);
            }
            PUTC(c, ']');
            break;
        case LEPT_OBJECT:
            PUTC(c, '{');
            for (i = 0; i < v->u.o.size; ++i) {
                if (i > 0) {
                    PUTC(c, ',');
                }
                lept_stringify_string(c, v->u.o.m[i].k, v->u.o.m[i].klen);
                PUTC(c, ':');
                lept_stringify_value(c, &v->u.o.m[i].v);
            }
            PUTC(c, '}');
            break;
        default:
            assert(0 && "invalid type");
    }
}

char* lept_stringify(const lept_value* v, size_t* length) {
    lept_context c;
    int ret;
    assert(v != NULL);
    c.stack = (char *)malloc(c.size = LEPT_PARSE_STRINGIFY_INIT_SIZE);
    c.top = 0;
    lept_stringify_value(&c, v);
    if (length) {
        *length = c.top;
    }
    PUTC(&c, '\0');
    return c.stack;
}