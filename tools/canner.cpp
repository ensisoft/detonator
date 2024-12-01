// Copyright (C) 2020-2024 Sami Väisänen
// Copyright (C) 2020-2024 Ensisoft http://www.ensisoft.com
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

// g++ -o canner -O3 canner.cpp

#include <cstdio>
#include <fstream>
#include <vector>
#include <algorithm>

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        printf("Usage: canner 'src file' 'dst file'\n"
               "For example: ./canner foobar.png foobar.h\n");
        return 1;
    }

    std::ifstream in(argv[1]);
    std::ofstream out(argv[2]);

    typedef unsigned char u8;

    std::vector<u8> buff;
    std::copy(std::istreambuf_iterator<char>(in),
       std::istreambuf_iterator<char>(),std::back_inserter(buff));

    //out << "\n\n" << "const unsigned char array [] = {\n";

    for (size_t i=0; i<buff.size(); ++i)
    {
        out << "0x" << std::hex << (int)(unsigned char)buff[i];
        if (i + 1 < buff.size())
            out << ",";
        if (!(i % 40) && i)
            out << "\n";
    }

    //out << "};\n\n";
    return 0;
}
