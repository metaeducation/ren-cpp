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

        CHECK(hasType<Block>(blk));
        CHECK(hasType<Block>(blk[1]));
        CHECK(hasType<Block>(blk[2]));

        Block blk1 = static_cast<Block>(blk[1]);
        Block blk2 = static_cast<Block>(blk[2]);
        CHECK(hasType<Integer>(blk1[1]));
        CHECK(hasType<Logic>(blk1[2]));
        CHECK(hasType<Logic>(blk2[1]));
        CHECK(hasType<Integer>(blk2[2]));
    }
}
