#include <iostream>
#include <cassert>

#include "rencpp/ren.hpp"

using namespace ren;

#include "catch.hpp"

TEST_CASE("block iteration", "[rebol]")
{
    Block blk {"1 2 3"};

    auto it = blk.begin();
    CHECK(it->isEqualTo(1));
    CHECK(it == blk.begin());
    CHECK(it != blk.end());

    it++;
    CHECK((*it).isEqualTo(2));
    CHECK(it != blk.begin());
    CHECK(it != blk.end());

    it++;
    CHECK(it->isEqualTo(3));
    CHECK(it != blk.begin());
    CHECK(it != blk.end());

    it++;
    CHECK(it != blk.begin());
    CHECK(it == blk.end());
}


TEST_CASE("ascii string iteration", "[rebol]")
{
    const char * renCstr = "Hello^/There\nWorld^/";
    const char * cppCstr = "Hello\nThere\nWorld\n";

    std::string s;
    for (auto c : String {renCstr})
        s.push_back(static_cast<char>(c));

    int index = 0;
    for (auto c : s) {
        CHECK(cppCstr[index] == c);
        index++;
    }
}


TEST_CASE("unicode string iteration", "[rebol]")
{
    const char * utf8Cstr = "MetÆducation\n";

    std::wstring ws;
    for (auto wc : String{utf8Cstr})
        ws.push_back(static_cast<wchar_t>(wc));

    // TBD: REQUIRE correct result beyond "compiles, doesn't crash"
}
