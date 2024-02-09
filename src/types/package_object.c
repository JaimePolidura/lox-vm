#include "package_object.h"

void init_package_object(struct package_object * package_object) {
    package_object->package = NULL;
    package_object->object.gc_marked = false;
    package_object->object.type = OBJ_PACKAGE;
}

lox_value_t to_lox_package(struct package * package) {
    struct package_object * package_object = ALLOCATE_OBJ(struct package_object, OBJ_PACKAGE);
    package_object->package = package;

    return TO_LOX_VALUE_OBJECT(package_object);
}