
#pragma once

#include <string>
#include <variant>

#include <android-base/logging.h>

namespace android {
namespace jsonpb {

template <typename T>
struct ErrorOr {
    template <class... Args>
    explicit ErrorOr(Args&&... args) : data_(kIndex1, std::forward<Args>(args)...) {}
    T& operator*() {
        CHECK(ok());
        return *std::get_if<1u>(&data_);
    }
    const T& operator*() const {
        CHECK(ok());
        return *std::get_if<1u>(&data_);
    }
    T* operator->() {
        CHECK(ok());
        return std::get_if<1u>(&data_);
    }
    const T* operator->() const {
        CHECK(ok());
        return std::get_if<1u>(&data_);
    }
    const std::string& error() const {
        CHECK(!ok());
        return *std::get_if<0u>(&data_);
    }
    bool ok() const { return data_.index() != 0; }
    static ErrorOr<T> MakeError(const std::string& message) {
        return ErrorOr<T>(message, Tag::kDummy);
    }

  private:
    enum class Tag { kDummy };
    static constexpr std::in_place_index_t<0> kIndex0{};
    static constexpr std::in_place_index_t<1> kIndex1{};
    ErrorOr(const std::string& msg, Tag) : data_(kIndex0, msg) {}

    std::variant<std::string, T> data_;
};

template <typename T>
inline ErrorOr<T> MakeError(const std::string& message) {
    return ErrorOr<T>::MakeError(message);
}

}  // namespace jsonpb
}  // namespace android
