/**
 * @file cppmemclass.hpp
 * @brief C++ memory class
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___CPPMEMCLASS_H
#define ___CPPMEMCLASS_H 0

#ifdef __cplusplus

namespace tosccp {

class NoCopy {
protected:
NoCopy() = default;
~NoCopy() = default;
NoCopy(const NoCopy &) = delete;
NoCopy& operator=(const NoCopy &) = delete;
};

class NoMove {
protected:
NoMove() = default;
~NoMove() = default;
NoMove(NoMove &&) = delete;
NoMove& operator=(NoMove &&) = delete;
};

class NoCopyMove : public NoCopy, public NoMove {
protected:
NoCopyMove() = default;
~NoCopyMove() = default;
};

}

#else
#error "C++ compiler required"
#endif

#endif
