/*
 *  twtw-json.h
 *  TwentyTwenty
 *
 *  Created by Pauli Ojala on 27.7.2009.
 *  Copyright 2009 Lacquer oy/ltd. All rights reserved.
 *
 */

#ifndef _TWTW_JSON_H_
#define _TWTW_JSON_H_

#include "twtw-glib.h"
#include "twtw-fixedpoint.h"


typedef enum {
    TWTW_JSON_VALUE_IS_STRING = 1,
    TWTW_JSON_VALUE_IS_KEYVALUE_LIST
} TwtwJSONValueType;

typedef struct {
    TwtwJSONValueType type;
    char *key;
    void *value;
    size_t valueArrayLength;
} TwtwJSONKeyValuePair;



#ifdef __cplusplus
extern "C" {
#endif


gboolean twtw_minijson_parse_array_of_flat_dicts(const char *jsonStr, TwtwJSONKeyValuePair **outValues, size_t *outValueCount, int *outErrorPos);

void twtw_minijson_destroy_kv_list(TwtwJSONKeyValuePair *values, size_t count);

gint twtw_minijson_kv_list_find_key(TwtwJSONKeyValuePair *values, size_t count, const char *keyToFind);


TWTWINLINE FUNCATTR_ALWAYS_INLINE void twtw_kv_set_string(TwtwJSONKeyValuePair *kv, const char *key, const char *value)
{
    kv->type = TWTW_JSON_VALUE_IS_STRING;
    kv->key = (char *)key;
    kv->value = (void *)value;
}


#ifdef __cplusplus
}
#endif

#endif