/**
 * @file cppmemview.hpp
 * @brief C++ memory view
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___CPPMEMVIEW_H
#define ___CPPMEMVIEW_H 0

#include <types.h>
#include <assert.h>
#include <cppruntime/cppmemclass.hpp>

#ifdef __cplusplus

namespace tosccp {

template <class T>
class MemoryView : public NoCopyMove {
private:
T *     _data;
int64_t _size;
public:
explicit MemoryView();
explicit MemoryView(T * data, size_t size);

T*      data() const;
int64_t size();

T& operator[](int64_t i) const;

MemoryView<T> subview(int64_t start, int64_t len) const;
MemoryView<T> subview(int64_t start) const;
MemoryView<T> subview() const;

operator     bool() const;
bool operator!() const;

bool operator==(const MemoryView<T> & other) const;
bool operator!=(const MemoryView<T> & other) const;

};

template <class T>
class ReadonlyMemoryView : public MemoryView<T> {
public:
using MemoryView<T>::MemoryView;

const T& operator[](int64_t i) const;

};

} // namespace tosccp

#else
#error "C++ compiler required"
#endif


#endif
