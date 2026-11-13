#include "dreamui/ink_style_snapshot.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>

namespace dreamui {
namespace {
constexpr std::uint32_t kMaxStyles=32;
constexpr std::uint32_t kMaxPropertiesPerStyle=1024;
constexpr std::uint32_t kMaxTotalProperties=4096;
std::uint32_t u32(const std::uint8_t* p,std::size_t off) {
    std::uint32_t x=0;
    std::memcpy(&x,p+off,sizeof(x));
    return x;
}
std::uint64_t u64(const std::uint8_t* p,std::size_t off) {
    std::uint64_t x=0;
    std::memcpy(&x,p+off,sizeof(x));
    return x;
}
bool read_at(const MemoryRead& read,std::uint64_t base,
             std::uint64_t off,void* out,std::size_t size) {
    return read && base>=0x10000 && off<=UINT64_MAX-base &&
           size<=UINT64_MAX-(base+off) &&
           read(base+off,out,size);
}
struct Array {
    std::uint64_t address=0;
    std::uint32_t capacity=0;
    std::uint32_t count=0;
};
bool read_array(const std::uint8_t* bytes,std::size_t off,
                Array& array) {
    array={u64(bytes,off),u32(bytes,off+8),u32(bytes,off+12)};
    return array.count<=array.capacity &&
           (array.count==0 || array.address>=0x10000);
}
} // namespace

InkStyleCaptureResult capture_loaded_ink_style_tree(
    const MemoryRead& read,std::uint64_t resource,std::uint64_t path,
    InkOwnedStyleTree& destination) {
    destination.clear();
    InkStyleCaptureResult result;
    if(!read || resource<0x10000 || path==0)return result;
    std::array<std::uint8_t,0x50> resource_header{};
    if(!read_at(read,resource,0,resource_header.data(),resource_header.size()) ||
       u64(resource_header.data(),0x30)!=path)return result;
    const auto instance_id=u64(resource_header.data(),0x28);
    if(!instance_id)return result;
    Array styles{};
    if(!read_array(resource_header.data(),0x40,styles))return result;
    result.observed_styles=styles.count;
    if(!styles.count) {
        result.status=InkStyleCaptureStatus::NoStyles;
        return result;
    }
    if(styles.count>kMaxStyles) {
        result.status=InkStyleCaptureStatus::LimitExceeded;
        return result;
    }

    // A provisional copy is committed to the destination ONLY after the
    // entire graph has passed. Rejecting any pointer or Variant cannot
    // leave a partial snapshot masquerading as a complete style tree.
    InkOwnedStyleTree staging;
    staging.resource_path_hash=path;
    staging.resource_instance_id=instance_id;
    staging.styles.reserve(styles.count);
    for(std::uint32_t si=0;si<styles.count;++si) {
        std::array<std::uint8_t,0x20> style{};
        if(!read_at(read,styles.address,
                    static_cast<std::uint64_t>(si)*0x20,
                    style.data(),style.size()))return result;
        Array props{};
        if(!read_array(style.data(),0x10,props))return result;
        if(props.count>kMaxPropertiesPerStyle ||
           props.count>kMaxTotalProperties-result.observed_properties) {
            result.status=InkStyleCaptureStatus::LimitExceeded;
            return result;
        }
        staging.styles.push_back({u64(style.data(),0),u64(style.data(),8),
            static_cast<std::uint32_t>(staging.properties.size()),props.count});
        for(std::uint32_t pi=0;pi<props.count;++pi) {
            std::array<std::uint8_t,0x20> prop{};
            if(!read_at(read,props.address,
                        static_cast<std::uint64_t>(pi)*0x20,
                        prop.data(),prop.size()))return result;
            const auto property_hash=u64(prop.data(),0);
            const auto type_word=u64(prop.data(),8);
            if(!property_hash || type_word<0x10000) {
                result.status=InkStyleCaptureStatus::InvalidVariant;
                return result;
            }
            const auto type=type_word & ~std::uint64_t{1};
            std::array<std::uint8_t,0x78> rtti{};
            if(!read_at(read,type,0,rtti.data(),rtti.size())) {
                result.status=InkStyleCaptureStatus::InvalidVariant;
                return result;
            }
            if(u64(rtti.data(),0x18)!=cname_hash("HDRColor")) {
                ++result.unsupported_properties;
                result.status=InkStyleCaptureStatus::UnsupportedVariant;
                return result;
            }
            // HDRColor is alignas(16); native Variant must NOT inline it.
            // Every color must refer to readable, 16-byte aligned storage
            // with type-authenticated size and alignment.
            const auto color_address=u64(prop.data(),0x10);
            if((type_word&1)!=0 || u32(rtti.data(),0x68)!=0x10 ||
               u32(rtti.data(),0x74)!=0x10 ||
               color_address<0x10000 || (color_address&15)!=0) {
                result.status=InkStyleCaptureStatus::InvalidVariant;
                return result;
            }
            HdrColor color{};
            if(!read_at(read,color_address,0,&color,sizeof(color)) ||
               !finite_hdr(color)) {
                result.status=InkStyleCaptureStatus::InvalidVariant;
                return result;
            }
            staging.properties.push_back({property_hash,color});
            ++result.observed_properties;
        }
    }
    result.status=InkStyleCaptureStatus::Captured;
    destination=std::move(staging);
    return result;
}
} // namespace dreamui
