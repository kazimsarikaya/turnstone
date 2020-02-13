asm (".code16gcc");
#include <strings.h>
#include <memory.h>
#include <utils.h>


int strlen(char* string) {
	if(string==NULL) {
		return 0;
	}
	int ret = 0;
	while(string[ret]) {
		ret++;
	}
	return ret;
}

int strcmp(char* string1, char* string2) {
	int len1=strlen(string1);
	int len2=strlen(string2);
	int minlen = MIN(len1,len2);
	for(int i=0; i<minlen; i++) {
		if(string1[i]<string2[i]) {
			return -1;
		}
		if(string1[i]>string2[i]) {
			return 1;
		}
	}
	if(len1==len2) {
		return 0;
	}
	return (minlen == len1) ? -1 : 1;
}

int strcpy(char* source, char* destination){
	if(source==NULL || destination==NULL) {
		return -1;
	}
	for(int i=0; i<strlen(source); i++) {
		destination[i] = source[i];
	}
	return 0;
}

char* strrev(char* source) {
	int len = strlen(source);
	if(len==0) {
		return NULL;
	}
	char *dest=simple_kmalloc(sizeof(char)*len);
	if(dest == NULL) {
		return NULL;
	}
	for(int i=0; i<len; i++) {
		dest[i] = source[len-i-1];
	}
	return dest;
}

int ato_base(char* source, int base) {
	int ret = 0;
	int p = 0;
	for(int i=strlen(source)-1; i>=0; i--) {
		if(source[i]<='9') {
			ret += ((int)(source[i]-48)) * power(base,p);
		} else {
			ret += ((int)(source[i]-55)) * power(base,p);
		}
		p++;
	}
	return ret;
}

char* ito_base(int number,int base){
	if(base<2 || base >36) {
		return NULL;
	}
	int len = 0;
	int temp = number;
	while(temp) {
		temp /= base;
		len++;
	}
	char* ret = simple_kmalloc(sizeof(char)*len+1);
	if (ret == NULL) {
		return NULL;
	}
	int i=1;
	int r;
	while(number) {
		r = number % base;
		number /= base;
		if(r<10) {
			ret[len-i] = 48+r;
		} else {
			ret[len-i] = 55+r;
		}
		i++;
	}
	ret[len] = NULL;
	return ret;
}


int strindexof(char*,char*);
