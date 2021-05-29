// Copyright (C) 2020-2021 Sami Väisänen
// Copyright (C) 2020-2021 Ensisoft http://www.ensisoft.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "config.h"

#include <string>
#include <iostream>

#include "base/test_minimal.h"
#include "base/test_float.h"
#include "base/cmdline.h"

int test_main(int argc, char* argv[])
{
    // empty cmdline
    {
        const char* argv[] = {
            "--foo",
            "--bar"
        };

        base::CommandLineArgumentStack args(0, argv);
        base::CommandLineOptions opt;
        opt.Parse(args);
    }

    // on/off flags
    {
        const char* argv[] = {
            "--foo",
            "--bar"
        };
        base::CommandLineArgumentStack args(2, argv);
        base::CommandLineOptions opt;
        opt.Add("--foo", "foo help");
        opt.Add("--bar", "bar help");
        opt.Parse(args);
        TEST_REQUIRE(opt.WasGiven("--foo"));
        TEST_REQUIRE(opt.WasGiven("--bar"));
    }

    // on/off flags mismatched order
    {
        const char* argv[] = {
                "--bar",
                "--foo"
        };
        base::CommandLineArgumentStack args(2, argv);
        base::CommandLineOptions opt;
        opt.Add("--foo", "foo help");
        opt.Add("--bar", "bar help");
        opt.Parse(args);
        TEST_REQUIRE(opt.WasGiven("--foo"));
        TEST_REQUIRE(opt.WasGiven("--bar"));
    }


    // on/off flags
    {
        const char* argv[] = {
            "--foo",
            "--bar"
        };
        base::CommandLineArgumentStack args(2, argv);
        base::CommandLineOptions opt;
        opt.Add("--foo", "foo help");
        opt.Add("--bar", "bar help");
        opt.Add("--keke", "keke help");
        opt.Parse(args);
        TEST_REQUIRE(opt.WasGiven("--foo"));
        TEST_REQUIRE(opt.WasGiven("--bar"));
        TEST_REQUIRE(opt.WasGiven("--keke") == false);
    }

    // with value parsing when values are given.
    {
        const char* argv[] = {
            "--integer=1234",
            "--string=foobar"
        };
        base::CommandLineArgumentStack args(2, argv);
        base::CommandLineOptions opt;
        opt.Add("--integer", "integer help", 0);
        opt.Add("--string", "string help", std::string(""));
        opt.Parse(args);
        TEST_REQUIRE(opt.WasGiven("--integer"));
        TEST_REQUIRE(opt.WasGiven("--string"));
        TEST_REQUIRE(opt.GetValue<int>("--integer") == 1234);
        TEST_REQUIRE(opt.GetValue<std::string>("--string") == "foobar");
    }

    // with value parsing when values are given, mismatched order.
    {
        const char* argv[] = {
                "--string=foobar",
                "--integer=1234"
        };
        base::CommandLineArgumentStack args(2, argv);
        base::CommandLineOptions opt;
        opt.Add("--integer", "integer help", 0);
        opt.Add("--string", "string help", std::string(""));
        opt.Parse(args);
        TEST_REQUIRE(opt.WasGiven("--integer"));
        TEST_REQUIRE(opt.WasGiven("--string"));
        TEST_REQUIRE(opt.GetValue<int>("--integer") == 1234);
        TEST_REQUIRE(opt.GetValue<std::string>("--string") == "foobar");
    }

    // string with spaces
    {
        const char* argv[] = {
                "--string=jeesus ajaa mopolla"
        };
        base::CommandLineArgumentStack args(1, argv);
        base::CommandLineOptions opt;
        opt.Add("--string", "string help", std::string(""));
        opt.Parse(args);
        TEST_REQUIRE(opt.WasGiven("--string"));
        TEST_REQUIRE(opt.GetValue<std::string>("--string") == "jeesus ajaa mopolla");
    }

    // with value parsing when values are missing.
    {
        const char* argv[] = {"nada"};

        base::CommandLineArgumentStack args(0, argv);
        base::CommandLineOptions opt;
        opt.Add("--integer", "integer help", 4444);
        opt.Add("--string", "string help", std::string("default"));
        opt.Parse(args);
        TEST_REQUIRE(opt.WasGiven("--integer") == false);
        TEST_REQUIRE(opt.WasGiven("--string") == false);
        TEST_REQUIRE(opt.GetValue<int>("--integer") == 4444);
        TEST_REQUIRE(opt.GetValue<std::string>("--string") == "default");
    }

    // float value. (special since the string format is locale specific)
    {
        const float test_value = 12345.0f;
        const auto str = "--float=" + std::to_string(test_value);

        const char* argv[] = {
            str.c_str()
        };
        base::CommandLineArgumentStack args(1, argv);
        base::CommandLineOptions opt;
        opt.Add("--float", "float help", 0.0f);
        opt.Parse(args);
        TEST_REQUIRE(opt.WasGiven("--float"));
        TEST_REQUIRE(real::equals(opt.GetValue<float>("--float"), test_value));
    }

    // float value. (special since the string format is locale specific)
    {
        const float test_value = 12345.0f;
        const char* argv[] = {"nada"};

        base::CommandLineArgumentStack args(0, argv);
        base::CommandLineOptions opt;
        opt.Add("--float", "float help", test_value);
        opt.Parse(args);
        TEST_REQUIRE(opt.WasGiven("--float") == false);
        TEST_REQUIRE(real::equals(opt.GetValue<float>("--float"), test_value));
    }

    // test parsing/matching errors.

    // unrecognized argument
    {
        const char* argv[] = {"--whatever"};
        base::CommandLineArgumentStack args(1, argv);
        base::CommandLineOptions opt;
        std::string error;
        TEST_REQUIRE(opt.Parse(args, &error) == false);
        std::cout << error << "\n";
    }

    // missing value
    {
        const char* argv[] = {
            "--integer=",
        };
        base::CommandLineArgumentStack args(1, argv);
        base::CommandLineOptions opt;
        opt.Add("--integer", "help", 1234);
        std::string error;
        TEST_REQUIRE(opt.Parse(args, &error) == false);
        std::cout << error << "\n";
    }

    // unpexpected format error
    {
        const char* argv[] = {
            "--integer=asgbasbdfas",
        };
        base::CommandLineArgumentStack args(1, argv);
        base::CommandLineOptions opt;
        opt.Add("--integer", "help", 1234);
        std::string error;
        TEST_REQUIRE(opt.Parse(args, &error) == false);
        std::cout << error << "\n";
    }

    // print
    {
        base::CommandLineOptions opt;
        opt.Add("--foo", "Foo help bla bla bla", 123);
        opt.Add("--bar", "Bar help");
        opt.Print(std::cout);
    }

    return 0;
}