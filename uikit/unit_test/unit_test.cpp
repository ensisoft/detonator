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
#include "base/test_help.h"
#include "base/assert.h"
#include "data/json.h"
#include "uikit/layout.h"
#include "uikit/painter.h"
#include "uikit/widget.h"
#include "uikit/window.h"
#include "uikit/state.h"

class Painter : public uik::Painter
{
public:
    struct Command {
        std::string name;
        std::string widget;
        std::string text;
        float line_height = 0.0f;
        PaintStruct ps;
    };
    mutable std::vector<Command> cmds;
    struct StyleInfo {
        std::string widget;
        std::string style;
    };
    mutable std::vector<StyleInfo> styles;

    void DrawWidgetBackground(const WidgetId& id, const PaintStruct& ps) const override
    {
        Command cmd;
        cmd.name   = "draw-widget-background";
        cmd.widget = id;
        cmd.ps     = ps;
        cmds.push_back(std::move(cmd));
    }
    void DrawWidgetBorder(const WidgetId& id, const PaintStruct& ps) const override
    {
        Command cmd;
        cmd.name   = "draw-widget-border";
        cmd.widget = id;
        cmd.ps     = ps;
        cmds.push_back(std::move(cmd));
    }
    void DrawStaticText(const WidgetId& id, const PaintStruct& ps, const std::string& text, float line_height) const override
    {
        Command cmd;
        cmd.name   = "draw-widget-text";
        cmd.widget = id;
        cmd.ps     = ps;
        cmd.text   = text;
        cmd.line_height = line_height;
        cmds.push_back(std::move(cmd));
    }
    void DrawEditableText(const WidgetId& id, const PaintStruct& ps, const EditableText&) const override
    {}
    void DrawWidgetFocusRect(const WidgetId& id, const PaintStruct& ps) const override
    {}
    void DrawTextEditBox(const WidgetId& id, const PaintStruct& ps) const override
    {}
    void DrawCheckBox(const WidgetId& id, const PaintStruct& ps, bool checked) const override
    {}
    void DrawRadioButton(const WidgetId& id, const PaintStruct& ps, bool selected) const override
    {}
    void DrawButton(const WidgetId& id, const PaintStruct& ps, ButtonIcon btn) const override
    {}
    virtual void DrawSlider(const WidgetId& id, const PaintStruct& ps, const uik::FRect& knob) const override
    {}
    void DrawProgressBar(const WidgetId&, const PaintStruct& ps, std::optional<float> percentage) const override
    {}
    bool ParseStyle(const WidgetId& id, const std::string& style) override
    {
        StyleInfo s;
        s.widget = id;
        s.style  = style;
        styles.push_back(std::move(s));
        return true;
    }
private:
};

class TestWidget : public uik::Widget
{
public:
    struct MouseData {
        std::string name;
        MouseEvent event;
    };
    std::vector<MouseData> mouse;
    std::string name;
    uik::FSize  size;
    uik::FPoint  point;
    base::bitflag<uik::Widget::Flags> flags;
    TestWidget()
    {
        flags.set(uik::Widget::Flags::Enabled, true);
        flags.set(uik::Widget::Flags::VisibleInGame, true);
    }

    virtual std::string GetId() const override
    { return "1234id"; }
    virtual std::string GetName() const override
    { return this->name; }
    virtual std::size_t GetHash() const override
    { return 0x12345; }
    virtual std::string GetStyleString() const override
    { return ""; }
    virtual uik::FSize GetSize() const override
    { return size; }
    virtual uik::FPoint GetPosition() const override
    { return point; }
    virtual Type GetType() const override
    { return TestWidget::Type::Label; }
    virtual bool TestFlag(Flags flag) const override
    { return this->flags.test(flag); }
    virtual void SetId(const std::string& id) override
    {}
    virtual void SetName(const std::string& name) override
    { this->name = name; }
    virtual void SetSize(const uik::FSize& size) override
    { this->size = size; }
    virtual void SetPosition(const uik::FPoint& pos) override
    { this->point = pos; }
    virtual void SetStyleString(const std::string& style) override
    {}
    virtual void SetFlag(Flags flag, bool on_off) override
    { this->flags.set(flag, on_off); }
    virtual void IntoJson(data::Writer& json) const override
    {}
    virtual bool FromJson(const data::Reader& json) override
    { return true; }

    virtual void Paint(const PaintEvent& paint, uik::State& state, uik::Painter& painter) const override
    {}
    virtual Action MouseEnter(uik::State& state) override
    {
        MouseData m;
        m.name = "enter";
        mouse.push_back(m);
        return Action{};
    }
    virtual Action MousePress(const MouseEvent& mouse, uik::State& state) override
    {
        MouseData m;
        m.name = "press";
        m.event = mouse;
        this->mouse.push_back(m);
        return Action{};
    }
    virtual Action MouseRelease(const MouseEvent& mouse, uik::State& state) override
    {
        MouseData m;
        m.name = "release";
        m.event = mouse;
        this->mouse.push_back(m);
        return Action{};
    }
    virtual Action MouseMove(const MouseEvent& mouse, uik::State& state) override
    {
        MouseData m;
        m.name = "move";
        m.event = mouse;
        this->mouse.push_back(m);
        return Action{};
    }
    virtual Action MouseLeave(uik::State& state) override
    {
        MouseData m;
        m.name = "leave";
        mouse.push_back(m);
        return Action{};
    }

    virtual std::unique_ptr<Widget> Copy() const override
    { return std::make_unique<TestWidget>(); }
    virtual std::unique_ptr<Widget> Clone() const override
    { return std::make_unique<TestWidget>(); }
private:
};

template<typename Widget>
void unit_test_widget()
{
    Widget widget;
    TEST_REQUIRE(widget.GetId() != "");
    TEST_REQUIRE(widget.GetName() == "");
    TEST_REQUIRE(widget.GetHash());
    TEST_REQUIRE(widget.GetStyleString() == "");
    TEST_REQUIRE(widget.GetSize() != uik::FSize(0.0f, 0.0f));
    TEST_REQUIRE(widget.GetPosition() == uik::FPoint(0.0f, 0.0f));
    TEST_REQUIRE(widget.TestFlag(uik::Widget::Flags::VisibleInGame));
    TEST_REQUIRE(widget.TestFlag(uik::Widget::Flags::Enabled));
    TEST_REQUIRE(widget.IsEnabled());
    TEST_REQUIRE(widget.IsVisible());

    widget.SetName("widget");
    widget.SetStyleString("style string");
    widget.SetSize(100.0f, 150.0f);
    widget.SetPosition(45.0f, 50.0f);
    widget.SetFlag(uik::Widget::Flags::VisibleInGame, false);

    data::JsonObject json;
    widget.IntoJson(json);
    {
        Widget other;
        TEST_REQUIRE(other.FromJson(json));
        TEST_REQUIRE(other.GetId() == widget.GetId());
        TEST_REQUIRE(other.GetName() == widget.GetName());
        TEST_REQUIRE(other.GetHash() == widget.GetHash());
        TEST_REQUIRE(other.GetStyleString() == widget.GetStyleString());
        TEST_REQUIRE(other.GetSize() == widget.GetSize());
        TEST_REQUIRE(other.GetPosition() == widget.GetPosition());
        TEST_REQUIRE(other.IsEnabled() == widget.IsEnabled());
        TEST_REQUIRE(other.IsVisible() == widget.IsVisible());
    }

    // copy
    {
        auto copy = widget.Copy();
        TEST_REQUIRE(copy->GetId() == widget.GetId());
        TEST_REQUIRE(copy->GetName() == widget.GetName());
        TEST_REQUIRE(copy->GetHash() == widget.GetHash());
        TEST_REQUIRE(copy->GetStyleString() == widget.GetStyleString());
        TEST_REQUIRE(copy->GetSize() == widget.GetSize());
        TEST_REQUIRE(copy->GetPosition() == widget.GetPosition());
        TEST_REQUIRE(copy->IsEnabled() == widget.IsEnabled());
        TEST_REQUIRE(copy->IsVisible() == widget.IsVisible());
    }

    // clone
    {
        auto clone = widget.Clone();
        TEST_REQUIRE(clone->GetId() != widget.GetId());
        TEST_REQUIRE(clone->GetHash() != widget.GetHash());
        TEST_REQUIRE(clone->GetName() == widget.GetName());
        TEST_REQUIRE(clone->GetStyleString() == widget.GetStyleString());
        TEST_REQUIRE(clone->GetSize() == widget.GetSize());
        TEST_REQUIRE(clone->GetPosition() == widget.GetPosition());
        TEST_REQUIRE(clone->IsEnabled() == widget.IsEnabled());
        TEST_REQUIRE(clone->IsVisible() == widget.IsVisible());
    }
}

void unit_test_label()
{
    unit_test_widget<uik::Label>();

    uik::Label widget;
    widget.SetText("hello");
    widget.SetLineHeight(2.0f);
    widget.SetSize(100.0f, 50.0f);

    // widget doesn't respond to mouse presses

    // widget doesn't respond to key presses

    // paint.
    {
        Painter p;
        uik::State s;
        uik::Widget::PaintEvent paint;
        paint.rect = widget.GetRect();
        widget.Paint(paint, s, p);
        TEST_REQUIRE(p.cmds[1].text == "hello");
        TEST_REQUIRE(p.cmds[1].line_height == real::float32(2.0f));
        TEST_REQUIRE(p.cmds[1].ps.rect == uik::FRect(0.0f, 0.0f, 100.0f, 50.0f));
    }
}

void unit_test_pushbutton()
{
    unit_test_widget<uik::PushButton>();

    uik::PushButton widget;
    widget.SetText("OK");
    widget.SetSize(100.0f, 20.0f);

    // mouse press/release -> button press action
    {
        uik::State s;
        uik::Widget::MouseEvent event;
        event.widget_window_rect = widget.GetRect();
        event.window_mouse_pos   = uik::FPoint(10.0f, 10.0f);
        event.button             = uik::MouseButton::Left;

        auto action = widget.MouseEnter(s);
        TEST_REQUIRE(action.type == uik::Widget::ActionType::None);
        action = widget.MousePress(event, s);
        TEST_REQUIRE(action.type == uik::Widget::ActionType::None);
        action = widget.MouseRelease(event, s);
        TEST_REQUIRE(action.type == uik::Widget::ActionType::ButtonPress);
        action = widget.MouseLeave(s);
        TEST_REQUIRE(action.type == uik::Widget::ActionType::None);
    }

    // mouse leaves while button down has occurred -> no action
    {
        uik::State s;
        uik::Widget::MouseEvent event;
        event.widget_window_rect = widget.GetRect();
        event.window_mouse_pos   = uik::FPoint(10.0f, 10.0f);
        event.button             = uik::MouseButton::Left;
        auto action = widget.MouseEnter(s);
        TEST_REQUIRE(action.type == uik::Widget::ActionType::None);
        action = widget.MousePress(event, s);
        TEST_REQUIRE(action.type == uik::Widget::ActionType::None);
        action = widget.MouseLeave(s);
        TEST_REQUIRE(action.type == uik::Widget::ActionType::None);
    }

    // todo: key press testing.


    // paint.
    {
        Painter p;
        uik::State s;
        uik::Widget::PaintEvent paint;
        paint.rect = widget.GetRect();
        widget.Paint(paint, s, p);
        TEST_REQUIRE(p.cmds[1].text == "OK");
        TEST_REQUIRE(p.cmds[1].ps.rect == uik::FRect(0.0f, 0.0f, 100.0f, 20.0f));
        TEST_REQUIRE(p.cmds[1].ps.pressed == false);

        uik::Widget::MouseEvent event;
        event.widget_window_rect = widget.GetRect();
        event.window_mouse_pos   = uik::FPoint(10.0f, 10.0f);
        event.button             = uik::MouseButton::Left;
        widget.MouseEnter(s);
        widget.MousePress(event, s);
        widget.Paint(paint, s, p);
        TEST_REQUIRE(p.cmds[3].ps.pressed == true);
    }
}

void unit_test_checkbox()
{
    unit_test_widget<uik::CheckBox>();

    // bug.
    {
        uik::CheckBox chk;
        chk.SetChecked(true);
        chk.SetName("check");
        chk.SetCheckLocation(uik::CheckBox::Check::Right);
        data::JsonObject json;
        chk.IntoJson(json);

        uik::CheckBox other;
        other.FromJson(json);
        TEST_REQUIRE(other.GetHash() == chk.GetHash());
        TEST_REQUIRE(other.IsChecked());
        TEST_REQUIRE(other.GetCheckLocation() == uik::CheckBox::Check::Right);
    }

}

void unit_test_groupbox()
{
    unit_test_widget<uik::GroupBox>();

    // todo:
}

void unit_test_window()
{
    uik::Window win;
    TEST_REQUIRE(win.GetId() != "");
    TEST_REQUIRE(win.GetName() == "");
    TEST_REQUIRE(win.GetNumWidgets() == 0);
    TEST_REQUIRE(win.FindWidgetByName("foo") == nullptr);
    TEST_REQUIRE(win.FindWidgetById("foo") == nullptr);

    win.SetName("window");
    win.SetStyleName("window_style.json");
    win.SetStyleString("style string");

    {
        uik::Form form;
        form.SetName("form");
        form.SetSize(400.0f, 500.0f);
        win.AddWidget(form);
        win.LinkChild(nullptr, win.FindWidgetByName("form"));
    }

    {
        uik::Label widget;
        widget.SetName("label");
        widget.SetText("label");
        win.AddWidget(widget);
        win.LinkChild(win.FindWidgetByName("form"), win.FindWidgetByName("label"));
    }

    {
        uik::PushButton widget;
        widget.SetName("pushbutton");
        widget.SetText("pushbutton");
        win.AddWidget(widget);
        win.LinkChild(win.FindWidgetByName("form"), win.FindWidgetByName("pushbutton"));
    }

    // container widget with some contained widgets in order
    // to have some widget recursion
    {
        uik::GroupBox group;
        group.SetName("groupbox");
        group.SetText("groupbox");
        win.AddWidget(group);

        uik::Label label;
        label.SetName("child_label0");
        label.SetText("child label0");
        win.AddWidget(label);

        win.LinkChild(win.FindWidgetByName("form"), win.FindWidgetByName("groupbox"));
        win.LinkChild(win.FindWidgetByName("groupbox"), win.FindWidgetByName("child_label0"));
    }

    TEST_REQUIRE(win.GetNumWidgets() == 5);
    TEST_REQUIRE(win.GetWidget(0).GetName() == "form");
    TEST_REQUIRE(win.GetWidget(1).GetName() == "label");
    TEST_REQUIRE(win.GetWidget(2).GetName() == "pushbutton");
    TEST_REQUIRE(win.GetWidget(3).GetName() == "groupbox");
    TEST_REQUIRE(win.GetWidget(4).GetName() == "child_label0");
    TEST_REQUIRE(win.FindWidgetByName("form") == &win.GetWidget(0));
    TEST_REQUIRE(win.FindWidgetByName("foobaser") == nullptr);
    TEST_REQUIRE(win.FindParent(win.FindWidgetByName("form")) == nullptr);
    TEST_REQUIRE(win.FindParent(win.FindWidgetByName("child_label0")) == win.FindWidgetByName("groupbox"));

    // hierarchy
    {
        class Visitor : public uik::Window::ConstVisitor
        {
        public:
            virtual void EnterNode(const uik::Widget* widget) override
            {
                mResult += widget->GetName();
                mResult += " ";
            }
        public:
            std::string mResult;
        } visitor;
        win.Visit(visitor, win.FindWidgetByName("form"));
        TEST_REQUIRE(visitor.mResult == "form label pushbutton groupbox child_label0 ");
    }

    // copy/assignment
    {
        uik::Window copy(win);
        TEST_REQUIRE(copy.GetHash() == win.GetHash());
        TEST_REQUIRE(copy.GetName() == "window");
        TEST_REQUIRE(copy.GetStyleName() == "window_style.json");
        TEST_REQUIRE(copy.GetStyleString() == "style string");
        TEST_REQUIRE(copy.GetNumWidgets() == 5);

        uik::Window w;
        w = win;
        TEST_REQUIRE(w.GetHash() == win.GetHash());
    }

    // serialize.
    {
        data::JsonObject json;
        win.IntoJson(json);
        // for debugging
        //std::cout << json.ToString();

        auto ret = uik::Window::FromJson(json);
        TEST_REQUIRE(ret.has_value());
        TEST_REQUIRE(ret->GetName() == "window");
        TEST_REQUIRE(ret->GetStyleName() == "window_style.json");
        TEST_REQUIRE(ret->GetStyleString() == "style string");
        TEST_REQUIRE(ret->GetNumWidgets() == 5);
        TEST_REQUIRE(ret->GetHash() == win.GetHash());
    }

    // serialize without any widgets
    {
        uik::Window win;
        data::JsonObject json;
        win.IntoJson(json);
        // for debugging
        std::cout << json.ToString();

        auto ret = uik::Window::FromJson(json);
        TEST_REQUIRE(ret.has_value());
        TEST_REQUIRE(ret->GetHash() == win.GetHash());
    }
}

void unit_test_window_paint()
{
    uik::Window win;
    {
        uik::Form  form;
        form.SetSize(500.0f, 500.0f);
        form.SetName("form");
        win.AddWidget(form);
        win.LinkChild(nullptr, win.FindWidgetByName("form"));
    }

    {
        uik::PushButton widget;
        widget.SetSize(50.0f, 20.0f);
        widget.SetPosition(25.0f, 35.0f);
        widget.SetName("pushbutton");
        widget.SetText("pushbutton");
        win.AddWidget(widget);
        win.LinkChild(win.FindWidgetByName("form"), win.FindWidgetByName("pushbutton"));
    }
    // container widget with some contained widgets in order
    // to have some widget recursion
    {
        uik::GroupBox group;
        group.SetName("groupbox");
        group.SetText("groupbox");
        group.SetSize(100.0f, 150.0f);
        group.SetPosition(300.0f, 400.0f);
        win.AddWidget(group);

        uik::Label label;
        label.SetName("child_label0");
        label.SetText("child label0");
        label.SetSize(100.0f, 10.0f);
        label.SetPosition(5.0f, 5.0f);
        win.AddWidget(label);

        win.LinkChild(win.FindWidgetByName("form"), win.FindWidgetByName("groupbox"));
        win.LinkChild(win.FindWidgetByName("groupbox"), win.FindWidgetByName("child_label0"));
    }
    uik::State state;
    const uik::Widget* form   = win.FindWidgetByName("form");
    const uik::Widget* button = win.FindWidgetByName("pushbutton");
    const uik::Widget* group  = win.FindWidgetByName("groupbox");
    const uik::Widget* label  = win.FindWidgetByName("child_label0");

    Painter p;
    win.Paint(state, p);

    // form.
    {
        TEST_REQUIRE(p.cmds[0].name == "draw-widget-background");
        TEST_REQUIRE(p.cmds[0].widget == form->GetId());
        TEST_REQUIRE(p.cmds[0].ps.clip.IsEmpty());
        TEST_REQUIRE(p.cmds[0].ps.rect == uik::FRect(0.0f, 0.0f, 500.0f, 500.0f));
        TEST_REQUIRE(p.cmds[1].name == "draw-widget-border");
        TEST_REQUIRE(p.cmds[1].widget == form->GetId());
        TEST_REQUIRE(p.cmds[1].ps.clip.IsEmpty());
        TEST_REQUIRE(p.cmds[1].ps.rect == uik::FRect(0.0f, 0.0f, 500.0f, 500.0f));
    }

    // push button.
    {
        TEST_REQUIRE(p.cmds[2].name == "draw-widget-background");
        TEST_REQUIRE(p.cmds[2].widget == button->GetId());
        TEST_REQUIRE(p.cmds[2].ps.clip == uik::FRect(0.0f, 0.0f, 500.0f, 500.0f));
        TEST_REQUIRE(p.cmds[2].ps.rect == uik::FRect(25.0f, 35.0f, 50.0f, 20.0f));
        TEST_REQUIRE(p.cmds[3].name == "draw-widget-text");
        TEST_REQUIRE(p.cmds[3].widget == button->GetId());
        TEST_REQUIRE(p.cmds[3].ps.clip == uik::FRect(0.0f, 0.0f, 500.0f, 500.0f));
        TEST_REQUIRE(p.cmds[3].ps.rect == uik::FRect(25.0f, 35.0f, 50.0f, 20.0f));
        TEST_REQUIRE(p.cmds[4].name == "draw-widget-border");
        TEST_REQUIRE(p.cmds[4].widget == button->GetId());
        TEST_REQUIRE(p.cmds[4].ps.clip == uik::FRect(0.0f, 0.0f, 500.0f, 500.0f));
        TEST_REQUIRE(p.cmds[4].ps.rect == uik::FRect(25.0f, 35.0f, 50.0f, 20.0f));
    }

    // group box.
    {
        TEST_REQUIRE(p.cmds[5].name == "draw-widget-background");
        TEST_REQUIRE(p.cmds[5].widget == group->GetId());
        TEST_REQUIRE(p.cmds[5].ps.clip == uik::FRect(0.0f, 0.0f, 500.0f, 500.0f));
        TEST_REQUIRE(p.cmds[5].ps.rect == uik::FRect(300.0f, 400.0f, 100.0f, 150.0f));
        TEST_REQUIRE(p.cmds[6].name == "draw-widget-border");
        TEST_REQUIRE(p.cmds[6].widget == group->GetId());
        TEST_REQUIRE(p.cmds[6].ps.clip == uik::FRect(0.0f, 0.0f, 500.0f, 500.0f));
        TEST_REQUIRE(p.cmds[6].ps.rect == uik::FRect(300.0f, 400.0f, 100.0f, 150.0f));
    }

    // label (child of the group box)
    {
        TEST_REQUIRE(p.cmds[7].name == "draw-widget-background");
        TEST_REQUIRE(p.cmds[7].widget == label->GetId());
        TEST_REQUIRE(p.cmds[7].ps.clip == uik::FRect(300.0f, 400.0f, 100.0f, 150.0f));
        TEST_REQUIRE(p.cmds[7].ps.rect == uik::FRect(305.0f, 405.0f, 100.0f, 10.0f));
    }
}

void unit_test_window_mouse()
{
    uik::Window win;

    {
        uik::Form form;
        form.SetName("form");
        form.SetSize(500.0f, 500.0f);
        win.AddWidget(form);
        win.LinkChild(nullptr, win.FindWidgetByName("form"));
    }

    {
        TestWidget t;
        t.SetName("widget0");
        t.SetSize(uik::FSize(40.0f, 40.0f));
        t.SetPosition(uik::FPoint(20.0f, 20.0f));
        win.AddWidget(t);
        win.LinkChild(win.FindWidgetByName("form"), win.FindWidgetByName("widget0"));
    }

    {
        TestWidget t;
        t.SetName("widget1");
        t.SetSize(uik::FSize(20.0f, 20.0f));
        t.SetPosition(uik::FPoint(100.0f, 100.0f));
        win.AddWidget(t);
        win.LinkChild(win.FindWidgetByName("form"), win.FindWidgetByName("widget1"));
    }

    auto* widget0 = static_cast<TestWidget*>(win.FindWidgetByName("widget0"));
    auto* widget1 = static_cast<TestWidget*>(win.FindWidgetByName("widget1"));

    // mouse enter, mouse leave.
    {
        uik::State state;
        uik::Window::MouseEvent mouse;
        mouse.window_mouse_pos = uik::FPoint(10.0f , 10.0f);
        win.MouseMove(mouse , state);

        mouse.window_mouse_pos = uik::FPoint(25.0f , 25.0f);
        win.MouseMove(mouse , state);
        TEST_REQUIRE(widget0->mouse[0].name == "enter");
        TEST_REQUIRE(widget0->mouse[1].name == "move");
        TEST_REQUIRE(widget0->mouse[1].event.widget_mouse_pos == uik::FPoint(5.0f , 5.0f));

        mouse.window_mouse_pos = uik::FPoint(65.0f, 65.0f);
        win.MouseMove(mouse , state);
        TEST_REQUIRE(widget0->mouse[2].name == "leave");
    }
}



void unit_test_window_transforms()
{
    uik::Window win;
    {
        uik::Form  form;
        form.SetSize(500.0f, 500.0f);
        form.SetName("form");
        win.AddWidget(form);
        win.LinkChild(nullptr, win.FindWidgetByName("form"));
    }

    {
        uik::PushButton widget;
        widget.SetSize(50.0f, 20.0f);
        widget.SetPosition(25.0f, 35.0f);
        widget.SetName("pushbutton");
        widget.SetText("pushbutton");
        win.AddWidget(widget);
        win.LinkChild(win.FindWidgetByName("form"), win.FindWidgetByName("pushbutton"));
    }
    // container widget with some contained widgets in order
    // to have some widget recursion
    {
        uik::GroupBox group;
        group.SetName("groupbox");
        group.SetText("groupbox");
        group.SetSize(100.0f, 150.0f);
        group.SetPosition(300.0f, 400.0f);
        win.AddWidget(group);

        uik::Label label;
        label.SetName("child_label0");
        label.SetText("child label0");
        label.SetSize(30.0f, 10.0f);
        label.SetPosition(5.0f, 5.0f);
        win.AddWidget(label);

        win.LinkChild(win.FindWidgetByName("form"), win.FindWidgetByName("groupbox"));
        win.LinkChild(win.FindWidgetByName("groupbox"), win.FindWidgetByName("child_label0"));
    }
    uik::State state;
    const uik::Widget* form   = win.FindWidgetByName("form");
    const uik::Widget* button = win.FindWidgetByName("pushbutton");
    const uik::Widget* group  = win.FindWidgetByName("groupbox");
    const uik::Widget* label  = win.FindWidgetByName("child_label0");

    // hit test
    {
        TEST_REQUIRE(!win.HitTest(-1.0f, 0.0f));
        TEST_REQUIRE(!win.HitTest(501.0f, 0.0f));
        TEST_REQUIRE(!win.HitTest(250.0f, -1.0f));
        TEST_REQUIRE(!win.HitTest(250.0f, 501.0f));

        uik::FPoint pos;
        TEST_REQUIRE(win.HitTest(26.0f, 36.0f, &pos) == button);
        TEST_REQUIRE(pos == uik::FPoint(1.0f, 1.0f));

        TEST_REQUIRE(win.HitTest(300.5f, 400.5f, &pos) == group);
        TEST_REQUIRE(pos == uik::FPoint(0.5f,  0.5f));
        TEST_REQUIRE(win.HitTest(399.0f, 549.0f, &pos) == group);
        TEST_REQUIRE(pos == uik::FPoint(99.0f, 149.0f));

        TEST_REQUIRE(win.HitTest(300.0f+5.0f+0.5f, 400.0f+5.0f+0.5f) == label);
        TEST_REQUIRE(win.HitTest(405.0f, 450.0f) == form);
    }


    // widget rect
    {

    }
}

void unit_test_util()
{
    auto widget = uik::CreateWidget(uik::Widget::Type::Label);

    auto* label = uik::WidgetCast<uik::Label>(widget);
    auto* spin  = uik::WidgetCast<uik::SpinBox>(widget);
    TEST_REQUIRE(label != nullptr);
    TEST_REQUIRE(spin == nullptr);
}

void unit_test_apply_style()
{
    uik::Window win;

    {
        uik::Label lbl;
        lbl.SetStyleString("label style");
        win.AddWidget(lbl);
    }
    {
        uik::CheckBox chk;
        chk.SetStyleString("check style");
        win.AddWidget(chk);
    }
    {
        uik::PushButton btn;
        btn.SetStyleString("button style");
        win.AddWidget(btn);
    }

    Painter p;
    win.Style(p);

    TEST_REQUIRE(p.styles.size() == 3);
    TEST_REQUIRE(p.styles[0].style == "label style");
    TEST_REQUIRE(p.styles[1].style == "check style");
    TEST_REQUIRE(p.styles[2].style == "button style");
}

int test_main(int argc, char* argv[])
{
    unit_test_label();
    unit_test_pushbutton();
    unit_test_checkbox();
    unit_test_groupbox();
    unit_test_window();
    unit_test_window_paint();
    unit_test_window_mouse();
    unit_test_window_transforms();
    unit_test_util();
    unit_test_apply_style();
    return 0;
}