/**
 * @file cppstring.64.cpp
 * @brief C++ string class
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <cppruntime/cppstring.hpp>
#include <strings.h>
#include <assert.h>

MODULE("turnstone.cppruntime.cppstring");

namespace tosccp {

string::string() : _str(nullptr), _len(0) {
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-use-of-uninitialized-value"
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
// Constructor for given length
string::string(int64_t len) {
    _len = len;
    _str = new char[_len + 1];
    assert(_str != nullptr && "Failed to allocate memory\n");
}
#pragma GCC diagnostic pop

// Constructor from C-string
string::string(const char_t* s) {
    _len = strlen(s);
    _str = new char[_len + 1];
    assert(_str != nullptr && "Failed to allocate memory\n");
    strcopy(s, _str);
}

// Copy constructor
string::string(const string & s) {
    _len = s._len;
    _str = new char[_len + 1];
    assert(_str != nullptr && "Failed to allocate memory\n");
    strcopy(s._str, _str);
}

// Move constructor
string::string(string && s) noexcept
    : _str(s._str), _len(s._len) {
    s._str = nullptr;
    s._len = 0;
}

// Destructor
string::~string() {
    delete[] _str;
}

// Copy assignment operator
string& string::operator=(const string & s) {
    if (this != &s) {
        delete[] _str;
        _len = s._len;
        _str = new char[_len + 1];
        assert(_str != nullptr && "Failed to allocate memory\n");
        strcopy(s._str, _str);
    }
    return *this;
}

// Move assignment operator
string& string::operator=(string && s) noexcept {
    if (this != &s) {
        delete[] _str;
        _str = s._str;
        _len = s._len;
        s._str = nullptr;
        s._len = 0;
    }
    return *this;
}

// Get string size
size_t string::size() const {
    return _len;
}

// Access elements
char& string::operator[](int64_t i) {
    assert(i >= 0 && i < _len && "Index out of range\n");
    return _str[i];
}

const char& string::operator[](int64_t i) const {
    assert(i >= 0 && i < _len && "Index out of range\n");
    return _str[i];
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-use-of-uninitialized-value"
// Concatenation operator
string string::operator+(const string & s) const {
    string newstr(_len + s._len); // Allocate new string
    strcopy(_str, newstr._str); // Copy first part
    strcopy(s._str, newstr._str + _len); // Copy second part
    return newstr; // Return new string
}
#pragma GCC diagnostic pop

// Append operator
string& string::operator+=(const string & s) {
    char* t = new char[_len + s._len + 1];
    assert(t != nullptr && "Failed to allocate memory\n");
    strcopy(_str, t);
    strcopy(s._str, t + _len);
    delete[] _str;
    _str = t;
    _len += s._len;
    return *this;
}

// Equality comparison
bool string::operator==(const string & s) const {
    return strcmp(_str, s._str) == 0;
}

// Inequality comparison
bool string::operator!=(const string & s) const {
    return strcmp(_str, s._str) != 0;
}

// Substring extraction
string* string::substr(int64_t start, int64_t len) const {
    assert(start >= 0 && start < _len && "Index out of range\n");
    assert(len >= 0 && start + len <= _len && "Index out of range\n");
    string* s = new string(len);
    assert(s != nullptr && "Failed to allocate memory\n");
    strcopy(_str + start, s->_str);
    return s;
}

// Get C-string
const char_t* string::c_str() const {
    return _str;
}


} // namespace tosccp
