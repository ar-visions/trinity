
#include <import>

double sqrt(double);
static int n_parts = 3;

// idea:
// silver should: swap = and : ... so : is const, and = is mutable-assign
// we serialize our data with : and we do not think of this as a changeable form, its our data and we want it intact, lol
// serialize object into json
string json(object a) {
    AType  type  = isa(a);
    string res   = string(alloc, 1024);
    /// start at 1024 pre-alloc
    if (!a) {
        append(res, "null");
    } else if (instanceof(a, array)) {
        // array with items
        push(res, '[');
        each (a, object, i) concat(res, json(i));
        push(res, ']');
    } else if (!(type->traits & A_TRAIT_PRIMITIVE)) {
        // object with fields
        push(res, '{');
        bool one = false;
        for (num m = 0; m < type->member_count; m++) {
            if (one) push(res, ',');
            type_member_t* mem = &type->members[m];
            if (!(mem->member_type & (A_TYPE_PROP | A_TYPE_INLAY))) continue;
            string name = string(mem->name);
            concat(res, json(name));
            push  (res, ':');
            object value = A_get_property(a, mem->name);
            concat(res, json(value));
            one = true;
        }
        push(res, '}');
    } else {
        A_serialize(type, res, a);
    }
    return res;
}

static cstr ws(cstr p) {
    cstr scan = p;
    while (*scan && isspace(*scan))
        scan++;
    return scan;
}

string parse_json_string(cstr origin, cstr* remainder) {
    if (*origin != '\"') return null;
    string res = string(alloc, 64);
    cstr scan;
    for (scan = &origin[1]; *scan;) {
        if (*scan == '\"') {
            scan++;
            break;
        }
        if (*scan == '\\') {
            scan++;
            if (*scan == 'n') push(res, 10);
            else if (*scan == 'r') push(res, 13);
            else if (*scan == 't') push(res,  9);
            else if (*scan == 'b') push(res,  8);
            else if (*scan == '/') push(res, '/');
            else if (*scan == 'u') {
                // Read the next 4 hexadecimal digits and compute the Unicode codepoint
                uint32_t code = 0;
                for (int i = 0; i < 4; i++) {
                    scan++;
                    char c = *scan;
                    if      (c >= '0' && c <= '9') code = (code << 4) | (c - '0');
                    else if (c >= 'a' && c <= 'f') code = (code << 4) | (c - 'a' + 10);
                    else if (c >= 'A' && c <= 'F') code = (code << 4) | (c - 'A' + 10);
                    else
                        fault("Invalid Unicode escape sequence");
                }
                // Convert the codepoint to UTF-8 encoded bytes
                if (code <= 0x7F) {
                    push(res, (i8)code);
                } else if (code <= 0x7FF) {
                    push(res, (i8)(0xC0 | (code >> 6)));
                    push(res, (i8)(0x80 | (code & 0x3F)));
                } else if (code <= 0xFFFF) {
                    push(res, (i8)(0xE0 | ( code >> 12)));
                    push(res, (i8)(0x80 | ((code >> 6) & 0x3F)));
                    push(res, (i8)(0x80 | ( code       & 0x3F)));
                } else if (code <= 0x10FFFF) {
                    push(res, (i8)(0xF0 | ( code >> 18)));
                    push(res, (i8)(0x80 | ((code >> 12) & 0x3F)));
                    push(res, (i8)(0x80 | ((code >> 6)  & 0x3F)));
                    push(res, (i8)(0x80 | ( code        & 0x3F)));
                } else {
                    fault("Unicode code point out of range");
                }
            }
        } else
            push(res, *scan);
        scan++;
    }
    *remainder = scan;
    return res;
}

object parse_object(cstr input, AType schema, cstr* remainder) {
    cstr   scan   = ws(input);
    cstr   origin = null;
    object res    = null;
    char  *endptr;

    if (remainder) *remainder = null;
    if ((*scan >= '0' && *scan <= '9') || *scan == '-') {
        origin = scan;
        int has_dot = 0;
        while (*++scan) {
            has_dot += *scan == '.';
            if (*scan != '.' && !(*scan >= '0' && *scan <= '9'))
                break;
        }
        if (has_dot || (schema->traits & A_TRAIT_REALISTIC)) {
            if (schema == typeid(f32))
                res = A_f32(strtof(origin, &scan));
            else
                res = A_f64(strtod(origin, &scan));
        } else
            res = A_i64(strtoll(origin, &scan, 10));
    }
    else if (*scan == '"' || *scan == '\'') {
        origin = scan;
        res = construct_with(schema, parse_json_string(origin, &scan));
    }
    else if (*scan == '{') {
        scan = ws(&scan[1]);
        map props = map(hsize, 16);
        for (;;) {
            if (*scan == '}') {
                scan++;
                break;
            }
            scan = ws(scan);
            if (*scan != '\"') return null;
            origin        = scan;
            string name   = parse_json_string(origin, &scan);
            Member member = A_member(schema, A_TYPE_PROP, name->chars);
            if (!member) {
                print("property '%o' not found in type: %s", name, schema->name);
                return null;
            }
            scan = ws(scan);
            if (*scan != ':') return null;
            scan = ws(&scan[1]);
            object value = parse_object(scan, member->type, &scan);
            float fvalue;
            memcpy(&fvalue, value, sizeof(float));
            if   (!value)
                return null;
            set(props, name, value);
            if (*scan == ',') {
                scan++;
                continue;
            } else if (*scan != '}')
                return null;
        }
        if (remainder) *remainder = scan;
        pairs (props, i) {
            print("key:%o value:%o (%s)", i->key, i->value, isa(i->value)->name);
            int test = 0;
            test += 2;
        }
        res = construct_with(schema, props);
    }
    if (remainder) *remainder = scan;
    return res;
}

object parse_array(cstr s, AType schema, cstr* remainder) {
    cstr scan = ws(s);
    verify(*scan == '[', "expected array '['");
    scan = ws(&scan[1]);
    array res = array();
    for (;;) {
        object obj = parse_object(scan, schema, &scan);
        if (!obj) break;
        push(res, obj);
    }
    if (remainder) *remainder = scan;
    return res;
}

// root will apply to objects contained within an array, or a top level object
object parse(cstr s, AType schema) {
    cstr    scan = ws(s);
    return *scan == '{' ? parse_object(scan, schema, null) :
           *scan == '[' ? parse_array (scan, schema, null) : null;
}

int main(int argc, char *argv[]) {
    A_start();

    // lets test with inlay vector data first
    symbol js   = "[{\"x\":2.0, \"y\":4.0,\"z\":\"4.0\"}]";
    print("sizeof(v3) = %i", sizeof(v3));
    array test = parse(js, typeid(v3));
    //v3     test = parse(js, typeid(v3));
    each (test, v3, i) {
        print("item = %o", i);
    }
    br();
    /// vector allocation of particles (just 256 for now)
    vertex parts = A_alloc(typeid(vertex), n_parts, true);

    // Bottom face edges
    parts[0] = (struct vertex) { .pos = {  0.0f, -0.5f,  0.5f } };
    parts[1] = (struct vertex) { .pos = { -0.5f,  0.5f,  0.5f } };
    parts[2] = (struct vertex) { .pos = {  0.5f,  0.5f,  0.5f } };

    trinity  t = trinity();
    window   w = window(t, t, width, 800, height, 800);
    shader   draw_shader = shader(t, t, vert, string("cube.vert"), frag, string("cube.frag"));
    pipeline cube = pipeline(t, t, w, w, shader, draw_shader, read, parts);
    push(w, cube);

    loop(w);
    return  0;
}