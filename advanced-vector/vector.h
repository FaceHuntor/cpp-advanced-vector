#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>


template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
            : buffer_(Allocate(capacity))
            , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept
    {
        Swap(other);
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        Swap(rhs);
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    Vector() = default;

    explicit Vector(size_t size)
            : data_(size)
            , size_(size)  //
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size_);
    }

    Vector(const Vector& other)
            : data_(other.size_)
            , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
    {
        Swap(other);
    };

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + std::min(rhs.size_, size_), data_.GetAddress());
                if(rhs.size_ <= size_) {
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                } else {
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        Swap(rhs);
        return *this;
    }

    void Swap(Vector& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    void Resize(size_t new_size) {
        if(size_ == new_size) {
            return;
        }
        if(new_size < size_) {
            std::destroy(data_.GetAddress() + new_size, data_.GetAddress() + size_);
        } else {
            Reserve(new_size);
            std::uninitialized_value_construct(data_.GetAddress() + size_, data_.GetAddress() + new_size);
        }
        size_ = new_size;
    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) noexcept {
        EmplaceBack(std::move(value));
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        return *Emplace(end(), std::forward<Args>(args)...);
    }

    void PopBack() {
        std::destroy_at(data_ + size_ - 1);
        --size_;
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);

        MoveOrCopyN(data_.GetAddress(), size_, new_data.GetAddress());

        std::destroy_n(data_.GetAddress(), size_);

        data_.Swap(new_data);
    }

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_ + size_;
    }

    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return data_ + size_;
    }

    const_iterator cbegin() const noexcept {
        return begin();
    }

    const_iterator cend() const noexcept {
        return end();
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        auto index = pos - begin();
        if(size_ < data_.Capacity()) {
            InsertWithoutReallocation(index, std::forward<Args>(args)...);
        } else {
            InsertWithReallocation(index, std::forward<Args>(args)...);
        }
        ++size_;
        return data_.GetAddress() + index;
    }

    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/ {
        if(pos == (end() - 1)) {
            PopBack();
            return end();
        }
        size_t index = pos - begin();
        for(auto i = index; i != size_ - 1; ++i) {
            data_[i] = std::forward<T>(data_[i + 1]);
        }
        std::destroy_at(data_ + size_ - 1);
        --size_;
        return data_ + index;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

private:
    static constexpr bool CanMove() {
        return std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>;
    }

    template<typename... Args>
    void InsertWithReallocation(size_t index, Args&&... args)
    {
        RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
        new (new_data + index) T(std::forward<Args>(args)...);
        try {
            MoveOrCopyN(data_.GetAddress(), index, new_data.GetAddress());
        } catch (...) {
            std::destroy_at(new_data + index);
            throw;
        }
        try {
            MoveOrCopyN(data_.GetAddress() + index, size_ - index, new_data.GetAddress() + index + 1);
        } catch (...) {
            std::destroy_n(new_data.GetAddress(), index + 1);
            throw;
        }

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    template<typename... Args>
    void InsertWithoutReallocation(size_t index, Args&&... args)
    {
        if(index == size_)
        {
            new (data_ + size_) T(std::forward<Args>(args)...);
        }
        else
        {
            T tmp(std::forward<Args>(args)...);
            new (data_ + size_) T(std::forward<T>(data_[size_ - 1]));
            try {
                std::move_backward(data_ + index, data_ + (size_ - 1), data_ + size_);
            } catch (...) {
                std::destroy_at(data_ + size_);
                throw;
            }
            data_[index] = std::forward<T>(tmp);
        }
    }

    template <typename InputIterator, typename ForwardIterator>
    void MoveOrCopyN(InputIterator first, size_t n, ForwardIterator result)
    {
        if constexpr (CanMove()) {
            std::uninitialized_move_n(first, n, result);
        } else {
            std::uninitialized_copy_n(first, n, result);
        }
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};