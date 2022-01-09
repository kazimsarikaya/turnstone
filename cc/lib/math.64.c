#include <math.h>

#define MATH_TRESHOLD 0.00000000000001L

float64_t math_power(float64_t base, float64_t p) {
	return math_exp(p * math_log(base));
}

float64_t math_exp(float64_t number){
	float64_t x, p, frac, i, l;
	x = number;
	frac = x;
	p = (1.0 + x);
	i = 1.0;

	do {
		i++;
		frac *= (x / i);
		l = p;
		p += frac;
	}while(l != p);

	return p;
}

float64_t math_log(float64_t number){
	float64_t n, p, l, r, a;

	p = number;
	n = 0.0;

	while(p >= EXP) {
		p /= EXP;
		n++;
	}

	n += (p / EXP);
	p = number;

	do {
		a = n;
		l = (p / (math_exp(n - 1.0)));
		r = ((n - 1.0) * EXP);
		n = ((l + r) / EXP);
	}while(n != a && ABS((n - a)) > MATH_TRESHOLD);

	return n;
}

float64_t math_root(float64_t number, float64_t root) {
	float64_t a, n, s, l, r;
	a = number;
	n = root;
	s = 1.0;

	do {
		l = (a / math_power(s, (n - 1.0)));
		r = (n - 1.0) *  s;
		s = (l + r) / n;
	}while(l != s && ABS((l - s)) > MATH_TRESHOLD);

	return s;
}

float64_t math_antilog(float64_t power, float64_t exp){
	float64_t t = math_log(power);
	t /= exp;
	return math_log(t);
}
