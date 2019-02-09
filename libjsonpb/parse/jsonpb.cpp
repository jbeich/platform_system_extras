#include <jsonpb/jsonpb.h>

#include <android-base/logging.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/util/type_resolver_util.h>

namespace android {
namespace jsonpb {

using google::protobuf::DescriptorPool;
using google::protobuf::Message;
using google::protobuf::scoped_ptr;
using google::protobuf::util::NewTypeResolverForDescriptorPool;
using google::protobuf::util::TypeResolver;

static constexpr char kTypeUrlPrefix[] = "type.googleapis.com";

std::string GetTypeUrl(const Message& message) {
    return std::string(kTypeUrlPrefix) + "/" + message.GetDescriptor()->full_name();
}

ErrorOr<std::string> MessageToJsonString(const Message& message) {
    scoped_ptr<TypeResolver> resolver(
            NewTypeResolverForDescriptorPool(kTypeUrlPrefix, DescriptorPool::generated_pool()));

    google::protobuf::util::JsonOptions options;
    options.add_whitespace = true;

    std::string json;
    auto status = BinaryToJsonString(resolver.get(), GetTypeUrl(message),
                                     message.SerializeAsString(), &json, options);

    if (!status.ok()) {
        return MakeError<std::string>(status.error_message().as_string());
    }
    return ErrorOr<std::string>(std::move(json));
}

namespace internal {
ErrorOr<std::monostate> JsonStringToMessage(const std::string& content, Message* message) {
    scoped_ptr<TypeResolver> resolver(
            NewTypeResolverForDescriptorPool(kTypeUrlPrefix, DescriptorPool::generated_pool()));

    std::string binary;
    auto status = JsonToBinaryString(resolver.get(), GetTypeUrl(*message), content, &binary);
    if (!status.ok()) {
        return MakeError<std::monostate>(status.error_message().as_string());
    }
    if (!message->ParseFromString(binary)) {
        return MakeError<std::monostate>("Fail to parse.");
    }
    return ErrorOr<std::monostate>();
}
}  // namespace internal

}  // namespace jsonpb
}  // namespace android
