#include "package_object.h"

void init_package_object(struct package_object * package_object) {
    package_object->package = NULL;
    package_object->object.gc_marked = false;
    package_object->object.type = OBJ_PACKAGE;
}