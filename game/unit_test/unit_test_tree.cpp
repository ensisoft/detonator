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
#include <cstddef>
#include <iostream>

#include "base/test_minimal.h"
#include "data/json.h"
#include "game/tree.h"
#include "game/treeop.h"

struct MyNode {
    std::string s;
    unsigned i = 0;
    MyNode(const std::string& s, unsigned i)
      : s(s)
      , i(i)
    {}
    MyNode() = default;

    static void TreeNodeToJson(data::Writer& data, const MyNode* node)
    {
        if (node)
        {
            data.Write("s", node->s);
            data.Write("i", node->i);
        }
    }
    static MyNode* TreeNodeFromJson(const data::Reader& data)
    {
        static std::vector<std::unique_ptr<MyNode>> nodes;
        if (data.IsEmpty())
            return nullptr;

        auto ret = std::make_unique<MyNode>();
        data.Read("s", &ret->s);
        data.Read("i", &ret->i);
        nodes.push_back(std::move(ret));
        return nodes.back().get();
    }
};

void TreeNodeToJson(data::Writer& data, const MyNode* node)
{
    data.Write("s", node->s);
    data.Write("i", node->i);
}

std::string WalkTree(game::RenderTree<MyNode>& tree)
{
    std::string names;
    tree.PreOrderTraverseForEach([&names](const auto* node) {
        if (node) {
            names.append(node->s);
            names.append(" ");
        }
    });
    if (!names.empty())
        names.pop_back();
    return names;
}
void unit_test_tree()
{
    // test link child
    {
        using MyTree = game::RenderTree<MyNode>;
        MyNode foo("foo", 123);
        MyNode bar("bar", 222);
        MyNode child0("child 0", 1);
        MyNode child1("child 1", 2);
        MyNode child2("child 2", 3);
        MyNode child3("child 3", 3);

        MyTree tree;
        tree.LinkChild(nullptr, &foo);
        tree.LinkChild(nullptr, &bar);
        tree.LinkChild(&foo, &child0);
        tree.LinkChild(&foo, &child1);
        tree.LinkChild(&bar, &child2);
        tree.LinkChild(&bar, &child3);
        TEST_REQUIRE(tree.GetParent(&foo) == nullptr);
        TEST_REQUIRE(tree.GetParent(&bar) == nullptr);
        TEST_REQUIRE(tree.GetParent(&child0) == &foo);
        TEST_REQUIRE(tree.GetParent(&child2) == &bar);
        TEST_REQUIRE(WalkTree(tree) == "foo child 0 child 1 bar child 2 child 3");
    }

    // test traversal.
    {
        using MyTree = game::RenderTree<MyNode>;
        MyNode foo("foo", 123);
        MyNode bar("bar", 222);
        MyNode child0("child 0", 1);
        MyNode child1("child 1", 2);
        MyNode child2("child 2", 3);
        MyNode child3("child 3", 3);

        MyTree tree;
        tree.LinkChild(nullptr, &foo);
        tree.LinkChild(nullptr, &bar);
        tree.LinkChild(&foo, &child0);
        tree.LinkChild(&foo, &child1);
        tree.LinkChild(&bar, &child2);
        tree.LinkChild(&bar, &child3);

        class Visitor : public MyTree::Visitor {
        public:
            virtual void EnterNode(MyNode* node) override
            {
                if (node)
                {
                    names.append(node->s);
                    names.append(" ");
                }
            }
            std::string names;
        private:
        };
        Visitor visitor;
        tree.PreOrderTraverse(visitor);
        TEST_REQUIRE(visitor.names == "foo child 0 child 1 bar child 2 child 3 ");
        visitor.names.clear();
        tree.PreOrderTraverse(visitor, &bar);
        TEST_REQUIRE(visitor.names == "bar child 2 child 3 ");

        class ConstVisitor : public MyTree::ConstVisitor {
        public:
            virtual void EnterNode(const MyNode* node) override
            {
                if (node)
                {
                    names.append(node->s);
                    names.append(" ");
                }
            }
            std::string names;
        private:
        };
        ConstVisitor const_visitor;
        tree.PreOrderTraverse(const_visitor);
        TEST_REQUIRE(const_visitor.names == "foo child 0 child 1 bar child 2 child 3 ");
    }

    // test reparenting
    {
        using MyTree = game::RenderTree<MyNode>;
        MyNode foo("foo", 123);
        MyNode bar("bar", 222);
        MyNode child0("child 0", 1);
        MyNode child1("child 1", 2);
        MyNode child2("child 2", 3);
        MyNode child3("child 3", 3);

        MyTree tree;
        tree.LinkChild(nullptr, &foo);
        tree.LinkChild(nullptr, &bar);
        tree.LinkChild(&foo, &child0);
        tree.LinkChild(&foo, &child1);
        tree.LinkChild(&bar, &child2);
        tree.LinkChild(&bar, &child3);

        tree.ReparentChild(&foo, &bar);
        TEST_REQUIRE(tree.GetParent(&bar) == &foo);
        TEST_REQUIRE(WalkTree(tree) == "foo child 0 child 1 bar child 2 child 3");
        tree.ReparentChild(nullptr, &bar);
        TEST_REQUIRE(tree.GetParent(&bar) == nullptr);
        TEST_REQUIRE(WalkTree(tree) == "foo child 0 child 1 bar child 2 child 3");
        tree.ReparentChild(&bar, &foo);
        TEST_REQUIRE(WalkTree(tree) == "bar child 2 child 3 foo child 0 child 1");
        tree.ReparentChild(nullptr, &foo);
        TEST_REQUIRE(WalkTree(tree) == "bar child 2 child 3 foo child 0 child 1");

        tree.ReparentChild(&child0, &child2);
        TEST_REQUIRE(WalkTree(tree) == "bar child 3 foo child 0 child 2 child 1");
        tree.ReparentChild(&bar, &child2);
        TEST_REQUIRE(WalkTree(tree) == "bar child 3 child 2 foo child 0 child 1");
    }

    {
        using MyTree = game::RenderTree<MyNode>;
        MyNode foo("foo", 123);
        MyNode bar("bar", 222);
        MyNode child0("child 0", 1);
        MyNode child1("child 1", 2);
        MyNode child2("child 2", 3);
        MyNode child3("child 3", 3);

        MyTree tree;
        tree.LinkChild(nullptr, &foo);
        tree.LinkChild(nullptr, &bar);
        tree.LinkChild(&foo, &child0);
        tree.LinkChild(&foo, &child1);
        tree.LinkChild(&bar, &child2);
        tree.LinkChild(&bar, &child3);

        tree.DeleteNode(&foo);
        TEST_REQUIRE(tree.HasNode(&foo) == false);
        TEST_REQUIRE(WalkTree(tree) == "bar child 2 child 3");
        tree.DeleteNode(&child3);
        TEST_REQUIRE(tree.HasNode(&child3) == false);
        //std::cout << WalkTree(tree) << std::endl;
        TEST_REQUIRE(WalkTree(tree) == "bar child 2");
    }

    // JSON serialization
    {
        using MyTree = game::RenderTree<MyNode>;
        MyNode foo("foo", 123);
        MyNode bar("bar", 222);
        MyNode child0("child 0", 1);
        MyNode child1("child 1", 2);
        MyNode child2("child 2", 3);
        MyNode child3("child 3", 3);

        MyTree tree;
        tree.LinkChild(nullptr, &foo);
        tree.LinkChild(nullptr, &bar);
        tree.LinkChild(&foo, &child0);
        tree.LinkChild(&foo, &child1);
        tree.LinkChild(&bar, &child2);
        tree.LinkChild(&bar, &child3);

        data::JsonObject json;
        game::RenderTreeIntoJson(tree, &MyNode::TreeNodeToJson, json);
        std::cout << json.ToString() << std::endl;

        tree.Clear();
        game::RenderTreeFromJson(tree, &MyNode::TreeNodeFromJson, json);
        std::cout << std::endl;
        std::cout << WalkTree(tree);
        TEST_REQUIRE(WalkTree(tree) == "foo child 0 child 1 bar child 2 child 3");

    }
}

void unit_test_treeop()
{
    {
        using MyTree = game::RenderTree<MyNode>;
        MyNode foo("foo", 123);
        MyNode bar("bar", 222);
        MyNode child0("child 0", 1);
        MyNode child1("child 1", 2);
        MyNode child2("child 2", 3);
        MyNode child3("child 3", 3);

        MyTree tree;
        tree.LinkChild(nullptr, &foo);
        tree.LinkChild(nullptr, &bar);
        tree.LinkChild(&foo, &child0);
        tree.LinkChild(&foo, &child1);
        tree.LinkChild(&bar, &child2);
        tree.LinkChild(&bar, &child3);

        std::vector<const MyNode*> path;
        // root to root is just one hop, i.e. root
        TEST_REQUIRE(game::SearchChild<MyNode>(tree, nullptr, nullptr, &path));
        TEST_REQUIRE(path.size() == 1);
        TEST_REQUIRE(path[0] == nullptr);

        path.clear();
        TEST_REQUIRE(game::SearchChild<MyNode>(tree, &child3, &child3, &path));
        TEST_REQUIRE(path.size() == 1);
        TEST_REQUIRE(path[0] == &child3);

        path.clear();
        TEST_REQUIRE(game::SearchChild<MyNode>(tree, &child3, nullptr, &path));
        TEST_REQUIRE(path.size() == 3);
        TEST_REQUIRE(path[0] == nullptr);
        TEST_REQUIRE(path[1] == &bar);
        TEST_REQUIRE(path[2] == &child3);

        // the node is not a child of the given parent.
        path.clear();
        TEST_REQUIRE(game::SearchChild<MyNode>(tree, &child3, &foo, &path) == false);

        path.clear();
        TEST_REQUIRE(game::SearchParent<MyNode>(tree, &child3, nullptr, &path));
        TEST_REQUIRE(path.size() == 3);
        TEST_REQUIRE(path[0] == &child3);
        TEST_REQUIRE(path[1] == &bar);
        TEST_REQUIRE(path[2] == nullptr);

        path.clear();
        TEST_REQUIRE(game::SearchParent<MyNode>(tree, &child3, &child3, &path));
        TEST_REQUIRE(path.size() == 1);
        TEST_REQUIRE(path[0] == &child3);

        path.clear();
        TEST_REQUIRE(game::SearchParent<MyNode>(tree, &child3, &foo, &path) == false);
    }
}

int test_main(int argc, char* argv[])
{
    unit_test_tree();
    unit_test_treeop();
    return 0;
}
