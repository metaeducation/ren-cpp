#include <iostream>

#include "rencpp/ren.hpp"

using namespace ren;

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("block test", "[block]")
{
    SECTION("empty")
    {
        Block block {};
        REQUIRE(block.length() == 0);
    }

    SECTION("singleton") {
        // This is a tricky case

        Block singleton {"foo"};
        Value singletonAsValue = singleton;

        static_cast<Block>(singletonAsValue);

        Block singletonInitializer {singleton};
    }


    Block threeEmpties {Block {}, Block {}, Block {}};

    Block randomStuff {"blue", Block {true, 1020}, 3.04};


    SECTION("nested")
    {
        // See discussion on:
        //
        //     https://github.com/hostilefork/rencpp/issues/1

        Block blk { {1, true}, {false, 2} };

        REQUIRE(blk.isBlock());
        REQUIRE(blk[1].isBlock());
        REQUIRE(blk[2].isBlock());

        Block blk1 = static_cast<Block>(blk[1]);
        Block blk2 = static_cast<Block>(blk[2]);
        REQUIRE(blk1[1].isInteger());
        REQUIRE(blk1[2].isLogic());
        REQUIRE(blk2[1].isLogic());
        REQUIRE(blk2[2].isInteger());
    }
}
