/**
 * @file math.64.c
 * @brief Math library for 64-bit floating point numbers.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <math.h>

MODULE("turnstone.lib");

#define MATH_TRESHOLD 0.00000000000001L

/**
 * @brief calculates approx for sin of number
 * @param[in] number number to calculate
 * @return sin(number)
 */
float64_t math_sin_approx(float64_t number);

int64_t math_ceil(float64_t num) {
    int64_t res = 0;
    boolean_t pos = true;

    if(num < 0) {
        pos = false;
        num = ABS(num);
    }

    res = (int64_t)num;

    if(res != num) {
        res++;
    }

    if(!pos) {
        res *= -1;
    }

    return res;
}

int64_t math_floor(float64_t num) {
    int64_t res = 0;
    boolean_t pos = true;

    if(num < 0) {
        pos = false;
        num = ABS(num);
    }

    res = (int64_t)num;

    if(!pos) {
        res *= -1;
    }

    return res;
}

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

float64_t math_antilog(float64_t power, float64_t base){
    float64_t t = math_log(power);
    t /= base;
    return math_log(t);
}

float64_t math_sin_approx(float64_t number) {
    float64_t res = number * (180 - number);
    res = res * (27900 + res);
    return res / 291600000;
}

float64_t math_sin(float64_t number) {
    uint64_t k = math_floor(number / 180);
    float64_t tmp = number - k * 180;

    if(tmp == 0 || tmp == 180) {
        return 0;
    }

    float64_t res = 0;

    switch(k % 2) {
    case 0:
        res = math_sin_approx(tmp);
        break;
    case 1:
        res = -math_sin_approx(360 - tmp);
        break;
    }

    return res;
}
