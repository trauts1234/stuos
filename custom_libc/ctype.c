#include "ctype.h"

int isalpha(int c) {
    return isupper(c) || islower(c);
}
int isalnum(int c) {
    return isalpha(c) || isdigit(c);
}
int isascii(int c) {
    return c >= 0 && c <= 127;
}
int isblank(int c) {
    return c == ' ' || c == '\t';
}
int iscntrl(int c) {
    unsigned int uc = (unsigned int)c;
    return ((uc <= 0x1F) || (uc == 0x7F)) ? 1 : 0;
}
int isdigit(int c) {
    return c >= '0' && c <= '9';
}
int isgraph(int c) {
    return c > 0x20 && c <= 0x7E;
}
int islower(int c) {
    return c >= 'a' && c <= 'z';
}
int isprint(int c) {
    return c >= 0x20 && c <= 0x7E;
}
int ispunct(int c) {
    return isprint(c) && c != ' ' && !isalnum(c);
}
int isspace(int c) {
    return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
}
int isupper(int c) {
    return c >= 'A' && c <= 'Z';
}
int isxdigit(int c) {
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int tolower(int c) {
    unsigned char uc = (unsigned char)c;

    if (uc >= 'A' && uc <= 'Z')
        return (int)(uc + 32);

    return c; 
}