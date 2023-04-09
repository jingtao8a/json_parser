#ifndef LEPTJSON_H__
#define LEPTJSON_H__

#include <stddef.h> //size_t

typedef enum { LEPT_NULL, LEPT_FALSE, LEPT_TRUE, LEPT_NUMBER, LEPT_STRING, LEPT_ARRAY, LEPT_OBJECT } lept_type;

typedef struct lept_value lept_value;
typedef struct lept_member lept_member;

#define LEPT_KEY_NOT_EXIST ((size_t)-1)

struct lept_value {
    union{
        struct {lept_member* m; size_t size, capacity;} o;//对象
        struct {lept_value* e; size_t size, capacity;} a;//数组
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
char* lept_stringify(const lept_value* v, size_t* length);//生成json

void lept_copy(lept_value* dst, lept_value* src);//深拷贝
void lept_move(lept_value* dst, lept_value* src);//移动语义
void lept_swap(lept_value* dst, lept_value* src);//交换

void lept_free(lept_value* v);

lept_type lept_get_type(const lept_value* v);
int lept_is_equal(const lept_value* lhs, const lept_value* rhs);//相等比较

#define lept_set_null(v) lept_free(v)

int lept_get_boolean(const lept_value* v);
void lept_set_boolean(lept_value* v, int b);

double lept_get_number(const lept_value* v);
void lept_set_number(lept_value* v, double n);

const char* lept_get_string(const lept_value* v);
size_t lept_get_string_length(const lept_value* v);
void lept_set_string(lept_value* v, const char* s, size_t len);

void lept_set_array(lept_value* v, size_t capacity);//设置array的capacity
size_t lept_get_array_size(const lept_value* v);
size_t lept_get_array_capacity(const lept_value* v);
void lept_reserve_array(lept_value* v, size_t capacity);//重新设置array的capacity
void lept_shrink_array(lept_value* v);//收缩array的capacity
void lept_clear_array(lept_value* v);//清空array的元素
lept_value* lept_get_array_element(lept_value* v, size_t index);
lept_value* lept_pushback_array_element(lept_value* v);//往array中push一个element，size++
void lept_popback_array_element(lept_value* v);//从array中pop一个element，size--
lept_value* lept_insert_array_element(lept_value* v, size_t index);//从index处插入一个元素
void lept_erase_array_element(lept_value* v, size_t index, size_t count);//删除array从index开始的count个元素

void lept_set_object(lept_value* v, size_t capacity);//设置object的capacity
size_t lept_get_object_size(const lept_value* v);
size_t lept_get_object_capacity(const lept_value* v);
void lept_reserve_object(lept_value* v, size_t capacity);
void lept_shrink_object(lept_value* v);
void lept_clear_object(lept_value* v);
const char* lept_get_object_key(const lept_value* v, size_t index);
size_t lept_get_object_key_length(const lept_value* v, size_t index);
lept_value* lept_get_object_value(lept_value* v, size_t index);
size_t lept_find_object_index(const lept_value* v, const char* key, size_t klen);
lept_value* lept_find_object_value(lept_value* v, const char* key, size_t klen);
lept_value* lept_set_object_value(lept_value* v, const char* key, size_t klen);
void lept_remove_object_value(lept_value* v, size_t index);


#endif