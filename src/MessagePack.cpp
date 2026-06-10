#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include "mpack.h"
#include "MessagePack.hpp"

namespace pkpy {

static Variant mpack_to_gd(mpack_node_t node) {
    mpack_type_t type = mpack_node_type(node);

    switch(type) {
        case mpack_type_nil: return Variant();

        case mpack_type_bool: return mpack_node_bool(node);

        case mpack_type_int: return mpack_node_i64(node);

        case mpack_type_uint: return mpack_node_u64(node);

        case mpack_type_float: return mpack_node_float(node);

        case mpack_type_double: return mpack_node_double(node);

        case mpack_type_str: {
            const char* str = mpack_node_str(node);
            size_t len = mpack_node_strlen(node);
            return String::utf8(str, len);
        }

        case mpack_type_bin: {
            const char* data = mpack_node_bin_data(node);
            size_t len = mpack_node_bin_size(node);
            PackedByteArray arr;
            arr.resize(len);
            memcpy(arr.ptrw(), data, len);
            return arr;
        }

        case mpack_type_array: {
            size_t count = mpack_node_array_length(node);
            Array arr;
            arr.resize(count);
            for(int i = 0; i < count; i++) {
                mpack_node_t child = mpack_node_array_at(node, i);
                arr[i] = mpack_to_gd(child);
            }
            return arr;
        }

        case mpack_type_map: {
            size_t count = mpack_node_map_count(node);
            Dictionary dict;
            for(size_t i = 0; i < count; i++) {
                mpack_node_t key_node = mpack_node_map_key_at(node, i);
                mpack_node_t val_node = mpack_node_map_value_at(node, i);
                Variant key = mpack_to_gd(key_node);
                Variant value = mpack_to_gd(val_node);
                dict[key] = value;
            }
            return dict;
        }
        default: return Variant(); // should never happen
    }
}

static bool gd_to_mpack(Variant object, mpack_writer_t* writer) {
    switch(object.get_type()) {
        case Variant::NIL: mpack_write_nil(writer); break;
        case Variant::BOOL: mpack_write_bool(writer, (bool)object); break;
        case Variant::INT: mpack_write_int(writer, (int64_t)object); break;
        case Variant::FLOAT: mpack_write_double(writer, (double)object); break;
        case Variant::STRING: {
            CharString arr = ((String)object).utf8();
            mpack_write_str(writer, arr.ptr(), arr.size());
            break;
        }
        case Variant::PACKED_BYTE_ARRAY: {
            PackedByteArray arr = (PackedByteArray)object;
            mpack_write_bin(writer, (const char*)arr.ptr(), arr.size());
            break;
        }
        case Variant::ARRAY: {
            Array arr = (Array)object;
            mpack_build_array(writer);
            for(int i = 0; i < arr.size(); i++) {
                if(!gd_to_mpack(arr[i], writer)) {
                    mpack_complete_array(writer);
                    return false;
                }
            }
            mpack_complete_array(writer);
            break;
        }
        case Variant::DICTIONARY: {
            Dictionary dict = (Dictionary)object;
            mpack_build_map(writer);
            Array keys = dict.keys();
            for(int i = 0; i < keys.size(); i++) {
                Variant key = keys[i];
                Variant value = dict[key];
                if(!gd_to_mpack(key, writer)) {
                    mpack_complete_map(writer);
                    return false;
                }
                if(!gd_to_mpack(value, writer)) {
                    mpack_write_nil(writer);
                    mpack_complete_map(writer);
                    return false;
                }
            }
            mpack_complete_map(writer);
            break;
        }
        default: return false;
    }
    return true;
}


Variant MessagePack::loads(const PackedByteArray &arr) {
    mpack_tree_t tree;
    mpack_tree_init_data(&tree, (const char*)arr.ptr(), arr.size());
    mpack_tree_parse(&tree);

    if(mpack_tree_error(&tree) != mpack_ok) {
        mpack_tree_destroy(&tree);
        return Variant();
    }

    mpack_node_t node = mpack_tree_root(&tree);
    Variant res = mpack_to_gd(node);
    mpack_tree_destroy(&tree);
    return res;
}

static void mpack_free(void* p) {
    MPACK_FREE(p);
}

PackedByteArray MessagePack::dumps(Variant object) {
    char* data;
    size_t size;
    mpack_writer_t writer;
    mpack_writer_init_growable(&writer, &data, &size);

    bool ok = gd_to_mpack(object, &writer);
    mpack_writer_destroy(&writer);
    if(!ok) return PackedByteArray();

    PackedByteArray arr;
    arr.resize(size);
    memcpy(arr.ptrw(), data, size);

    mpack_free(data);
    return arr;
}

void MessagePack::_bind_methods() {
	ClassDB::bind_static_method("MessagePack", D_METHOD("loads", "data"), &MessagePack::loads);
	ClassDB::bind_static_method("MessagePack", D_METHOD("dumps", "object"), &MessagePack::dumps);
}

}   // namespace pkpy