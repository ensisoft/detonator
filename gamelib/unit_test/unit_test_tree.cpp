// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#include "config.h"

#include "warnpush.h"
#  include <nlohmann/json.hpp>
#include "warnpop.h"

#include <string>
#include <cstddef>
#include <iostream>

#include "base/test_minimal.h"
#include "gamelib/tree.h"

struct MyNode {
    std::string s;
    std::size_t i;
    MyNode(const std::string& s, std::size_t i)
      : s(s)
      , i(i)
    {}
    MyNode() = default;
    static MyNode* TreeNodeFromJson(const nlohmann::json& json)
    {
        static std::vector<std::unique_ptr<MyNode>> nodes;

        auto ret = std::make_unique<MyNode>();
        ret->s = json["s"];
        ret->i = json["i"];
        nodes.push_back(std::move(ret));
        return nodes.back().get();
    }
};

nlohmann::json TreeNodeToJson(const MyNode* node)
{
    nlohmann::json ret;
    ret["s"] = node->s;
    ret["i"] = node->i;
    return ret;
}

int test_main(int argc, char* argv[])
{
    using MyTree = game::TreeNode<MyNode>;

    {
        MyTree tree;
        TEST_REQUIRE(tree.GetNumChildren() == 0);
        TEST_REQUIRE(tree.GetNumNodes() == 1);
        TEST_REQUIRE(tree.FindParent(&tree) == nullptr);

        MyNode foo("foo", 123);
        MyNode bar("bar", 222);

        tree.AppendChild(&foo);
        tree.AppendChild(&bar);
        TEST_REQUIRE(tree.GetNumChildren() == 2);
        TEST_REQUIRE(tree.GetNumNodes() == 3);
        TEST_REQUIRE(tree.GetChildNode(0).GetValue()->s == "foo");
        TEST_REQUIRE(tree.GetChildNode(0).GetValue()->i == 123);
        TEST_REQUIRE(tree.GetChildNode(1).GetValue()->s == "bar");
        TEST_REQUIRE(tree.GetChildNode(1).GetValue()->i == 222);
        TEST_REQUIRE(tree.FindNodeByValue(&foo) == &tree.GetChildNode(0));
        TEST_REQUIRE(tree.FindNodeByValue(&bar) == &tree.GetChildNode(1));

        tree.TakeChild(0);
        TEST_REQUIRE(tree.GetNumChildren() == 1);
        TEST_REQUIRE(tree.GetChildNode(0).GetValue()->s == "bar");
        TEST_REQUIRE(tree.GetChildNode(0).GetValue()->i == 222);
        tree.TakeChild(0);
        TEST_REQUIRE(tree.GetNumChildren() == 0);

        MyNode child0("child 0", 1);
        MyNode child1("child 1", 2);

        tree.InsertChild(&child0, 0);
        tree.InsertChild(&child1, 1);
        TEST_REQUIRE(tree.GetNumChildren() == 2);
        TEST_REQUIRE(tree.GetChildNode(0).GetValue()->s == "child 0");
        TEST_REQUIRE(tree.GetChildNode(1).GetValue()->s == "child 1");

        MyNode child2("child 2", 3);
        // insert at the start.
        tree.InsertChild(&child2, 0);
        TEST_REQUIRE(tree.GetNumChildren() == 3);
        TEST_REQUIRE(tree.GetChildNode(0).GetValue()->s == "child 2");
        TEST_REQUIRE(tree.GetChildNode(1).GetValue()->s == "child 0");
        TEST_REQUIRE(tree.GetChildNode(2).GetValue()->s == "child 1");

        MyNode child3("child 3", 3);
        // insert in the middle
        tree.InsertChild(&child3, 1);
        TEST_REQUIRE(tree.GetNumChildren() == 4);
        TEST_REQUIRE(tree.GetChildNode(0).GetValue()->s == "child 2");
        TEST_REQUIRE(tree.GetChildNode(1).GetValue()->s == "child 3");
        TEST_REQUIRE(tree.GetChildNode(2).GetValue()->s == "child 0");
        TEST_REQUIRE(tree.GetChildNode(3).GetValue()->s == "child 1");

        // take from the middle
        tree.TakeChild(1);
        TEST_REQUIRE(tree.GetNumChildren() == 3);
        TEST_REQUIRE(tree.GetChildNode(0).GetValue()->s == "child 2");
        TEST_REQUIRE(tree.GetChildNode(1).GetValue()->s == "child 0");
        TEST_REQUIRE(tree.GetChildNode(2).GetValue()->s == "child 1");
    }

    {
        MyNode root("root", 123);
        MyNode child0("child 0", 1);
        MyNode child1("child 1", 2);
        MyNode grand_child0("grand child 0", 4);
        MyNode grand_child1("grand child 1", 5);

        MyTree tree;
        tree.SetValue(&root);
        tree.AppendChild(&child0);
        tree.AppendChild(&child1);
        tree.GetChildNode(0).AppendChild(&grand_child0);
        tree.GetChildNode(0).AppendChild(&grand_child1);

        TEST_REQUIRE(tree.GetNumNodes() == 5);
        TEST_REQUIRE(tree.FindNodeByValue(&grand_child0) == &tree.GetChildNode(0).GetChildNode(0));
        TEST_REQUIRE(tree.FindNodeByValue(&grand_child1) == &tree.GetChildNode(0).GetChildNode(1));
        TEST_REQUIRE(tree.FindParent(&tree.GetChildNode(0).GetChildNode(0)) == &tree.GetChildNode(0));
        TEST_REQUIRE(tree.FindParent(&tree.GetChildNode(0).GetChildNode(1)) == &tree.GetChildNode(0));
    }

    // serialization
    {
        MyNode root("root", 123);
        MyNode child0("child 0", 1);
        MyNode child1("child 1", 2);
        MyNode grand_child0("grand child 0", 4);
        MyNode grand_child1("grand child 1", 5);

        MyTree tree;
        tree.SetValue(&root);
        tree.AppendChild(&child0);
        tree.AppendChild(&child1);
        tree.GetChildNode(0).AppendChild(&grand_child0);
        tree.GetChildNode(0).AppendChild(&grand_child1);

        auto json = tree.ToJson();
        std::cout << json.dump(2) << std::endl;

        tree.Clear();

        tree = MyTree::FromJson(json).value();
        TEST_REQUIRE(tree.GetNumNodes() == 5);
        TEST_REQUIRE(tree.GetValue()->s == "root");
        TEST_REQUIRE(tree.GetValue()->i == 123);
        TEST_REQUIRE(tree.GetChildNode(0).GetValue()->s == "child 0");
        TEST_REQUIRE(tree.GetChildNode(1).GetValue()->s == "child 1");
        TEST_REQUIRE(tree.GetChildNode(0).GetChildNode(0).GetValue()->s == "grand child 0");
        TEST_REQUIRE(tree.GetChildNode(0).GetChildNode(1).GetValue()->s == "grand child 1");
    }

    // traversal
    {
        MyNode root("root", 123);
        MyNode child0("child 0", 1);
        MyNode child1("child 1", 2);
        MyNode grand_child0("grand child 0", 4);
        MyNode grand_child1("grand child 1", 5);

        MyTree tree;
        tree.SetValue(&root);
        tree.AppendChild(&child0);
        tree.AppendChild(&child1);
        tree.GetChildNode(0).AppendChild(&grand_child0);
        tree.GetChildNode(0).AppendChild(&grand_child1);

        class Visitor : public MyTree::Visitor
        {
        public:
            virtual void EnterNode(MyNode* node) override
            {
                nodes.push_back(node);
            }
            // flat list of nodes in the order they're traversed.
            std::vector<MyNode*> nodes;
        };
        Visitor visitor;
        tree.PreOrderTraverse(visitor);
        TEST_REQUIRE(visitor.nodes.size() == 5);
        TEST_REQUIRE(visitor.nodes[0]->s == "root");
        TEST_REQUIRE(visitor.nodes[1]->s == "child 0");
        TEST_REQUIRE(visitor.nodes[2]->s == "grand child 0");
        TEST_REQUIRE(visitor.nodes[3]->s == "grand child 1");
        TEST_REQUIRE(visitor.nodes[4]->s == "child 1");
    }

    // traversal early exit.
    {
        MyNode root("root", 123);
        MyNode child0("child 0", 1);
        MyNode child1("child 1", 2);
        MyNode grand_child0("grand child 0", 4);
        MyNode grand_child1("grand child 1", 5);

        MyTree tree;
        tree.SetValue(&root);
        tree.AppendChild(&child0);
        tree.AppendChild(&child1);
        tree.GetChildNode(0).AppendChild(&grand_child0);
        tree.GetChildNode(0).AppendChild(&grand_child1);

        class Visitor : public MyTree::Visitor
        {
        public:
            virtual void EnterNode(MyNode* node) override
            {
                nodes.push_back(node);
                if (node->s == "grand child 0")
                    done = true;
            }
            virtual bool IsDone() const override
            { return done; }
            // flat list of nodes in the order they're traversed.
            std::vector<MyNode*> nodes;
            bool done = false;
        };
        Visitor visitor;
        tree.PreOrderTraverse(visitor);
        TEST_REQUIRE(visitor.nodes.size() == 3);
        TEST_REQUIRE(visitor.nodes[0]->s == "root");
        TEST_REQUIRE(visitor.nodes[1]->s == "child 0");
        TEST_REQUIRE(visitor.nodes[2]->s == "grand child 0");
    }

    return 0;
}