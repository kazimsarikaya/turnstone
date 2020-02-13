#ifndef ___STRINGS_H
#define ___STRINGS_H 0

int strlen(char*);
int strcmp(char*, char*);
int strcpy(char*, char*);
char* strrev(char*);
int ato_base(char*, int);
#define atoi(number) ato_base(number,10)
#define atoh(number) ato_base(number,16)
char* ito_base(int,int);
#define itoa(number) ito_base(number,10)
#define itoh(number) ito_base(number,16)
int strindexof(char*,char*);

#endif
