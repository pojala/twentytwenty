/*
 *  twtw-json.c
 *  TwentyTwenty
 *
 *  Created by Pauli Ojala on 27.7.2009.
 *  Copyright 2009 Lacquer oy/ltd. All rights reserved.
 *
 */

#include "twtw-json.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>


static size_t readStringFromJSON(const char *orig, const char *end, char **outStr)
{
    gboolean isUnquoted = FALSE;
    const char *s = orig;
    
    if (*orig != '\"') {
        isUnquoted = TRUE;
    } else {
        s++;  // skip past opening quote
    }
    
    while (s < end) {
        if (isUnquoted) {
            if (*s == ' ' || *s == ',' || *s == '}' || *s == ']') break;
        }
        else if (*s == '\\' && s < (end - 1) && s[1] == '\"') {
            s++;
        } else {
            if (*s == '\"') break;
        }
        s++;
    }
    if (s >= end) {
        if (isUnquoted) printf("** unquoted string doesn't have terminating space\n");
        else printf("** string doesn't have closing quote\n");
        return 0;
    }
    
    int len = (isUnquoted) ? (s - orig) : (s - orig - 1);
    s = (isUnquoted) ? orig : orig + 1;
    
    *outStr = g_malloc(len + 1);
    char *o = *outStr;
    o[len] = 0;
    
    int i;
    for (i = 0; i < len; i++) {
        if ( !isUnquoted && (*s == '\\' && s < (end - 1) && *(s+1) == '\"')) {
            *o = '\"';
            s++;
        } else {
            *o = *s;
        }
        o++;
        s++;
    }
    return len + 2;
}


enum {
    stateExpectingDictOrArrayEnd = 0,
    stateExpectingKeyOrDictEnd,
    stateExpectingValue
};

gboolean twtw_minijson_parse_array_of_flat_dicts(const char *jsonStr, TwtwJSONKeyValuePair **outValues, size_t *outValueCount, int *outErrorPos)
{
    if ( !jsonStr) return FALSE;
    
    size_t len = strlen(jsonStr);
    if (len < 1) return FALSE;
    
    const char *end = jsonStr + len;
    const char *s = jsonStr;
    while (isspace(*s) && s < end) {
        s++;
    }
    if (s == end) return FALSE;

#define ERR_PARSE()  {  error = 1; \
                        if (outErrorPos) *outErrorPos = s - jsonStr;  \
                        goto bail;  }

    int error = 0;
    size_t topLevelCount = 0;
    TwtwJSONKeyValuePair *topLevelDicts = NULL;
    
    size_t latestValueCount = 0;
    TwtwJSONKeyValuePair *latestValues = NULL;
    char *latestKey = NULL;
    gint state = stateExpectingDictOrArrayEnd;

    if (*s != '[') ERR_PARSE();  // not an array
    s++;

    while (s < end) {
        if (isspace(*s)) {
            s++;
        }
        else {
            switch (state) {
                case stateExpectingDictOrArrayEnd: {
                    if (*s == ',') {
                        s++;
                        while (s < end && isspace(*s))
                            s++;
                        if (s == end) ERR_PARSE();
                    }
                    else if (*s == ']')
                        goto bail;
                        
                    else if (*s != '{')
                        ERR_PARSE();
                    
                    state = stateExpectingKeyOrDictEnd;
                    s++;
                    break;
                }
                    
                case stateExpectingKeyOrDictEnd: {
                    if (*s == '}') {
                        if (latestValueCount > 0) {
                            // the dict has ended, so store it in toplevel values
                            topLevelCount++;
                            topLevelDicts = (topLevelDicts) ? g_realloc(topLevelDicts, topLevelCount*sizeof(TwtwJSONKeyValuePair))
                                                            : g_malloc(topLevelCount*sizeof(TwtwJSONKeyValuePair));
                            
                            topLevelDicts[topLevelCount-1].type = TWTW_JSON_VALUE_IS_KEYVALUE_LIST;
                            topLevelDicts[topLevelCount-1].key = NULL;
                            topLevelDicts[topLevelCount-1].value = latestValues;
                            topLevelDicts[topLevelCount-1].valueArrayLength = latestValueCount;
                            latestValues = NULL;
                            latestValueCount = 0;
                        }
                        state = stateExpectingDictOrArrayEnd;
                        s++;
                        continue;
                    }
                    else if (*s == ',') {
                        s++;
                        while (s < end && isspace(*s))
                            s++;
                        if (s == end) ERR_PARSE();
                    }
                
                    char *str = NULL;
                    size_t readBytes = readStringFromJSON(s, end, &str);
                    if (readBytes < 2 || !str)
                        ERR_PARSE();
                    
                    s += readBytes;
                    latestKey = str;
                    state = stateExpectingValue;
                    break;
                }
                    
                case stateExpectingValue: {
                    if (*s == ':') {
                        s++;
                        while (s < end && isspace(*s))
                            s++;
                        if (s == end) ERR_PARSE();
                    }
                
                    char *str = NULL;
                    size_t readBytes = readStringFromJSON(s, end, &str);
                    if (readBytes < 2 || !str)
                        ERR_PARSE();
                        
                    s += readBytes;
                    
                    latestValueCount++;
                    latestValues = (latestValues) ? g_realloc(latestValues, latestValueCount*sizeof(TwtwJSONKeyValuePair))
                                                  : g_malloc(latestValueCount*sizeof(TwtwJSONKeyValuePair));
                    
                    memset(latestValues + latestValueCount - 1, 0, sizeof(TwtwJSONKeyValuePair));
                    latestValues[latestValueCount-1].type = TWTW_JSON_VALUE_IS_STRING;
                    latestValues[latestValueCount-1].key = latestKey;
                    latestValues[latestValueCount-1].value = str;
                    
                    latestKey = NULL;
                    state = stateExpectingKeyOrDictEnd;
                    break;
                }
            }
        }
    }
    
#undef ERR_PARSE
    
bail:
    if (error) {
        g_free(latestKey);
        twtw_minijson_destroy_kv_list(latestValues, latestValueCount);
        twtw_minijson_destroy_kv_list(topLevelDicts, topLevelCount);
        return FALSE;
    } else {
        *outValues = topLevelDicts;
        *outValueCount = topLevelCount;
        return TRUE;
    }
}


void twtw_minijson_destroy_kv_list(TwtwJSONKeyValuePair *values, size_t count)
{
    long i;
    for (i = 0; i < count; i++) {
        g_free(values[i].key);
        
        if (values[i].type == TWTW_JSON_VALUE_IS_KEYVALUE_LIST) {
            twtw_minijson_destroy_kv_list ((TwtwJSONKeyValuePair *)values[i].value, values[i].valueArrayLength);
        }
        else {
            g_free(values[i].value);
        }
    }
    
    g_free(values);
}

gint twtw_minijson_kv_list_find_key(TwtwJSONKeyValuePair *values, size_t count, const char *keyToFind)
{
    long i;
    for (i = 0; i < count; i++) {
        if (values[i].key && 0 == strcmp(keyToFind, values[i].key)) {
            return i;
        }
    }
    return -1;
}
