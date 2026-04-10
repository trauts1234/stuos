#ifndef _STDIO_H
#define _STDIO_H

#include "stddef.h"
#include "stdarg.h"

typedef struct {
    int file_descriptor_number;
} FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

//used for whence, when lseeking
# define SEEK_SET	0	/* Seek from beginning of file.  */
# define SEEK_CUR	1	/* Seek from current position.  */
# define SEEK_END	2	/* Seek from end of file.  */

int printf(const char* format, ...);
int vprintf(const char* format, va_list arg);
int  sprintf(char* s, const char* format, ...);
int vsprintf(char* s, const char* format, va_list arg);
int  snprintf(char* s, size_t count, const char* format, ...);
int vsnprintf(char* s, size_t count, const char* format, va_list arg);
int fctprintf(void (*out)(char c, void* extra_arg), void* extra_arg, const char* format, ...);
int vfctprintf(void (*out)(char c, void* extra_arg), void* extra_arg, const char* format, va_list arg);

int fprintf(FILE* stream, const char* format, ...);
int vfprintf(FILE *stream, const char *format, va_list ap);

int fputc(int c, FILE *stream);
int putc(int c, FILE *stream);
int putchar(int c);
int puts(const char *s);

FILE   *fopen(const char *pathname, const char *mode);
int     fclose(FILE *stream);

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fputs(const char *s, FILE *stream);
char *fgets(char *s, int size, FILE *stream);

int    fseek(FILE *stream, long offset, int whence);

int fflush (FILE *);

#endif