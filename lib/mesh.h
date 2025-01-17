#ifndef _TRINITY_MESH_H_
#define _TRINITY_MESH_H_

#define Key_schema(X, YY) \
    i_enum_value_v(X, YY, Undefined,      0) \
    i_enum_value_v(X, YY, Space,         32) \
    i_enum_value_v(X, YY, Apostrophe,    39) \
    i_enum_value_v(X, YY, Comma,         44) \
    i_enum_value_v(X, YY, Minus,         45) \
    i_enum_value_v(X, YY, Period,        46) \
    i_enum_value_v(X, YY, Slash,         47) \
    i_enum_value_v(X, YY, K0,            48) \
    i_enum_value_v(X, YY, K1,            49) \
    i_enum_value_v(X, YY, K2,            50) \
    i_enum_value_v(X, YY, K3,            51) \
    i_enum_value_v(X, YY, K4,            52) \
    i_enum_value_v(X, YY, K5,            53) \
    i_enum_value_v(X, YY, K6,            54) \
    i_enum_value_v(X, YY, K7,            55) \
    i_enum_value_v(X, YY, K8,            56) \
    i_enum_value_v(X, YY, K9,            57) \
    i_enum_value_v(X, YY, SemiColon,     59) \
    i_enum_value_v(X, YY, Equal,         61) \
    i_enum_value_v(X, YY, A,             65) \
    i_enum_value_v(X, YY, B,             66) \
    i_enum_value_v(X, YY, C,             67) \
    i_enum_value_v(X, YY, D,             68) \
    i_enum_value_v(X, YY, E,             69) \
    i_enum_value_v(X, YY, F,             70) \
    i_enum_value_v(X, YY, G,             71) \
    i_enum_value_v(X, YY, H,             72) \
    i_enum_value_v(X, YY, I,             73) \
    i_enum_value_v(X, YY, J,             74) \
    i_enum_value_v(X, YY, K,             75) \
    i_enum_value_v(X, YY, L,             76) \
    i_enum_value_v(X, YY, M,             77) \
    i_enum_value_v(X, YY, N,             78) \
    i_enum_value_v(X, YY, O,             79) \
    i_enum_value_v(X, YY, P,             80) \
    i_enum_value_v(X, YY, Q,             81) \
    i_enum_value_v(X, YY, R,             82) \
    i_enum_value_v(X, YY, S,             83) \
    i_enum_value_v(X, YY, T,             84) \
    i_enum_value_v(X, YY, U,             85) \
    i_enum_value_v(X, YY, V,             86) \
    i_enum_value_v(X, YY, W,             87) \
    i_enum_value_v(X, YY, X,             88) \
    i_enum_value_v(X, YY, Y,             89) \
    i_enum_value_v(X, YY, Z,             90) \
    i_enum_value_v(X, YY, LeftBracket,   91) \
    i_enum_value_v(X, YY, BackSlash,     92) \
    i_enum_value_v(X, YY, RightBracket,  93) \
    i_enum_value_v(X, YY, GraveAccent,   96) \
    i_enum_value_v(X, YY, World1,       161) \
    i_enum_value_v(X, YY, World2,       162) \
    i_enum_value_v(X, YY, Escape,       256) \
    i_enum_value_v(X, YY, Enter,        257) \
    i_enum_value_v(X, YY, Tab,          258) \
    i_enum_value_v(X, YY, Backspace,    259) \
    i_enum_value_v(X, YY, Insert,       260) \
    i_enum_value_v(X, YY, Delete,       261) \
    i_enum_value_v(X, YY, Right,        262) \
    i_enum_value_v(X, YY, Left,         263) \
    i_enum_value_v(X, YY, Down,         264) \
    i_enum_value_v(X, YY, Up,           265) \
    i_enum_value_v(X, YY, PageUp,       266) \
    i_enum_value_v(X, YY, PageDown,     267) \
    i_enum_value_v(X, YY, Home,         268) \
    i_enum_value_v(X, YY, End,          269) \
    i_enum_value_v(X, YY, CapsLock,     280) \
    i_enum_value_v(X, YY, ScrollLock,   281) \
    i_enum_value_v(X, YY, NumLock,      282) \
    i_enum_value_v(X, YY, PrintScreen,  283) \
    i_enum_value_v(X, YY, Pause,        284) \
    i_enum_value_v(X, YY, F1,           290) \
    i_enum_value_v(X, YY, F2,           291) \
    i_enum_value_v(X, YY, F3,           292) \
    i_enum_value_v(X, YY, F4,           293) \
    i_enum_value_v(X, YY, F5,           294) \
    i_enum_value_v(X, YY, F6,           295) \
    i_enum_value_v(X, YY, F7,           296) \
    i_enum_value_v(X, YY, F8,           297) \
    i_enum_value_v(X, YY, F9,           298) \
    i_enum_value_v(X, YY, F10,          299) \
    i_enum_value_v(X, YY, F11,          300) \
    i_enum_value_v(X, YY, F12,          301) \
    i_enum_value_v(X, YY, F13,          302) \
    i_enum_value_v(X, YY, F14,          303) \
    i_enum_value_v(X, YY, F15,          304) \
    i_enum_value_v(X, YY, F16,          305) \
    i_enum_value_v(X, YY, F17,          306) \
    i_enum_value_v(X, YY, F18,          307) \
    i_enum_value_v(X, YY, F19,          308) \
    i_enum_value_v(X, YY, F20,          309) \
    i_enum_value_v(X, YY, F21,          310) \
    i_enum_value_v(X, YY, F22,          311) \
    i_enum_value_v(X, YY, F23,          312) \
    i_enum_value_v(X, YY, F24,          313) \
    i_enum_value_v(X, YY, F25,          314) \
    i_enum_value_v(X, YY, Kp0,          320) \
    i_enum_value_v(X, YY, Kp1,          321) \
    i_enum_value_v(X, YY, Kp2,          322) \
    i_enum_value_v(X, YY, Kp3,          323) \
    i_enum_value_v(X, YY, Kp4,          324) \
    i_enum_value_v(X, YY, Kp5,          325) \
    i_enum_value_v(X, YY, Kp6,          326) \
    i_enum_value_v(X, YY, Kp7,          327) \
    i_enum_value_v(X, YY, Kp8,          328) \
    i_enum_value_v(X, YY, Kp9,          329) \
    i_enum_value_v(X, YY, KpDecimal,    330) \
    i_enum_value_v(X, YY, KpDivide,     331) \
    i_enum_value_v(X, YY, KpMultiply,   332) \
    i_enum_value_v(X, YY, KpSubtract,   333) \
    i_enum_value_v(X, YY, KpAdd,        334) \
    i_enum_value_v(X, YY, KpEnter,      335) \
    i_enum_value_v(X, YY, KpEqual,      336) \
    i_enum_value_v(X, YY, LeftShift,    340) \
    i_enum_value_v(X, YY, LeftControl,  341) \
    i_enum_value_v(X, YY, LeftAlt,      342) \
    i_enum_value_v(X, YY, LeftSuper,    343) \
    i_enum_value_v(X, YY, RightShift,   344) \
    i_enum_value_v(X, YY, RightControl, 345) \
    i_enum_value_v(X, YY, RightAlt,     346) \
    i_enum_value_v(X, YY, RightSuper,   347) \
    i_enum_value_v(X, YY, Menu,         348)
declare_enum(Key)



#define Polygon_schema(X, Y) \
    i_enum_value_v(X, Y, undefined, 0) \
    i_enum_value_v(X, Y, tri, 1) \
    i_enum_value_v(X, Y, quad, 2) \
    i_enum_value_v(X, Y, wire, 3) \
    i_enum_value_v(X, Y, mixed, 4) \
    i_enum_value_v(X, Y, ngon, 5)
declare_enum(Polygon)


#define Asset_schema(X, Y) \
    i_enum_value_v(X, Y, undefined, 0) \
    i_enum_value_v(X, Y, color, 1) \
    i_enum_value_v(X, Y, normal, 2) \
    i_enum_value_v(X, Y, material, 3) \
    i_enum_value_v(X, Y, reflect, 4) \
    i_enum_value_v(X, Y, env, 5) \
    i_enum_value_v(X, Y, attachment, 6) \
    i_enum_value_v(X, Y, depth_stencil, 7) \
    i_enum_value_v(X, Y, multisample, 8)
declare_enum(Asset)


#define Sampling_schema(X, Y) \
    i_enum_value_v(X, Y, undefined, 0) \
    i_enum_value_v(X, Y, nearest, 1) \
    i_enum_value_v(X, Y, linear, 2) \
    i_enum_value_v(X, Y, ansio, 3)
declare_enum(Sampling)



#define HumanVertex_schema(X, Y) \
    i_prop(X, Y, public, v3, pos)          \
    i_prop(X, Y, public, v3, normal)       \
    i_prop(X, Y, public, v2, uv0)           \
    i_prop(X, Y, public, v2, uv1)           \
    i_prop(X, Y, public, v4, tangent)     \
    i_prop(X, Y, public, v4, joints0)     \
    i_prop(X, Y, public, v4, joints1)     \
    i_prop(X, Y, public, v4, weights0)    \
    i_prop(X, Y, public, v4, weights1)
declare_struct(HumanVertex)