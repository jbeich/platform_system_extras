
#pragma once

#include <string>

#include <jsonpb/error_or.h>

#include <google/protobuf/message.h>

namespace android {
namespace jsonpb {

namespace internal {
ErrorOr<std::monostate> JsonStringToMessage(const std::string& content,
                                            google::protobuf::Message* message);
}  // namespace internal

// TODO: JsonStringToMessage is a newly added function in protobuf
// and is not yet available in the android tree. Replace this function with
// https://developers.google.com/protocol-buffers/docs/reference/cpp/google.protobuf.util.json_util#JsonStringToMessage.details
// when the android tree gets updated
template <typename T>
ErrorOr<T> JsonStringToMessage(const std::string& content) {
    ErrorOr<T> ret;
    auto error = internal::JsonStringToMessage(content, &*ret);
    if (!error.ok()) {
        return MakeError<T>(error.error());
    }
    return ret;
}

// TODO: MessageToJsonString is a newly added function in protobuf
// and is not yet available in the android tree. Replace this function with
// https://developers.google.com/protocol-buffers/docs/reference/cpp/google.protobuf.util.json_util#MessageToJsonString.details
// when the android tree gets updated.
//
// The new MessageToJsonString also allows preserving proto field names. However,
// the function here can't. Hence, a field name "foo_bar" without json_name option
// will be "fooBar" in the final output. Additional checks are needed to ensure
// that doesn't happen.
ErrorOr<std::string> MessageToJsonString(const google::protobuf::Message& message);

}  // namespace jsonpb
}  // namespace android
