#ifndef RawArray_hpp
#define RawArray_hpp

namespace abaci::utility {

template<typename T>
class RawArray {
    T** ptr;
    std::size_t capacity{ 1 }, arraySize{};
public:
    RawArray(T **ptr) : ptr{ ptr } {
        *ptr = new T[capacity];
    }
    ~RawArray() {
        delete[] *ptr;
        *ptr = nullptr;
    }
    auto size() const {
        return arraySize;
    }
    auto getCapacity() const {
        return capacity;
    }
    std::size_t add() {
        if (++arraySize > capacity) {
            grow();
        }
        return arraySize - 1;
    }
    void grow() {
        auto oldSize = capacity;
        auto *oldPtr = *ptr;
        capacity *= 2;
        *ptr = new T[capacity];
        memcpy(*ptr, oldPtr, sizeof(T) * oldSize);
        delete[] oldPtr;
    }
};

} // namespace abaci::utility

#endif