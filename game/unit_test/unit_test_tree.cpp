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

#include <cstddef>
#include <cstdio>
#include <string>
#include <iostream>
#include <limits>
#include <algorithm>

#include "base/test_minimal.h"
#include "base/test_help.h"
#include "base/utility.h"
#include "base/math.h"
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
void unit_test_render_tree()
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

void unit_test_render_tree_op()
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

struct Entity {
    std::string name;
    base::FRect rect;
};

void unit_test_quadtree_insert_query()
{
    // basic test
    {
        game::QuadTree<Entity*> tree(100.0f, 100.0f, 1);
        TEST_REQUIRE(tree->HasChildren() == false);
        TEST_REQUIRE(tree->HasItems() == false);
        TEST_REQUIRE(tree->GetRect() == base::FRect(0.0f, 0.0f, 100.0f, 100.0f));

        Entity e;
        e.name = "entity";

        TEST_REQUIRE(tree.Insert(base::FRect(0.0f, 0.0f, 5.0f, 5.0f), &e));
        TEST_REQUIRE(tree->HasItems() == true);
        TEST_REQUIRE(tree->HasChildren() == false);

        std::vector<Entity*> result;
        game::QueryQuadTree(base::FRect(6.0f, 6.0f, 10.0f, 10.0f), tree, &result);
        TEST_REQUIRE(result.empty());
        game::QueryQuadTree(base::FRect(0.0f, 0.0f, 1.0f, 1.0f), tree, &result);
        TEST_REQUIRE(result[0]->name == "entity");

        result.clear();
        game::QueryQuadTree(base::FRect(0.0f, 0.0f, 100.0f, 100.0f), tree, &result);
        TEST_REQUIRE(result[0]->name == "entity");

        tree.Clear();
        TEST_REQUIRE(tree->HasChildren() == false);
        TEST_REQUIRE(tree->HasItems() == false);
        TEST_REQUIRE(tree->GetRect() == base::FRect(0.0f, 0.0f, 100.0f, 100.0f));
        result.clear();
        game::QueryQuadTree(base::FRect(0.0f, 0.0f, 100.0f, 100.0f), tree, &result);
        TEST_REQUIRE(result.empty());

    }

    // test an object in every top level quadrant
    {
        std::vector<Entity> objects;
        objects.resize(4);
        objects[0].name = "e0";
        objects[1].name = "e1";
        objects[2].name = "e2";
        objects[3].name = "e3";

        game::QuadTree<Entity*> tree(100.0f, 100.0f, 1);

        base::FRect rect(0.0f, 0.0f, 10.0f, 10.0f);

        rect.Move(10.0f, 10.0f);
        TEST_REQUIRE(tree.Insert(rect, &objects[0]));

        rect.Move(10.0f, 60.0f);
        TEST_REQUIRE(tree.Insert(rect, &objects[1]));

        rect.Move(60.0f, 0.0f);
        TEST_REQUIRE(tree.Insert(rect, &objects[2]));

        rect.Move(60.0f, 60.0f);
        TEST_REQUIRE(tree.Insert(rect, &objects[3]));

        // Quadrants
        // _____________
        // |  0  |  2  |
        // |_____|_____|
        // |  1  |  3  |
        // |_____|_____|
        //

        TEST_REQUIRE(tree->HasChildren() == true);
        TEST_REQUIRE(tree->GetNumItems() == 0);
        TEST_REQUIRE(tree->GetChildQuadrant(0)->HasChildren() == false);
        TEST_REQUIRE(tree->GetChildQuadrant(0)->HasItems() == true);
        TEST_REQUIRE(tree->GetChildQuadrant(0)->GetRect() == base::FRect(0.0f, 0.0f, 50.0f, 50.0f));
        TEST_REQUIRE(tree->GetChildQuadrant(0)->GetItemObject(0)->name == "e0");
        TEST_REQUIRE(tree->GetChildQuadrant(0)->GetItemRect(0) == base::FRect(10.0f, 10.0f, 10.0f, 10.0f));

        TEST_REQUIRE(tree->GetChildQuadrant(1)->HasChildren() == false);
        TEST_REQUIRE(tree->GetChildQuadrant(1)->HasItems() == true);
        TEST_REQUIRE(tree->GetChildQuadrant(1)->GetRect() == base::FRect(0.0f, 50.0f, 50.0f, 50.0f));
        TEST_REQUIRE(tree->GetChildQuadrant(1)->GetItemObject(0)->name == "e1");
        TEST_REQUIRE(tree->GetChildQuadrant(1)->GetItemRect(0) == base::FRect(10.0f, 60.0f, 10.0f, 10.0f));

        TEST_REQUIRE(tree->GetChildQuadrant(2)->HasChildren() == false);
        TEST_REQUIRE(tree->GetChildQuadrant(2)->HasItems() == true);
        TEST_REQUIRE(tree->GetChildQuadrant(2)->GetRect() == base::FRect(50.0f, 0.0f, 50.0f, 50.0f));
        TEST_REQUIRE(tree->GetChildQuadrant(2)->GetItemObject(0)->name == "e2");
        TEST_REQUIRE(tree->GetChildQuadrant(2)->GetItemRect(0) == base::FRect(60.0f, 0.0f, 10.0f, 10.0f));

        TEST_REQUIRE(tree->GetChildQuadrant(3)->HasChildren() == false);
        TEST_REQUIRE(tree->GetChildQuadrant(3)->HasItems() == true);
        TEST_REQUIRE(tree->GetChildQuadrant(3)->GetRect() == base::FRect(50.0f, 50.0f, 50.0f, 50.0f));
        TEST_REQUIRE(tree->GetChildQuadrant(3)->GetItemObject(0)->name == "e3");
        TEST_REQUIRE(tree->GetChildQuadrant(3)->GetItemRect(0) == base::FRect(60.0f, 60.0f, 10.0f, 10.0f));

        // query different areas.
        // whole space
        {
            std::vector<Entity*> result;
            game::QueryQuadTree(base::FRect(0.0f, 0.0f, 100.0f, 100.0f), tree, &result);
            TEST_REQUIRE(result.size() == 4);
            TEST_REQUIRE(result[0]->name == "e0");
            TEST_REQUIRE(result[1]->name == "e1");
            TEST_REQUIRE(result[2]->name == "e2");
            TEST_REQUIRE(result[3]->name == "e3");
        }
        // q0
        {
            std::vector<Entity*> result;
            game::QueryQuadTree(base::FRect(0.0f, 0.0f, 50.0f, 50.0f), tree, &result);
            TEST_REQUIRE(result.size() == 1);
            TEST_REQUIRE(result[0]->name == "e0");
        }
        // q1
        {
            std::vector<Entity*> result;
            game::QueryQuadTree(base::FRect(0.0f, 50.0f, 50.0f, 50.0f), tree, &result);
            TEST_REQUIRE(result.size() == 1);
            TEST_REQUIRE(result[0]->name == "e1");
        }
        // q2
        {
            std::vector<Entity*> result;
            game::QueryQuadTree(base::FRect(50.0f, 0.0f, 50.0f, 50.0f), tree, &result);
            TEST_REQUIRE(result.size() == 1);
            TEST_REQUIRE(result[0]->name == "e2");
        }

        // q3
        {
            std::vector<Entity*> result;
            game::QueryQuadTree(base::FRect(50.0f, 50.0f, 50.0f, 50.0f), tree, &result);
            TEST_REQUIRE(result.size() == 1);
            TEST_REQUIRE(result[0]->name == "e3");
        }
        // q0+q2
        {
            std::vector<Entity*> result;
            game::QueryQuadTree(base::FRect(0.0f, 0.0f, 100.0f, 50.0f), tree, &result);
            TEST_REQUIRE(result.size() == 2);
            TEST_REQUIRE(result[0]->name == "e0");
            TEST_REQUIRE(result[1]->name == "e2");
        }
        // q0+q1
        {
            std::vector<Entity*> result;
            game::QueryQuadTree(base::FRect(0.0f, 0.0f, 50.0f, 100.0f), tree, &result);
            TEST_REQUIRE(result.size() == 2);
            TEST_REQUIRE(result[0]->name == "e0");
            TEST_REQUIRE(result[1]->name == "e1");
        }
    }

    // test recursive split (quadrant within a top level quadrant)
    {
        std::vector<Entity> objects;
        objects.resize(4);
        objects[0].name = "e0";
        objects[1].name = "e1";
        objects[2].name = "e2";
        objects[3].name = "e3";

        game::QuadTree<Entity*> tree(100.0f, 100.0f, 1);

        base::FRect rect(0.0f, 0.0f, 10.0f, 10.0f);

        rect.Move(10.0f, 10.0f);
        TEST_REQUIRE(tree.Insert(rect, &objects[0]));

        rect.Move(10.0f, 35.0f);
        TEST_REQUIRE(tree.Insert(rect, &objects[1]));

        rect.Move(35.0f, 10.0f);
        TEST_REQUIRE(tree.Insert(rect, &objects[2]));

        rect.Move(35.0f, 35.0f);
        TEST_REQUIRE(tree.Insert(rect, &objects[3]));

        // query different areas.
        // whole space
        {
            std::vector<Entity*> result;
            game::QueryQuadTree(base::FRect(0.0f, 0.0f, 100.0f, 100.0f), tree, &result);
            TEST_REQUIRE(result.size() == 4);
            TEST_REQUIRE(result[0]->name == "e0");
            TEST_REQUIRE(result[1]->name == "e1");
            TEST_REQUIRE(result[2]->name == "e2");
            TEST_REQUIRE(result[3]->name == "e3");
        }
        // q0
        {
            std::vector<Entity*> result;
            game::QueryQuadTree(base::FRect(0.0f, 0.0f, 50.0f, 50.0f), tree, &result);
            TEST_REQUIRE(result.size() == 4);
            TEST_REQUIRE(result[0]->name == "e0");
            TEST_REQUIRE(result[1]->name == "e1");
            TEST_REQUIRE(result[2]->name == "e2");
            TEST_REQUIRE(result[3]->name == "e3");
        }
        // q0 > q0
        {
            std::vector<Entity*> result;
            game::QueryQuadTree(base::FRect(0.0f, 0.0f, 25.0f, 25.0f), tree, &result);
            TEST_REQUIRE(result.size() == 1);
            TEST_REQUIRE(result[0]->name == "e0");
        }
    }


    // test an object that is split between quadrants
    {
        game::QuadTree<Entity*> tree(100.0f, 100.0f, 1);

        Entity e0;
        e0.name = "e0";

        Entity e1;
        e1.name = "e1";

        // add one item in order to make the node split on the next insert
        TEST_REQUIRE(tree.Insert(base::FRect(0.0f, 0.0f, 20.0f, 20.0f), &e0));

        // right in the middle so gets split to every quadrant
        TEST_REQUIRE(tree.Insert(base::FRect(45.0f, 45.0f, 10.0f, 10.0f), &e1));

        TEST_REQUIRE(tree->HasChildren());
        // top level q0 should be split
        {
            const auto* q0 = tree->GetChildQuadrant(0);
            TEST_REQUIRE(q0->GetChildQuadrant(0)->GetItemObject(0)->name == "e0");
            TEST_REQUIRE(q0->GetChildQuadrant(3)->GetItemObject(0)->name == "e1");
        }

        // query different areas
        // q0
        {
            std::vector<Entity*> result;
            game::QueryQuadTree(base::FRect(0.0f, 0.0f, 50.0f, 50.0f), tree, &result);
            TEST_REQUIRE(result.size() == 2);
            TEST_REQUIRE(result[0]->name == "e0");
            TEST_REQUIRE(result[1]->name == "e1");
        }

        // q1
        {
            std::vector<Entity*> result;
            game::QueryQuadTree(base::FRect(0.0f, 50.0f, 50.0f, 50.0f), tree, &result);
            TEST_REQUIRE(result.size() == 1);
            TEST_REQUIRE(result[0]->name == "e1");
        }
        // q2
        {
            std::vector<Entity*> result;
            game::QueryQuadTree(base::FRect(50.0f, 0.0f, 50.0f, 50.0f), tree, &result);
            TEST_REQUIRE(result.size() == 1);
            TEST_REQUIRE(result[0]->name == "e1");
        }
        // q3
        {
            std::vector<Entity*> result;
            game::QueryQuadTree(base::FRect(50.0f, 50.0f, 50.0f, 50.0f), tree, &result);
            TEST_REQUIRE(result.size() == 1);
            TEST_REQUIRE(result[0]->name == "e1");
        }
    }

    {
        std::vector<Entity> entities;
        entities.resize(10*10);
        for (int row=0; row<10; ++row)
        {
            for (int col=0; col<10; ++col)
            {
                const auto x = col * 10 + 2.5;
                const auto y = row * 10 + 2.5;
                const auto w = 5.0f;
                const auto h = 5.0f;
                entities[row * 10 + col].name = std::to_string(row) + ":" + std::to_string(col);
                entities[row * 10 + col].rect = base::FRect(x, y, w, h);
            }
        }
        game::QuadTree<Entity*> tree(100.0f, 100.0f, 2);
        for (auto& entity : entities)
            TEST_REQUIRE(tree.Insert(entity.rect, (Entity*)&entity));

        for (auto& entity : entities)
        {
            std::set<Entity*> ret;
            game::QueryQuadTree(entity.rect, tree, &ret);
            TEST_REQUIRE(ret.size() == 1);
            TEST_REQUIRE(*ret.begin() == &entity);
        }
    }
}

void unit_test_quadtree_erase()
{
    // Quadrants
    // _____________
    // |  0  |  2  |
    // |_____|_____|
    // |  1  |  3  |
    // |_____|_____|
    //

    // test an object in every top level quadrant
    {
        std::vector<Entity> objects;
        objects.resize(4);
        objects[0].name = "e0";
        objects[1].name = "e1";
        objects[2].name = "e2";
        objects[3].name = "e3";

        game::QuadTree<Entity*> tree(100.0f, 100.0f, 1);

        base::FRect rect(0.0f, 0.0f, 10.0f, 10.0f);

        rect.Move(10.0f, 10.0f);
        TEST_REQUIRE(tree.Insert(rect, &objects[0]));
        rect.Move(10.0f, 60.0f);
        TEST_REQUIRE(tree.Insert(rect, &objects[1]));
        rect.Move(60.0f, 0.0f);
        TEST_REQUIRE(tree.Insert(rect, &objects[2]));
        rect.Move(60.0f, 60.0f);
        TEST_REQUIRE(tree.Insert(rect, &objects[3]));

        TEST_REQUIRE(tree.GetRoot().HasChildren());
        // nothing should be erased
        tree.Erase([](const Entity* e, const base::FRect& rect) {
            return e->name == "keke";
        });
        TEST_REQUIRE(tree->HasChildren());

        tree.Erase([](const Entity* e, const base::FRect& rect) {
            return e->name == "e0";
        });
        TEST_REQUIRE(tree->HasChildren() == true);
        TEST_REQUIRE(tree->GetSize() == 3);
        TEST_REQUIRE(tree->GetChildQuadrant(0)->GetNumItems() == 0);
        TEST_REQUIRE(tree->GetChildQuadrant(1)->GetNumItems() == 1);
        TEST_REQUIRE(tree->GetChildQuadrant(2)->GetNumItems() == 1);
        TEST_REQUIRE(tree->GetChildQuadrant(3)->GetNumItems() == 1);
        TEST_REQUIRE(tree->GetChildQuadrant(1)->GetItemObject(0)->name == "e1");
        TEST_REQUIRE(tree->GetChildQuadrant(2)->GetItemObject(0)->name == "e2");
        TEST_REQUIRE(tree->GetChildQuadrant(3)->GetItemObject(0)->name == "e3");

        // erase everything
        tree.Erase([](const Entity* e, const base::FRect& rect) {
            return true;
        });
        TEST_REQUIRE(tree->GetSize() == 0);
        TEST_REQUIRE(tree->HasChildren() == false);
    }

    {
        std::vector<Entity> objects;
        objects.resize(4);
        objects[0].name = "e0";
        objects[1].name = "e1";
        objects[2].name = "e2";
        objects[3].name = "e3";
        game::QuadTree<Entity*> tree(-50.0f, -50.0f,100.0f, 100.0f, 4);

        base::FRect rect(-20.0f, -20.0f, 10.0f, 10.0f);
        tree.Insert(rect, &objects[0]);

        rect.Move(-20.0f, 20.0f);
        tree.Insert(rect, &objects[1]);

        rect.Move(20.0f, -20.0f);
        tree.Insert(rect, &objects[2]);

        rect.Move(20.0f, 20.0f);
        tree.Insert(rect, &objects[3]);
        TEST_REQUIRE(tree->HasChildren() == false);
        TEST_REQUIRE(tree->GetItemObject(0)->name == "e0");
        TEST_REQUIRE(tree->GetItemObject(1)->name == "e1");
        TEST_REQUIRE(tree->GetItemObject(2)->name == "e2");
        TEST_REQUIRE(tree->GetItemObject(3)->name == "e3");

        // add one more to cause the root node to split into child
        // quadrants.
        rect.Move(-25.0f, -25.0f);
        Entity ent;
        ent.name = "ent";
        tree.Insert(rect, &ent);
        TEST_REQUIRE(tree->HasChildren());
        TEST_REQUIRE(tree->GetNumItems() == 0);
        TEST_REQUIRE(tree->GetChildQuadrant(0)->GetItemObject(0)->name == "e0");
        TEST_REQUIRE(tree->GetChildQuadrant(1)->GetItemObject(0)->name == "e1");
        TEST_REQUIRE(tree->GetChildQuadrant(2)->GetItemObject(0)->name == "e2");
        TEST_REQUIRE(tree->GetChildQuadrant(3)->GetItemObject(0)->name == "e3");
        TEST_REQUIRE(tree->GetChildQuadrant(0)->GetItemObject(1)->name == "ent");

        // delete the 5th element which will cause the quadrants
        // to be deleted.
        tree.Erase([](Entity* ent, const base::FRect& rect) {
            return ent->name == "ent";
        });

        TEST_REQUIRE(tree->HasChildren() == false);
        TEST_REQUIRE(tree->GetItemObject(0)->name == "e0");
        TEST_REQUIRE(tree->GetItemObject(1)->name == "e1");
        TEST_REQUIRE(tree->GetItemObject(2)->name == "e2");
        TEST_REQUIRE(tree->GetItemObject(3)->name == "e3");
    }

    // test erasing an object that was split between quadrants
    // currently there's no merging of object back into a single
    // shape since there's no generic way to realize object identity.
    {
        game::QuadTree<Entity*> tree(-50.0f, -50.0f,100.0f, 100.0f, 1);

        Entity first;
        first.name = "first";
        first.rect = base::FRect(-10.0f, -20.0f, 10.0f, 10.0f);
        tree.Insert(first.rect, &first);
        TEST_REQUIRE(tree->GetItemObject(0)->name == "first");

        Entity split;
        split.rect = base::FRect(-10.0f, 20.0f, 20.0f, 20.0f);
        split.name = "split";
        tree.Insert(split.rect, &split);
        TEST_REQUIRE(tree->GetChildQuadrant(0)->GetItemObject(0)->name == "first");
        TEST_REQUIRE(tree->GetChildQuadrant(1)->GetItemObject(0)->name == "split");
        TEST_REQUIRE(tree->GetChildQuadrant(3)->GetItemObject(0)->name == "split");

        tree.Erase([](Entity* entity, const base::FRect& rect) {
            return entity->name == "first";
        });
        TEST_REQUIRE(tree->HasChildren() == true);
        TEST_REQUIRE(tree->GetChildQuadrant(1)->GetItemObject(0)->name == "split");
        TEST_REQUIRE(tree->GetChildQuadrant(3)->GetItemObject(0)->name == "split");
    }
}

void perf_test_quadtree_even_grid(unsigned max_items, unsigned max_levels)
{
    std::printf("Total quadtree nodes = %d\n", game::QuadTree<Entity>::FindMaxNumNodes(max_levels));

    // let's say we have a game space 1000x1000 units in size.
    // we place evenly 100x100  units in this space each within
    // their own 10x10 "cell"
    std::vector<Entity> entities;
    entities.resize(100*100);

    for (int row=0; row<100; ++row)
    {
        for (int col=0; col<100; ++col)
        {
            const auto x = col * 10 + 2.5;
            const auto y = row * 10 + 2.5;
            const auto w = 5.0f;
            const auto h = 5.0f;
            entities[row * 100 + col].name = std::to_string(row) + ":" + std::to_string(col);
            entities[row * 100 + col].rect = base::FRect(x, y, w, h);
        }
    }

    // measure tree build times.
    // the time to clear the tree is included here since that'd
    // be a realistic use-case (clear tree, then rebuild)
    {
        game::QuadTree<Entity*> tree(1000.0f, 1000.0f, max_items, max_levels);

        const auto& ret = base::TimedTest(100, [&entities, &tree]() {
            for (auto& entity : entities)
                TEST_REQUIRE(tree.Insert(entity.rect, (Entity*)&entity));
            tree.Clear();
        });
        base::PrintTestTimes("Build QuadTree", ret);
    }

    // build tree once and query several times.
    // for example let's say there was a game loop that
    // for each object checks whether it's in collision
    // with another object. so a naive O(N²) algorithm.
    {
        const auto baseline = base::TimedTest(10, [&entities]() {
            for (unsigned i = 0; i < entities.size(); ++i) {
                for (unsigned j = i+1; j < entities.size(); ++j) {
                    const auto& a = entities[i];
                    const auto& b = entities[j];
                    auto ret = base::Intersect(a.rect, b.rect);
                    if (!ret.IsEmpty())
                        std::printf("side effect for not optimizing the test away!");
                }
            }
        });
        base::PrintTestTimes("Baseline O(N²) collision", baseline);
    }

    {
        game::QuadTree<Entity*> tree(1000.0f, 1000.0f, max_items, max_levels);
        for (auto& entity : entities)
            TEST_REQUIRE(tree.Insert(entity.rect, &entity));

        const auto ret = base::TimedTest(10, [&entities, &tree]() {
            for (const auto& entity : entities) {
                std::set<Entity*> ret;
                game::QueryQuadTree(entity.rect, tree, &ret);
                TEST_REQUIRE(ret.size() == 1); // should always just find itself.
                if (*ret.begin() != &entity)
                    std::printf("side effect for not optimizing the test away!");
            }
        });
        base::PrintTestTimes("QuadTree based collision", ret);
    }
}

int test_main(int argc, char* argv[])
{
    unit_test_render_tree();
    unit_test_render_tree_op();
    unit_test_quadtree_insert_query();
    unit_test_quadtree_erase();

    bool perf_test = false;
    unsigned num_items  = game::QuadTree<Entity>::DefaultMaxItems;
    unsigned max_levels = game::QuadTree<Entity>::DefaultMaxLevels;
    for (int i=1; i<argc; ++i)
    {
        if (!std::strcmp("--perftest", argv[i]))
            perf_test = true;
        else if (!std::strcmp("--max-items", argv[i]))
            num_items = std::atoi(argv[++i]);
        else if (!std::strcmp("--max-levels", argv[i]))
            max_levels = std::atoi(argv[++i]);
        else std::printf("Unrecognized cmdline param: '%s'\n", argv[i]);
    }
    if (perf_test)
        perf_test_quadtree_even_grid(num_items, max_levels);
    return 0;
}
