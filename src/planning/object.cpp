#include "../../include/planning/object.hpp"

namespace wlplan {
  namespace planning {
    Object::Object(const std::string &object_name, const std::string &object_type)
        : object_name(object_name), object_type(object_type) {}

    Object::Object(const std::string &object_name) : object_name(object_name), object_type("object") {}

    std::string Object::to_string() const { return object_name; }
  }  // namespace planning
} // namespace wlplan