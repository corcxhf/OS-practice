#ifndef _CTYPE_H
#define _CTYPE_H

#ifdef __cplusplus
extern "C" {
#endif

int iscntrl(int c);
int isdigit(int c);
int isalpha(int c);
int isalnum(int c);
int islower(int c);
int isupper(int c);
int isprint(int c);
int isspace(int c);
int tolower(int c);
int toupper(int c);

#ifdef __cplusplus
}
#endif

#endif
