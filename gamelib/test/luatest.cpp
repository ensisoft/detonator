
#include "config.h"

#include "warnpush.h"
#  define SOL_ALL_SAFETIES_ON 1
#  include <sol/sol.hpp>
#  include <glm/glm.hpp>
#include "warnpop.h"

#include <string>
#include <unordered_map>
#include <chrono>
#include <thread>


// type provided by the engine
class Entity
{
public:
    Entity(std::string type, std::string name)
      : mType(type)
      , mName(name)
    {}

    glm::vec2 GetPosition() const
    { return mPos; }
    void SetPosition(const glm::vec2& pos)
    { mPos = pos; }
    std::string GetName() const
    { return mName; }
private:
    std::string mType;
    std::string mName;
    glm::vec2 mPos;
};

class Scene
{
public:
    Scene()
    {
        mEntities.emplace_back("tank", "tank 1");
        mEntities.emplace_back("tank", "tank 2");
        mEntities.emplace_back("tank", "tank 3");
        mMap["foo"] = &mEntities[0];
        mMap["bar"] = &mEntities[1];
        mMap["meh"] = &mEntities[2];
    }
    Entity* GetEntity(const std::string& id)
    {
        return mMap[id];
    }
private:
    std::vector<Entity> mEntities;
    std::unordered_map<std::string, Entity*> mMap;
};


class Foobar
{
public:
    int GetValue(int index) const
    {
        std::cout << "Foobar::GetValue " << index << std::endl;
        return 1234;
    }
};

class Doodah
{
public:
    int GetValue(int index) const
    {
        std::cout << "Doodah::GetValue " << index << std::endl;
        return 1234;
    }
};

int main(int argc, char* argv[])
{
    sol::state L;
    L.open_libraries();

    // the index only works when set in the constructor?
    auto foo = L.new_usertype<Foobar>("foo",
		sol::meta_function::index, &Foobar::GetValue);
    auto bar = L.new_usertype<Doodah>("bar");
    bar.set_function(sol::meta_function::index, &Foobar::GetValue);

	L.script(
            "f = foo.new()\n"
            "b = bar.new()\n"
		"print(f[1])\n"
		"print(b[1])\n"
	);
    return 0;
}