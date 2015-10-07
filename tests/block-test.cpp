#include <iostream>

#include "rencpp/ren.hpp"

using namespace ren;

#include "catch.hpp"

TEST_CASE("block test", "[rebol] [block]")
{
    SECTION("empty")
    {
        Block block {};
        CHECK(block.length() == 0);

        Block explicitEmpties {Block {}, Block {}, Block {}};
        Block implicitEmpties {{}, {}, {}};

        CHECK(explicitEmpties.isEqualTo(implicitEmpties));
    }

    SECTION("singleton") {
        // This is a tricky case

        Block singleton {"foo"};
        AnyValue singletonAsValue = singleton;

        static_cast<Block>(singletonAsValue);

        Block singletonInitializer {singleton};
    }


    Block randomStuff {"blue", Block {true, 1020}, 3.04};


    SECTION("nested")
    {
        // See discussion on:
        //
        //     https://github.com/hostilefork/rencpp/issues/1

        Block blk { {1, true}, {false, 2} };

        CHECK(is<Block>(blk));
        CHECK(is<Block>(blk[1]));
        CHECK(is<Block>(blk[2]));

        Block blk1 = static_cast<Block>(blk[1]);
        Block blk2 = static_cast<Block>(blk[2]);
        CHECK(is<Integer>(blk1[1]));
        CHECK(is<Logic>(blk1[2]));
        CHECK(is<Logic>(blk2[1]));
        CHECK(is<Integer>(blk2[2]));
    }
}
