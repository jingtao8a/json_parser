#ifndef LEPTJSON_H__
#define LEPTJSON_H__

#include <stddef.h> //size_t

typedef enum { LEPT_NULL, LEPT_FALSE, LEPT_TRUE, LEPT_NUMBER, LEPT_STRING, LEPT_ARRAY, LEPT_OBJECT } lept_type;

typedef struct lept_value lept_value;
typedef struct lept_member lept_member;


struct lept_value {
    union{
        struct {lept_member* m; size_t size;} o;//对象
        struct {lept_value* e; size_t size;} a;//数组
        struct {char *s; size_t len;} s;//字符串
        double n;//数字
    }u;
    lept_type type;
};//节点

struct lept_member {
    char* k; size_t klen;//member key 字符串
    lept_value v;//member value
};

enum {
    LEPT_PARSE_OK = 0,
    LEPT_PARSE_EXPECT_VALUE,//没有json字符串
    LEPT_PARSE_INVALID_VALUE,//错误的值
    LEPT_PARSE_ROOT_NOT_SINGULAR,//没有根lept_value
    LEPT_PARSE_NUMBER_TOO_BIG,//解析的数字太大
    LEPT_PARSE_MISS_QUOTATION_MARK,//解析字符串时没有右双引号
    LEPT_PARSE_INVALID_STRING_ESCAPE,//解析字符串时，错误的转义符
    LEPT_PARSE_INVALID_STRING_CHAR,//解析字符串时错误的符号
    LEPT_PARSE_INVALID_UNICODE_HEX,//解析UNICODE错误1
    LEPT_PARSE_INVALID_UNICODE_SURROGATE,//解析UNICODE错误2
    LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET,//解析数组时没有逗号和右括号封闭
    LEPT_PARSE_MISS_KEY,//解析对象时没有key
    LEPT_PARSE_MISS_COLON,//解析对象时没有冒号
    LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET,//解析对象时没有以逗号或}结尾
};//解析json时的返回值

#define lept_init(v) do { (v)->type = LEPT_NULL; } while(0)

int lept_parse(lept_value* v, const char* json);//解析json

void lept_free(lept_value* v);

lept_type lept_get_type(const lept_value* v);

#define lept_set_null(v) lept_free(v)

int lept_get_boolean(const lept_value* v);
void lept_set_boolean(lept_value* v, int b);

double lept_get_number(const lept_value* v);
void lept_set_number(lept_value* v, double n);

const char* lept_get_string(const lept_value* v);
size_t lept_get_string_length(const lept_value* v);
void lept_set_string(lept_value* v, const char* s, size_t len);

size_t lept_get_array_size(const lept_value* v);
lept_value* lept_get_array_element(const lept_value* v, size_t index);

size_t lept_get_object_size(const lept_value* v);
const char* lept_get_object_key(const lept_value* v, size_t index);
size_t lept_get_object_key_length(const lept_value* v, size_t index);
lept_value* lept_get_object_value(const lept_value* v, size_t index);

#endif