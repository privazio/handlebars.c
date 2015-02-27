/* Generated by re2c 0.14 on Thu Feb 26 18:58:05 2015 */

#include "handlebars_scanners.h"

#define YYCTYPE char

short handlebars_scanner_next_whitespace(const char * s, short def)
{
    const YYCTYPE * YYCURSOR = s;
    
    for (;;) {
        
{
    YYCTYPE yych;

    yych = *YYCURSOR;
    if (yych <= ' ') {
        if (yych <= '\n') {
            if (yych <= 0x00) goto yy3;
            if (yych <= 0x08) goto yy9;
            if (yych <= '\t') goto yy5;
            goto yy7;
        } else {
            if (yych <= '\f') {
                if (yych <= '\v') goto yy5;
                goto yy9;
            } else {
                if (yych <= '\r') goto yy5;
                if (yych <= 0x1F) goto yy9;
                goto yy5;
            }
        }
    } else {
        if (yych <= 0xE0) {
            if (yych <= 0x7F) goto yy9;
            if (yych <= 0xC1) goto yy2;
            if (yych <= 0xDF) goto yy11;
            goto yy12;
        } else {
            if (yych <= 0xF0) {
                if (yych <= 0xEF) goto yy13;
                goto yy14;
            } else {
                if (yych <= 0xF3) goto yy15;
                if (yych <= 0xF4) goto yy16;
            }
        }
    }
yy2:
yy3:
    ++YYCURSOR;
    { break; }
yy5:
    ++YYCURSOR;
    { continue; }
yy7:
    ++YYCURSOR;
    { return 1; }
yy9:
    ++YYCURSOR;
    { return 0; }
yy11:
    yych = *++YYCURSOR;
    if (yych <= 0x7F) goto yy2;
    if (yych <= 0xBF) goto yy9;
    goto yy2;
yy12:
    yych = *++YYCURSOR;
    if (yych <= 0x9F) goto yy2;
    if (yych <= 0xBF) goto yy11;
    goto yy2;
yy13:
    yych = *++YYCURSOR;
    if (yych <= 0x7F) goto yy2;
    if (yych <= 0xBF) goto yy11;
    goto yy2;
yy14:
    yych = *++YYCURSOR;
    if (yych <= 0x8F) goto yy2;
    if (yych <= 0xBF) goto yy13;
    goto yy2;
yy15:
    yych = *++YYCURSOR;
    if (yych <= 0x7F) goto yy2;
    if (yych <= 0xBF) goto yy13;
    goto yy2;
yy16:
    ++YYCURSOR;
    if ((yych = *YYCURSOR) <= 0x7F) goto yy2;
    if (yych <= 0x8F) goto yy13;
    goto yy2;
}

    }
    
    return def;
}

short handlebars_scanner_prev_whitespace(const char * s, short def)
{
    const YYCTYPE * YYCURSOR = s;
    short match = def;
    
    for (;;) {
        
{
    YYCTYPE yych;
    yych = *YYCURSOR;
    if (yych <= ' ') {
        if (yych <= '\n') {
            if (yych <= 0x00) goto yy20;
            if (yych <= 0x08) goto yy26;
            if (yych <= '\t') goto yy22;
            goto yy24;
        } else {
            if (yych <= '\f') {
                if (yych <= '\v') goto yy22;
                goto yy26;
            } else {
                if (yych <= '\r') goto yy22;
                if (yych <= 0x1F) goto yy26;
                goto yy22;
            }
        }
    } else {
        if (yych <= 0xE0) {
            if (yych <= 0x7F) goto yy26;
            if (yych <= 0xC1) goto yy19;
            if (yych <= 0xDF) goto yy28;
            goto yy29;
        } else {
            if (yych <= 0xF0) {
                if (yych <= 0xEF) goto yy30;
                goto yy31;
            } else {
                if (yych <= 0xF3) goto yy32;
                if (yych <= 0xF4) goto yy33;
            }
        }
    }
yy19:
yy20:
    ++YYCURSOR;
    { break; }
yy22:
    ++YYCURSOR;
    { continue; }
yy24:
    ++YYCURSOR;
    { match = 1; continue; }
yy26:
    ++YYCURSOR;
    { match = 0; continue; }
yy28:
    yych = *++YYCURSOR;
    if (yych <= 0x7F) goto yy19;
    if (yych <= 0xBF) goto yy26;
    goto yy19;
yy29:
    yych = *++YYCURSOR;
    if (yych <= 0x9F) goto yy19;
    if (yych <= 0xBF) goto yy28;
    goto yy19;
yy30:
    yych = *++YYCURSOR;
    if (yych <= 0x7F) goto yy19;
    if (yych <= 0xBF) goto yy28;
    goto yy19;
yy31:
    yych = *++YYCURSOR;
    if (yych <= 0x8F) goto yy19;
    if (yych <= 0xBF) goto yy30;
    goto yy19;
yy32:
    yych = *++YYCURSOR;
    if (yych <= 0x7F) goto yy19;
    if (yych <= 0xBF) goto yy30;
    goto yy19;
yy33:
    ++YYCURSOR;
    if ((yych = *YYCURSOR) <= 0x7F) goto yy19;
    if (yych <= 0x8F) goto yy30;
    goto yy19;
}

    }
    
    return match;
}
