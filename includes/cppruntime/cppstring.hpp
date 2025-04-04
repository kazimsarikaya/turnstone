/**
 * @file cppstring.hpp
 * @brief C++ string class
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___CPPSTRING_H
#define ___CPPSTRING_H 0

#include <types.h>

#ifdef __cplusplus

namespace tosccp {

class string {
private:
char_t * _str = nullptr;
int64_t  _len = 0;

public:
// Constructors
explicit string();
explicit string(int64_t len);
explicit string(const char_t * s);

// Copy and move constructors
string(const string & s);
string(string && s) noexcept;

// Destructor
~string();

// Assignment operators
string& operator=(const string & s);
string& operator=(string && s) noexcept;

// Member functions
size_t              size() const;
char& operator      [](int64_t i);
const char& operator[](int64_t i) const;

// Concatenation and append
string operator +(const string & s) const;
string& operator+=(const string & s);

// Comparison operators
bool operator==(const string & s) const;
bool operator!=(const string & s) const;

// Substring
string* substr(int64_t start, int64_t len) const;

// Get C-string
const char_t* c_str() const;
};

}

#else
#error "C++ compiler required"
#endif

#endif
