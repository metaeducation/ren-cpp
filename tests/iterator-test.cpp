#include <iostream>
#include <cassert>

#include "rencpp/ren.hpp"

using namespace ren;

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("block iteration", "[block]")
{
    Block blk {"1 2 3"};

    auto it = blk.begin();
    REQUIRE(it->isEqualTo(1));
    REQUIRE(it == blk.begin());
    REQUIRE(it != blk.end());

    it++;
    REQUIRE((*it).isEqualTo(2));
    REQUIRE(it != blk.begin());
    REQUIRE(it != blk.end());

    it++;
    REQUIRE(it->isEqualTo(3));
    REQUIRE(it != blk.begin());
    REQUIRE(it != blk.end());

    it++;
    REQUIRE(it != blk.begin());
    REQUIRE(it == blk.end());
}


TEST_CASE("ascii string iteration", "[iterator] [string]")
{
    const char * renCstr = "Hello^/There\nWorld^/";
    const char * cppCstr = "Hello\nThere\nWorld\n";

    std::string s;
    for (auto c : String {renCstr})
        s.push_back(static_cast<char>(c));

    int index = 0;
    for (auto c : s) {
        REQUIRE(cppCstr[index] == c);
        index++;
    }
}


TEST_CASE("unicode string iteration", "[iterator] [string]")
{
    const char * utf8Cstr = "MetÆducation\n";

    std::wstring ws;
    for (auto wc : String{utf8Cstr})
        ws.push_back(static_cast<wchar_t>(wc));

    // TBD: REQUIRE correct result beyond "compiles, doesn't crash"
}
