asm (".code16gcc");
#include <utils.h>
#include <memory.h>

int power(int base,int p) {
	if (p == 0 ) {
		return 1;
	}
	int ret = 1;
	while(p) {
		if(p&0x1) {
			p--;
			ret *= base;
		} else {
			p /=2;
			base *= base;
		}
	}
	return ret;
}
