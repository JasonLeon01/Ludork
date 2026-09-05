#include <UI/UiControlAdapterRegistry.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::array<std::uint32_t, 64> sha256Constants = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu,
    0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
    0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
    0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u,
    0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u,
    0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
    0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
    0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

std::string sha256(std::string_view value) {
    std::vector<std::uint8_t> message(value.begin(), value.end());
    const std::uint64_t bitLength =
        static_cast<std::uint64_t>(message.size()) * 8u;
    message.push_back(0x80u);
    while (message.size() % 64u != 56u) {
        message.push_back(0u);
    }
    for (int shift = 56; shift >= 0; shift -= 8) {
        message.push_back(
            static_cast<std::uint8_t>((bitLength >> shift) & 0xffu));
    }

    std::array<std::uint32_t, 8> hash = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
    };
    for (std::size_t block = 0; block < message.size(); block += 64u) {
        std::array<std::uint32_t, 64> schedule{};
        for (std::size_t index = 0; index < 16u; ++index) {
            const std::size_t offset = block + index * 4u;
            schedule[index] =
                (static_cast<std::uint32_t>(message[offset]) << 24u) |
                (static_cast<std::uint32_t>(message[offset + 1u]) << 16u) |
                (static_cast<std::uint32_t>(message[offset + 2u]) << 8u) |
                static_cast<std::uint32_t>(message[offset + 3u]);
        }
        for (std::size_t index = 16u; index < schedule.size(); ++index) {
            const std::uint32_t first = std::rotr(schedule[index - 15u], 7) ^
                                        std::rotr(schedule[index - 15u], 18) ^
                                        (schedule[index - 15u] >> 3u);
            const std::uint32_t second = std::rotr(schedule[index - 2u], 17) ^
                                         std::rotr(schedule[index - 2u], 19) ^
                                         (schedule[index - 2u] >> 10u);
            schedule[index] =
                schedule[index - 16u] + first + schedule[index - 7u] + second;
        }

        std::array<std::uint32_t, 8> state = hash;
        for (std::size_t index = 0; index < schedule.size(); ++index) {
            const std::uint32_t sum1 = std::rotr(state[4], 6) ^
                                       std::rotr(state[4], 11) ^
                                       std::rotr(state[4], 25);
            const std::uint32_t choice =
                (state[4] & state[5]) ^ (~state[4] & state[6]);
            const std::uint32_t temporary1 = state[7] + sum1 + choice +
                                             sha256Constants[index] +
                                             schedule[index];
            const std::uint32_t sum0 = std::rotr(state[0], 2) ^
                                       std::rotr(state[0], 13) ^
                                       std::rotr(state[0], 22);
            const std::uint32_t majority = (state[0] & state[1]) ^
                                           (state[0] & state[2]) ^
                                           (state[1] & state[2]);
            const std::uint32_t temporary2 = sum0 + majority;
            state[7] = state[6];
            state[6] = state[5];
            state[5] = state[4];
            state[4] = state[3] + temporary1;
            state[3] = state[2];
            state[2] = state[1];
            state[1] = state[0];
            state[0] = temporary1 + temporary2;
        }
        for (std::size_t index = 0; index < hash.size(); ++index) {
            hash[index] += state[index];
        }
    }

    constexpr std::string_view digits = "0123456789abcdef";
    std::string result;
    result.reserve(hash.size() * 8u);
    for (const std::uint32_t word : hash) {
        for (int shift = 28; shift >= 0; shift -= 4) {
            result.push_back(digits[(word >> shift) & 0x0fu]);
        }
    }
    return result;
}

std::string_view childPolicyName(UiChildPolicy policy) {
    switch (policy) {
        case UiChildPolicy::None:
            return "none";
        case UiChildPolicy::Single:
            return "single";
        case UiChildPolicy::Multiple:
            return "multiple";
    }
    throw std::logic_error("Unknown UI child policy");
}

std::string_view slotTypeName(UiControlSlotType slotType) {
    switch (slotType) {
        case UiControlSlotType::None:
            return {};
        case UiControlSlotType::Canvas:
            return "canvas";
        case UiControlSlotType::List:
            return "list";
    }
    throw std::logic_error("Unknown UI Slot type");
}

std::string adapterFingerprintSource() {
    std::vector<const UiControlAdapterDescriptor*> descriptors;
    descriptors.reserve(uiControlAdapterDescriptorTable.size());
    for (const UiControlAdapterDescriptor& descriptor :
         uiControlAdapterDescriptorTable) {
        descriptors.push_back(&descriptor);
    }
    std::sort(descriptors.begin(), descriptors.end(),
              [](const UiControlAdapterDescriptor* left,
                 const UiControlAdapterDescriptor* right) {
                  return left->controlId < right->controlId;
              });

    std::string source;
    for (const UiControlAdapterDescriptor* descriptor : descriptors) {
        source.append(descriptor->controlId);
        source.push_back('|');
        source.append(descriptor->adapter);
        source.push_back('|');
        source.append(childPolicyName(descriptor->childPolicy));
        source.push_back('|');
        source.append(slotTypeName(descriptor->slotType));
        source.push_back('|');
        for (const UiControlPropertyDescriptor& property :
             descriptor->properties) {
            if (property.editorOnly) {
                continue;
            }
            source.append(property.id);
            source.push_back(':');
            source.append(property.type);
            source.push_back(':');
            source.append(property.required ? "true" : "false");
            source.push_back(';');
        }
        source.push_back('\n');
    }
    return source;
}

}  // namespace

std::string_view uiControlAdapterFingerprint() {
    static const std::string fingerprint = sha256(adapterFingerprintSource());
    return fingerprint;
}
