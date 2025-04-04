/**
 * @file cppmemview.64.cpp
 * @brief C++ memory view
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <cppruntime/cppmemview.hpp>
#include <assert.h>

MODULE("turnstone.cppruntime.cppmemview");

namespace tosccp {

template <class T>
MemoryView<T>::MemoryView()
    : _data(nullptr), _size(0) {
}

template <class T>
MemoryView<T>::MemoryView(T * data, size_t size)
    : _data(data), _size(size) {
}

template <class T>
T* MemoryView<T>::data() const {
    return _data;
}

template <class T>
int64_t MemoryView<T>::size() {
    return _size;
}

template <class T>
T& MemoryView<T>::operator[](int64_t i) const {
    assert(i >= 0 && i < _size && "Index out of range\n");
    return _data[i];
}

template <class T>
MemoryView<T> MemoryView<T>::subview(int64_t start, int64_t len) const {
    assert(start >= 0 && start < _size && "Index out of range\n");
    assert(len >= 0 && start + len <= _size && "Index out of range\n");
    return MemoryView<T>(_data + start, len);
}

template <class T>
MemoryView<T> MemoryView<T>::subview(int64_t start) const {
    assert(start >= 0 && start < _size && "Index out of range\n");
    return MemoryView<T>(_data + start, _size - start);
}

template <class T>
MemoryView<T> MemoryView<T>::subview() const {
    return MemoryView<T>(_data, _size);
}

template <class T>
MemoryView<T>::operator bool() const {
    return _data != nullptr;
}

template <class T>
bool MemoryView<T>::operator!() const {
    return _data == nullptr;
}

template <class T>
bool MemoryView<T>::operator==(const MemoryView<T> & other) const {
    if (_size != other._size) {
        return false;
    }

    for (int64_t i = 0; i < _size; i++) {
        if (_data[i] != other._data[i]) {
            return false;
        }
    }

    return true;
}

template <class T>
bool MemoryView<T>::operator!=(const MemoryView<T> & other) const {
    return !(*this == other);
}

template class MemoryView<char_t>;
template class MemoryView<int16_t>;
template class MemoryView<int32_t>;
template class MemoryView<int64_t>;
template class MemoryView<uint8_t>;
template class MemoryView<uint16_t>;
template class MemoryView<uint32_t>;
template class MemoryView<uint64_t>;

template <class T>
const T& ReadonlyMemoryView<T>::operator[](int64_t i) const {
    return MemoryView<T>::operator[](i);
}

template class ReadonlyMemoryView<char_t>;
template class ReadonlyMemoryView<int16_t>;
template class ReadonlyMemoryView<int32_t>;
template class ReadonlyMemoryView<int64_t>;
template class ReadonlyMemoryView<uint8_t>;
template class ReadonlyMemoryView<uint16_t>;
template class ReadonlyMemoryView<uint32_t>;
template class ReadonlyMemoryView<uint64_t>;


} // namespace tosccp
