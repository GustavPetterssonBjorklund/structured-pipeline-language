#include "support/TestAssertions.hpp"

#include <spl/Value.hpp>

int main() {
    using namespace spl;
    using namespace spl::test;

    Value score{95.5};
    require(score.is(Type::Float64), "float type was not preserved");
    require(score.to_string() == "95.5", "float formatting changed");
    require(Value{true}.to_string() == "true", "bool formatting changed");
    require(Value{std::string{"hello"}}.get<std::string>() == "hello", "string access failed");
}
