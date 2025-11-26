#ifndef PLANNING_OBJECT_HPP
#define PLANNING_OBJECT_HPP

#include <string>

namespace wlplan {
  namespace planning {
class Object {
   public:
    std::string object_name;
    std::string object_type;

    Object(const std::string &object_name, const std::string &object_type);
    Object(const std::string &object_name);

    std::string to_string() const;
    std::string get_type() const {return object_type; };
    std::string get_name() const {return object_name; };

    bool operator==(const Object &other) const { return to_string() == other.to_string(); }
    bool operator< (const Object &other) const { return to_string() < other.to_string(); }
    bool operator> (const Object &other) const { return to_string() > other.to_string(); }
    std::size_t hash() const { return std::hash<std::string>()(to_string()); }
  };

}  // namespace planning
}  // namespace wlplan

// https://stackoverflow.com/questions/17016175/c-unordered-map-using-a-custom-class-type-as-the-key
template <>
struct std::hash<wlplan::planning::Object>
{
  std::size_t operator()(const wlplan::planning::Object& k) const
  {
    return std::hash<std::string>()(k.to_string());
  }
};

#endif  // PLANNING_OBJECT_HPP
