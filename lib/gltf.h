#ifndef _TRINITY_GLTF_
#define _TRINITY_GLTF_

#define ComponentType_schema(X,Y) \
    i_enum_value_v(X,Y, NONE,           0) \
    i_enum_value_v(X,Y, BYTE,           5120) \
    i_enum_value_v(X,Y, UNSIGNED_BYTE,  5121) \
    i_enum_value_v(X,Y, SHORT,          5122) \
    i_enum_value_v(X,Y, UNSIGNED_SHORT, 5123) \
    i_enum_value_v(X,Y, UNSIGNED_INT,   5125) \
    i_enum_value_v(X,Y, FLOAT,          5126)
#endif
declare_enum(ComponentType)


#define CompoundType_schema(X,Y) \
    i_enum_value_v(X,Y, NONE,           0) \
    i_enum_value_v(X,Y, SCALAR,         1) \
    i_enum_value_v(X,Y, VEC2,           2) \
    i_enum_value_v(X,Y, VEC3,           3) \
    i_enum_value_v(X,Y, VEC4,           4) \
    i_enum_value_v(X,Y, MAT2,           5) \
    i_enum_value_v(X,Y, MAT3,           6) \
    i_enum_value_v(X,Y, MAT4,           7) \
declare_enum(CompoundType)


#define TargetType_schema(X,Y) \
    i_enum_value_v(X,Y, NONE,           0) \
    i_enum_value_v(X,Y, ARRAY_BUFFER,   34962) \
    i_enum_value_v(X,Y, ELEMENT_BUFFER, 34963)
declare_enum(TargetType)


#define Mode_schema(X,Y) \
    i_enum_value_v(X,Y, NONE,           0) \
    i_enum_value_v(X,Y, LINES,          1) \
    i_enum_value_v(X,Y, LINE_LOOP,      2) \
    i_enum_value_v(X,Y, TRIANGLES,      4) \
    i_enum_value_v(X,Y, TRIANGLE_STRIP, 5) \
    i_enum_value_v(X,Y, TRIANGLE_FAN,   6) \
    i_enum_value_v(X,Y, QUADS,          7)
declare_enum(Mode)


#define Interpolation_schema(X,Y) \
    i_enum_value_v(X,Y, LINEAR,         0) \
    i_enum_value_v(X,Y, STEP,           1) \
    i_enum_value_v(X,Y, CUBICSPLINE,    2)
declare_enum(Interpolation)


#define Sampler_schema(X,Y) \
    i_prop(X,Y, public, u64,            input) \
    i_prop(X,Y, public, u64,            output) \
    i_prop(X,Y, public, Interpolation,  interpolation)
#ifndef Sampler_intern
#define Sampler_intern
#endif
declare_class(Sampler)


#define ChannelTarget_schema(X,Y) \
    i_prop(X,Y, public, u64,            node) \
    i_prop(X,Y, public, string,         path)
#ifndef ChannelTarget_intern
#define ChannelTarget_intern
#endif
declare_class(ChannelTarget)


#define Channel_schema(X,Y) \
    i_prop(X,Y, public, u64,            sampler) \
    i_prop(X,Y, public, ChannelTarget,  target)
#ifndef Channel_intern
#define Channel_intern
#endif
declare_class(Channel)


#define Animation_schema(X,Y) \
    i_prop(X,Y, public, string,         name) \
    i_prop(X,Y, public, array_Sampler,  samplers)
#ifndef Animation_intern
#define Animation_intern
#endif
declare_class(Animation)


#define SparseInfo_schema(X, Y) \
    i_prop(X, Y, public, u64,           bufferView) \
    i_prop(X, Y, public, ComponentType, componentType) \
    i_method(X, Y, public, properties,  meta)
#ifndef SparseInfo_intern
#define SparseInfo_intern
#endif
declare_class(SparseInfo)


#define Sparse_schema(X, Y) \
    i_prop(X, Y, public, u64, count) \
    i_prop(X, Y, public, SparseInfo,    indices) \
    i_prop(X, Y, public, SparseInfo,    values) \
    i_method(X, Y, public, properties,  meta)
#ifndef Sparse_intern
#define Sparse_intern
#endif
declare_class(Sparse)


#define Accessor_schema(X, Y) \
    i_prop(X, Y, public, u64,           bufferView) \
    i_prop(X, Y, public, ComponentType, componentType) \
    i_prop(X, Y, public, CompoundType,  type) \
    i_prop(X, Y, public, u64,           count) \
    i_prop(X, Y, public, v3,            min) \
    i_prop(X, Y, public, v3,            max) \
    i_prop(X, Y, public, Sparse,        sparse) \
    i_method(X, Y, public, u64,         vcount) \
    i_method(X, Y, public, u64,         component_size) \
    i_method(X, Y, public, properties,  meta)
#ifndef Accessor_intern
#define Accessor_intern
#endif
declare_class(Accessor)


#define BufferView_schema(X, Y) \
    i_prop(X, Y, public, u64, buffer) \
    i_prop(X, Y, public, u64, byteLength) \
    i_prop(X, Y, public, u64, byteOffset) \
    i_prop(X, Y, public, TargetType, target) \
    i_method(X, Y, public, properties, meta)
#ifndef BufferView_intern
#define BufferView_intern
#endif
declare_class(BufferView)


#define Skin_schema(X, Y) \
    i_prop(X, Y, public, string, name) \
    i_prop(X, Y, public, Array_int, joints) \
    i_prop(X, Y, public, i32, inverseBindMatrices) \
    i_prop(X, Y, public, mx, extras) \
    i_prop(X, Y, public, mx, extensions) \
    i_method(X, Y, public, properties, meta)
#ifndef Skin_intern
#define Skin_intern
#endif
declare_class(Skin)


#define Transform_schema(X, Y) \
    i_prop(X, Y, public, Joint,             jdata) \
    i_prop(X, Y, public, i32,               istate) \
    i_prop(X, Y, public, m44f,              local) \
    i_prop(X, Y, public, m44f,              local_default) \
    i_prop(X, Y, public, i32,               iparent) \
    i_prop(X, Y, public, Array_int,         ichildren) \
    i_method(X, Y, public, none, multiply,  const m44f&) \
    i_method(X, Y, public, none, set,       const m44f&) \
    i_method(X, Y, public, none, set_default) \
    i_method(X, Y, public, none, propagate)
#ifndef Transform_intern
#define Transform_intern
#endif
declare_class(Transform)


#define Node_schema(X, Y) \
    i_prop(X, Y, public, string,            name) \
    i_prop(X, Y, public, i32,               skin) \
    i_prop(X, Y, public, i32,               mesh) \
    i_prop(X, Y, public, Array_float,       translation) \
    i_prop(X, Y, public, Array_float,       rotation) \
    i_prop(X, Y, public, Array_float,       scale) \
    i_prop(X, Y, public, Array_float,       weights) \
    i_prop(X, Y, public, Array_int,         children) \
    i_prop(X, Y, public, i32,               joint_index) \
    i_prop(X, Y, public, bool,              processed) \
    i_prop(X, Y, public, object,            mx_joints)
#ifndef Node_intern
#define Node_intern
#endif
declare_class(Node)


#define Primitive_schema(X, Y) \
    i_prop(X, Y, public, map,               attributes) \
    i_prop(X, Y, public, u64,               indices) \
    i_prop(X, Y, public, i32,               material) \
    i_prop(X, Y, public, Mode,              mode) \
    i_prop(X, Y, public, Array_map,         targets)
#ifndef Primitive_intern
#define Primitive_intern
#endif
declare_class(Primitive)


#define Mesh_schema(X, Y) \
    i_prop(X, Y, public, string,            name) \
    i_prop(X, Y, public, Array_Primitive,   primitives) \
    i_prop(X, Y, public, Array_float,       weights) \
    i_prop(X, Y, public, MeshExtras,        extras)
#ifndef Mesh_intern
#define Mesh_intern
#endif
declare_class(Mesh)


#define Scene_schema(X, Y) \
    i_prop(X, Y, public, string,            name) \
    i_prop(X, Y, public, Array_u64,         nodes)
#ifndef Scene_intern
#define Scene_intern
#endif
declare_class(Scene)


#define AssetDesc_schema(X, Y) \
    i_prop(X, Y, public, string,            generator) \
    i_prop(X, Y, public, string,            copyright) \
    i_prop(X, Y, public, string,            version)
#ifndef AssetDesc_intern
#define AssetDesc_intern
#endif
declare_class(AssetDesc)


#define Buffer_schema(X, Y) \
    i_prop(X, Y, public, u64,               byteLength) \
    i_prop(X, Y, public, object,            uri)
#ifndef Buffer_intern
#define Buffer_intern
#endif
declare_class(Buffer)


#define Model_schema(X, Y) \
    i_prop  (X, Y, public, Array_Node,      nodes) \
    i_prop  (X, Y, public, Array_Skin,      skins) \
    i_prop  (X, Y, public, Array_Accessor,  accessors) \
    i_prop  (X, Y, public, Array_BufferView, bufferViews) \
    i_prop  (X, Y, public, Array_Mesh,      meshes) \
    i_prop  (X, Y, public, u64,             scene) \
    i_prop  (X, Y, public, Array_Scene,     scenes) \
    i_prop  (X, Y, public, AssetDesc,       asset) \
    i_prop  (X, Y, public, Array_Buffer,    buffers) \
    i_prop  (X, Y, public, Array_Animation, animations) \
    i_method(X, Y, public, properties,      meta) \
    i_method(X, Y, public, Node,            find, string) \
    i_method(X, Y, public, Node,            parent, Node) \
    i_method(X, Y, public, i32,             index_of, string) \
    i_index (X, Y, public, Node,            string) \
    i_method(X, Y, public, Joints,          joints, Node) \
    i_ctr(X, Y, public, path)
#ifndef Model_intern
#define Model_intern
#endif
declare_class(Model)

/// type-safe container declarations; needed for proper json serialization
/// the Model is given in the definition, in .c file
/// this merely forms a type alias with an additional type_t slot used at meta[0]
/// meta types are merely types with an array of types filled at index
declare_meta(array_Sampler,    array)
declare_meta(array_Channel,    array)
declare_meta(array_map,        array)
declare_meta(array_Primitive,  array)
declare_meta(array_Node,       array)
declare_meta(array_Skin,       array)
declare_meta(array_Accessor,   array)
declare_meta(array_BufferView, array)
declare_meta(array_Mesh,       array)
declare_meta(array_Scene,      array)
declare_meta(array_Buffer,     array)
declare_meta(array_Animation,  array)

