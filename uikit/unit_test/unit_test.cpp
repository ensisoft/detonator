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
#include "uikit/animation.h"
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
    void DrawScrollBar(const WidgetId&, const PaintStruct& ps, const uik::FRect& handle) const override
    {

    }
    virtual void DrawToggle(const WidgetId& id, const PaintStruct& ps, const uik::FRect& knob, bool on_off) const override
    {

    }
    virtual void DrawShape(const WidgetId& id, const PaintStruct& ps, const Shape& shape) const override
    {

    }

    virtual void PushMask(const MaskStruct& mask) override
    {
        mClipMaskStack.push(mask);
    }
    virtual void PopMask() override
    {
        TEST_REQUIRE(!mClipMaskStack.empty());
    }

    virtual bool ParseStyle(const WidgetId& id, const std::string& style) override
    {
        StyleInfo s;
        s.widget = id;
        s.style  = style;
        styles.push_back(std::move(s));
        return true;
    }
private:
    std::stack<MaskStruct> mClipMaskStack;
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
    virtual std::string GetAnimationString() const override
    { return ""; }
    virtual uik::FSize GetSize() const override
    { return size; }
    virtual uik::FPoint GetPosition() const override
    { return point; }
    virtual Type GetType() const override
    { return TestWidget::Type::Label; }
    virtual bool TestFlag(Flags flag) const override
    { return this->flags.test(flag); }
    virtual unsigned GetTabIndex() const override
    { return 0; }
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
    virtual void SetAnimationString(const std::string&) override
    {}
    virtual void SetFlag(Flags flag, bool on_off) override
    { this->flags.set(flag, on_off); }
    virtual void SetTabIndex(unsigned index) override
    { }
    virtual void IntoJson(data::Writer& json) const override
    {}
    virtual bool FromJson(const data::Reader& json) override
    { return true; }

    virtual void Paint(const PaintEvent& paint, const uik::TransientState& state, uik::Painter& painter) const override
    {}
    virtual Action MouseEnter(uik::TransientState& state) override
    {
        MouseData m;
        m.name = "enter";
        mouse.push_back(m);
        return Action{};
    }
    virtual Action MousePress(const MouseEvent& mouse, uik::TransientState& state) override
    {
        MouseData m;
        m.name = "press";
        m.event = mouse;
        this->mouse.push_back(m);
        return Action{};
    }
    virtual Action MouseRelease(const MouseEvent& mouse, uik::TransientState& state) override
    {
        MouseData m;
        m.name = "release";
        m.event = mouse;
        this->mouse.push_back(m);
        return Action{};
    }
    virtual Action MouseMove(const MouseEvent& mouse, uik::TransientState& state) override
    {
        MouseData m;
        m.name = "move";
        m.event = mouse;
        this->mouse.push_back(m);
        return Action{};
    }
    virtual Action MouseLeave(uik::TransientState& state) override
    {
        MouseData m;
        m.name = "leave";
        mouse.push_back(m);
        return Action{};
    }
    virtual Action KeyDown(const KeyEvent& key, uik::TransientState& state) override
    {
        return {};
    }
    virtual Action KeyUp(const KeyEvent& key, uik::TransientState& state) override
    {
        return {};
    }

    virtual std::unique_ptr<Widget> Copy() const override
    { return std::make_unique<TestWidget>(); }
    virtual std::unique_ptr<Widget> Clone() const override
    { return std::make_unique<TestWidget>(); }
    virtual void SetStyleProperty(const std::string& key, const uik::StyleProperty& prop) override
    {}
    virtual const uik::StyleProperty* GetStyleProperty(const std::string& key) const override
    {
        return nullptr;
    }
    virtual void DeleteStyleProperty(const std::string& key) override
    {}
    virtual void SetStyleMaterial(const std::string& key, const std::string& material) override
    {

    }
    virtual const std::string* GetStyleMaterial(const std::string& key) const override
    {
        return nullptr;
    }
    virtual void DeleteStyleMaterial(const std::string& key) override
    {

    }
    virtual void CopyStateFrom(const Widget* other) override
    {

    }
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
    TEST_REQUIRE(widget.GetTabIndex() == 0);

    widget.SetName("widget");
    widget.SetStyleString("style string");
    widget.SetAnimationString("animation string");
    widget.SetSize(100.0f, 150.0f);
    widget.SetPosition(45.0f, 50.0f);
    widget.SetFlag(uik::Widget::Flags::VisibleInGame, false);
    widget.SetStyleProperty("flag", true);
    widget.SetStyleProperty("float", 1.0f);
    widget.SetStyleProperty("string", std::string("foobar"));
    widget.SetTabIndex(123);

    data::JsonObject json;
    widget.IntoJson(json);
    {
        Widget other;
        TEST_REQUIRE(other.FromJson(json));
        TEST_REQUIRE(other.GetId() == widget.GetId());
        TEST_REQUIRE(other.GetName() == widget.GetName());
        TEST_REQUIRE(other.GetHash() == widget.GetHash());
        TEST_REQUIRE(other.GetStyleString() == widget.GetStyleString());
        TEST_REQUIRE(other.GetAnimationString() == widget.GetAnimationString());
        TEST_REQUIRE(other.GetSize() == widget.GetSize());
        TEST_REQUIRE(other.GetPosition() == widget.GetPosition());
        TEST_REQUIRE(other.IsEnabled() == widget.IsEnabled());
        TEST_REQUIRE(other.IsVisible() == widget.IsVisible());
        TEST_REQUIRE(other.GetTabIndex() == widget.GetTabIndex());
        uik::StyleProperty prop = *other.GetStyleProperty("flag");
        TEST_REQUIRE(std::get<bool>(prop) == true);
        prop = *other.GetStyleProperty("float");
        TEST_REQUIRE(std::get<float>(prop) == real::float32(1.0f));
        prop = *other.GetStyleProperty("string");
        TEST_REQUIRE(std::get<std::string>(prop) == "foobar");
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
        TEST_REQUIRE(copy->GetTabIndex() == widget.GetTabIndex());
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
        TEST_REQUIRE(clone->GetTabIndex() == widget.GetTabIndex());
    }
}

void unit_test_label()
{
    TEST_CASE(test::Type::Feature)

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
        uik::TransientState s;
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
    TEST_CASE(test::Type::Feature)

    unit_test_widget<uik::PushButton>();

    uik::PushButton widget;
    widget.SetText("OK");
    widget.SetSize(100.0f, 20.0f);

    // mouse press/release -> button press action
    {
        uik::TransientState s;
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
        uik::TransientState s;
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
        uik::TransientState s;
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
    TEST_CASE(test::Type::Feature)

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
    TEST_CASE(test::Type::Feature)

    unit_test_widget<uik::GroupBox>();

    // todo:
}

void unit_test_window()
{
    TEST_CASE(test::Type::Feature)

    uik::Window win;
    TEST_REQUIRE(win.GetId() != "");
    TEST_REQUIRE(win.GetName() == "");
    TEST_REQUIRE(win.GetNumWidgets() == 0);
    TEST_REQUIRE(win.FindWidgetByName("foo") == nullptr);
    TEST_REQUIRE(win.FindWidgetById("foo") == nullptr);

    win.SetName("window");
    win.SetStyleName("window_style.json");
    win.SetStyleString("style string");
    win.SetScriptFile("123sgsss");

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
        TEST_REQUIRE(copy.GetScriptFile() == "123sgsss");
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

        uik::Window ret;
        TEST_REQUIRE(ret.FromJson(json));
        TEST_REQUIRE(ret.GetName() == "window");
        TEST_REQUIRE(ret.GetStyleName() == "window_style.json");
        TEST_REQUIRE(ret.GetStyleString() == "style string");
        TEST_REQUIRE(ret.GetScriptFile() == "123sgsss");
        TEST_REQUIRE(ret.GetNumWidgets() == 5);
        TEST_REQUIRE(ret.GetHash() == win.GetHash());
    }

    // serialize without any widgets
    {
        uik::Window win;
        data::JsonObject json;
        win.IntoJson(json);
        // for debugging
        std::cout << json.ToString();

        uik::Window ret;
        TEST_REQUIRE(ret.FromJson(json));
        TEST_REQUIRE(ret.GetHash() == win.GetHash());
    }
}

void unit_test_window_paint()
{
    TEST_CASE(test::Type::Feature)

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
    uik::TransientState state;
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
    TEST_CASE(test::Type::Feature)

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
        uik::TransientState state;
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
    TEST_CASE(test::Type::Feature)

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
    uik::TransientState state;
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
    TEST_CASE(test::Type::Feature)

    auto widget = uik::CreateWidget(uik::Widget::Type::Label);

    auto* label = uik::WidgetCast<uik::Label>(widget);
    auto* spin  = uik::WidgetCast<uik::SpinBox>(widget);
    TEST_REQUIRE(label != nullptr);
    TEST_REQUIRE(spin == nullptr);
}

void unit_test_apply_style()
{
    TEST_CASE(test::Type::Feature)

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

void unit_test_keyboard_focus()
{
    TEST_CASE(test::Type::Feature)

    // no widgets that can take keyboard focus.
    {
        uik::TransientState state;
        uik::Window window;
        window.EnableVirtualKeys(true);
        window.Open(state);

        uik::Window::KeyEvent event;
        event.key  = uik::VirtualKey::FocusNext;
        event.time = 0.0;
        auto actions = window.KeyDown(event, state);
        TEST_REQUIRE(actions.empty());
    }

    // widget that can take a keyboard focus is in a container
    {
        uik::TransientState state;
        uik::Window window;
        window.EnableVirtualKeys(true);

        auto* btn = window.AddWidget(uik::PushButton());
        auto* box = window.AddWidget(uik::GroupBox());
        window.LinkChild(nullptr, box);
        window.LinkChild(box, btn);
        window.Open(state);
        TEST_REQUIRE(window.GetFocusedWidget(state) == btn);
    }

    // widget that can focus is in a container that is disabled.
    {
        uik::TransientState state;
        uik::Window window;
        window.EnableVirtualKeys(true);

        auto* btn = window.AddWidget(uik::PushButton());
        auto* box = window.AddWidget(uik::GroupBox());
        box->SetEnabled(false);
        window.LinkChild(nullptr, box);
        window.LinkChild(box, btn);
        window.Open(state);
        TEST_REQUIRE(window.GetFocusedWidget(state) == nullptr);
    }

    // widget that can focus is in a container that is hidden
    {
        uik::TransientState state;
        uik::Window window;
        window.EnableVirtualKeys(true);

        auto* btn = window.AddWidget(uik::PushButton());
        auto* box = window.AddWidget(uik::GroupBox());
        box->SetVisible(false);
        window.LinkChild(nullptr, box);
        window.LinkChild(box, btn);
        window.Open(state);
        TEST_REQUIRE(window.GetFocusedWidget(state) == nullptr);
    }


    // one keyboard focusable widget
    {
        uik::Window window;
        uik::TransientState state;

        auto* btn = window.AddWidget(uik::PushButton());
        window.LinkChild(nullptr, btn);

        window.EnableVirtualKeys(true);
        window.Open(state);
        TEST_REQUIRE(window.GetFocusedWidget(state) == btn);

        uik::Window::KeyEvent event;
        event.key  = uik::VirtualKey::FocusNext;
        event.time = 0.0;
        window.KeyDown(event, state);
        TEST_REQUIRE(window.GetFocusedWidget(state) == btn);

        event.key  = uik::VirtualKey::FocusPrev;
        event.time = 0.0;
        window.KeyDown(event, state);
        TEST_REQUIRE(window.GetFocusedWidget(state) == btn);
    }

    // cycle over multiple focusable widgets backwards and forwards.
    {
        uik::Window window;
        uik::TransientState state;

        window.EnableVirtualKeys(true);
        auto* btn0 = window.AddWidget(uik::PushButton());
        auto* btn1 = window.AddWidget(uik::PushButton());
        auto* lbl  = window.AddWidget(uik::Label());
        TEST_REQUIRE(btn0->GetTabIndex() == 0);
        TEST_REQUIRE(btn1->GetTabIndex() == 1);
        window.LinkChild(nullptr, btn0);
        window.LinkChild(nullptr, btn1);
        window.LinkChild(nullptr, lbl);

        window.Open(state);
        TEST_REQUIRE(window.GetFocusedWidget(state) == btn0);

        uik::Window::KeyEvent event;
        event.key  = uik::VirtualKey::FocusNext;
        event.time = 0.0;
        window.KeyDown(event, state);
        TEST_REQUIRE(window.GetFocusedWidget(state) == btn1);

        event.key  = uik::VirtualKey::FocusNext;
        event.time = 0.0;
        window.KeyDown(event, state);
        TEST_REQUIRE(window.GetFocusedWidget(state) == btn0);

        event.key  = uik::VirtualKey::FocusPrev;
        event.time = 0.0;
        window.KeyDown(event, state);
        TEST_REQUIRE(window.GetFocusedWidget(state) == btn1);
    }

    {

    }
}

void unit_test_keyboard_radiobutton_select()
{
    TEST_CASE(test::Type::Feature)

    // when several radio buttons are in the same container (group)
    // using the up/down virtual keys will cycle over the radio buttons

    {

        uik::TransientState state;
        uik::Window window;
        window.EnableVirtualKeys(true);

        auto* rad0 = window.AddWidget(uik::RadioButton());
        rad0->SetSelected(true);
        window.LinkChild(nullptr, rad0);

        window.Open(state);
        TEST_REQUIRE(window.GetFocusedWidget(state) == rad0);
        TEST_REQUIRE(rad0->IsSelected() == true);

        uik::Window::KeyEvent event;
        event.key = uik::VirtualKey::MoveDown;
        event.time = 0.0;
        window.KeyDown(event, state);
        TEST_REQUIRE(rad0->IsSelected() == true);

        event.key = uik::VirtualKey::MoveUp;
        window.KeyDown(event, state);
        TEST_REQUIRE(rad0->IsSelected() == true);
    }

    // switch between two buttons.
    {
        uik::TransientState state;
        uik::Window window;
        window.EnableVirtualKeys(true);

        auto* rad0 = window.AddWidget(uik::RadioButton());
        auto* rad1 = window.AddWidget(uik::RadioButton());
        rad0->SetSelected(true);
        rad1->SetSelected(false);
        window.LinkChild(nullptr, rad0);
        window.LinkChild(nullptr, rad1);

        window.Open(state);
        TEST_REQUIRE(window.GetFocusedWidget(state) == rad0);
        TEST_REQUIRE(rad0->IsSelected() == true);
        TEST_REQUIRE(rad1->IsSelected() == false);

        uik::Window::KeyEvent event;
        event.key = uik::VirtualKey::MoveDown;
        event.time = 0.0;
        window.KeyDown(event, state);
        TEST_REQUIRE(rad0->IsSelected() == false);
        TEST_REQUIRE(rad1->IsSelected() == true);

        event.key = uik::VirtualKey::MoveUp;
        window.KeyDown(event, state);
        TEST_REQUIRE(rad0->IsSelected() == true);
        TEST_REQUIRE(rad1->IsSelected() == false);
    }

    // switch between two buttons when one is disabled.
    {
        uik::TransientState state;
        uik::Window window;
        window.EnableVirtualKeys(true);

        auto* rad0 = window.AddWidget(uik::RadioButton());
        auto* rad1 = window.AddWidget(uik::RadioButton());
        rad0->SetSelected(true);
        rad1->SetSelected(false);
        rad1->SetEnabled(false);
        window.LinkChild(nullptr, rad0);
        window.LinkChild(nullptr, rad1);

        window.Open(state);
        TEST_REQUIRE(window.GetFocusedWidget(state) == rad0);
        TEST_REQUIRE(rad0->IsSelected() == true);
        TEST_REQUIRE(rad1->IsSelected() == false);

        uik::Window::KeyEvent event;
        event.key = uik::VirtualKey::MoveDown;
        event.time = 0.0;
        window.KeyDown(event, state);
        TEST_REQUIRE(rad0->IsSelected() == true);
        TEST_REQUIRE(rad1->IsSelected() == false);

        event.key = uik::VirtualKey::MoveUp;
        window.KeyDown(event, state);
        TEST_REQUIRE(rad0->IsSelected() == true);
        TEST_REQUIRE(rad1->IsSelected() == false);
    }

    // switch between two buttons when one is hidden.
    {
        uik::TransientState state;
        uik::Window window;
        window.EnableVirtualKeys(true);

        auto* rad0 = window.AddWidget(uik::RadioButton());
        auto* rad1 = window.AddWidget(uik::RadioButton());
        rad0->SetSelected(true);
        rad1->SetSelected(false);
        rad1->SetVisible(false);
        window.LinkChild(nullptr, rad0);
        window.LinkChild(nullptr, rad1);

        window.Open(state);
        TEST_REQUIRE(window.GetFocusedWidget(state) == rad0);
        TEST_REQUIRE(rad0->IsSelected() == true);
        TEST_REQUIRE(rad1->IsSelected() == false);

        uik::Window::KeyEvent event;
        event.key = uik::VirtualKey::MoveDown;
        event.time = 0.0;
        window.KeyDown(event, state);
        TEST_REQUIRE(rad0->IsSelected() == true);
        TEST_REQUIRE(rad1->IsSelected() == false);

        event.key = uik::VirtualKey::MoveUp;
        window.KeyDown(event, state);
        TEST_REQUIRE(rad0->IsSelected() == true);
        TEST_REQUIRE(rad1->IsSelected() == false);
    }

    // bug, rapid move up/down in succession without the selection yet
    // being changed and with initial state being both buttons unselected.
    // (realizing the change in selection between auto exclusive radio
    // button group happens in PollAction)
    {
        uik::TransientState state;
        uik::Window window;
        window.EnableVirtualKeys(true);

        auto* rad0 = window.AddWidget(uik::RadioButton());
        auto* rad1 = window.AddWidget(uik::RadioButton());
        rad0->SetSelected(false);
        rad1->SetSelected(false);
        window.LinkChild(nullptr, rad0);
        window.LinkChild(nullptr, rad1);

        window.Open(state);
        TEST_REQUIRE(window.GetFocusedWidget(state) == rad0);
        TEST_REQUIRE(rad0->IsSelected() == false);
        TEST_REQUIRE(rad1->IsSelected() == false);

        uik::Window::KeyEvent event;
        event.key = uik::VirtualKey::MoveDown;
        event.time = 0.0;
        window.KeyDown(event, state);
        window.KeyDown(event, state);
        TEST_REQUIRE(rad0->IsSelected() == false);
        TEST_REQUIRE(rad1->IsSelected() == true);
    }
}

void unit_test_animation_parse()
{
    {
        constexpr const char* str = R"(
@foobar
0%
size 100.0 100.0
50%
size 200.0 50.0
100%
size 250.0 25.0

@something
0%
position 0.0 100.0
100%
position 20 50


$OnClick
resize 100.0 200.0
move 45.0 50.0
delay 1.0
duration 2.0
loops 5
interpolation Cosine

; this is a comment
$OnOpen
move 200.0 250.0
set prop float-prop 1.0
set prop color-prop0 1.0,0.0,0.0,1.0
set prop color-prop1 Red 1.0
set flag foo true
del prop foo

        )";

        std::vector<uik::Animation> animations;
        TEST_REQUIRE(uik::ParseAnimations(str, &animations));
        TEST_REQUIRE(animations.size() == 2);

        {
            const auto& animation = animations[0];
            TEST_REQUIRE(animation.GetDelay()         == 1.0f);
            TEST_REQUIRE(animation.GetDuration()      == 2.0f);
            TEST_REQUIRE(animation.GetInterpolation() == math::Interpolation::Cosine);
            TEST_REQUIRE(animation.GetTrigger()       == uik::Animation::Trigger::Click);
            TEST_REQUIRE(animation.GetLoops()         == 5);
            TEST_REQUIRE(animation.GetActionCount()   == 2);
            TEST_REQUIRE(animation.GetAction(0).type  == uik::Animation::Action::Type::Resize);
            TEST_REQUIRE(animation.GetAction(0).value == uik::FSize(100.0f, 200.0f));
            TEST_REQUIRE(animation.GetAction(1).type  == uik::Animation::Action::Type::Move);
            TEST_REQUIRE(animation.GetAction(1).value == uik::FPoint(45.0f, 50.0f));
        }

        {
            const auto& animation = animations[1];
            TEST_REQUIRE(animation.GetLoops()         == 1);
            TEST_REQUIRE(animation.GetDelay()         == 0.0f);
            TEST_REQUIRE(animation.GetDuration()      == 1.0f);
            TEST_REQUIRE(animation.GetInterpolation() == math::Interpolation::Linear);
            TEST_REQUIRE(animation.GetActionCount()   == 6);
            TEST_REQUIRE(animation.GetAction(0).type  == uik::Animation::Action::Type::Move);
            TEST_REQUIRE(animation.GetAction(1).type  == uik::Animation::Action::Type::SetProp);
            TEST_REQUIRE(animation.GetAction(1).key   == "float-prop");
            TEST_REQUIRE(animation.GetAction(1).value == uik::StyleProperty { 1.0f });
            TEST_REQUIRE(animation.GetAction(2).type  == uik::Animation::Action::Type::SetProp);
            TEST_REQUIRE(animation.GetAction(2).key   == "color-prop0");
            TEST_REQUIRE(animation.GetAction(2).value == uik::StyleProperty { uik::Color4f(1.0f, 0.0f, 0.0f, 1.0f) });
            TEST_REQUIRE(animation.GetAction(2).step  == 0.5f);
            TEST_REQUIRE(animation.GetAction(3).type  == uik::Animation::Action::Type::SetProp);
            TEST_REQUIRE(animation.GetAction(3).key   == "color-prop1");
            TEST_REQUIRE(animation.GetAction(3).value == uik::StyleProperty { uik::Color4f(uik::Color::Red) });
            TEST_REQUIRE(animation.GetAction(3).step  == 1.0f);
            TEST_REQUIRE(animation.GetAction(4).type  == uik::Animation::Action::Type::SetFlag);
            TEST_REQUIRE(animation.GetAction(4).key   == "foo");
            TEST_REQUIRE(animation.GetAction(4).value == true);
            TEST_REQUIRE(animation.GetAction(5).type  == uik::Animation::Action::Type::DelProp);
            TEST_REQUIRE(animation.GetAction(5).key   == "foo");
        }
    }

    {
        constexpr const char* str = R"(
asgasgga
        )";

        std::vector<uik::Animation> animations;
        TEST_REQUIRE(!uik::ParseAnimations(str, &animations));
    }

    {
        constexpr const char* str = R"(
$Foobar
move 100.0 200.0
        )";
        std::vector<uik::Animation> animations;
        TEST_REQUIRE(!uik::ParseAnimations(str, &animations));
    }

    {
        constexpr const char* str = R"(
$OnOpen
blergh 100.0 200.0
        )";
        std::vector<uik::Animation> animations;
        TEST_REQUIRE(!uik::ParseAnimations(str, &animations));
    }

    {
        constexpr const char* str = R"(
$OnOpen
move 100.0 xwg12
        )";
        std::vector<uik::Animation> animations;
        TEST_REQUIRE(!uik::ParseAnimations(str, &animations));
    }
}

void unit_test_widget_animation_on_open()
{
    TEST_CASE(test::Type::Feature)

    // check animation initial state when the animation first begins to execute
    {
        uik::Window window;
        uik::PushButton btn;
        btn.SetName("test");
        btn.SetPosition(uik::FPoint(10.0f, 10.0f));
        btn.SetSize(uik::FSize(10.0f, 10.0f));
        btn.SetAnimationString(R"(
$OnOpen
move 100.0 100.0
resize 100.0 100.0
delay 0.0
duration 1.0
loops 1

        )");

        auto* widget = window.AddWidget(btn);

        uik::TransientState state;
        uik::AnimationStateArray animations;
        window.Open(state, &animations);
        // the initial state is fetched from the widget when the animation begins to
        // execute the first time. i.e after it's become active (trigger has executed)
        // and any possible delay has been consumed.
        window.Update(state, 0.0, 0.5f, &animations);

        TEST_REQUIRE(animations.size() == 1);
        TEST_REQUIRE(animations[0].GetState() == uik::Animation::State::Active);
        TEST_REQUIRE(animations[0].GetTime() == 0.5f);
        TEST_REQUIRE(animations[0].GetDuration() == 1.0f);
        TEST_REQUIRE(animations[0].GetActionCount() == 2);
        TEST_REQUIRE(animations[0].GetAction(0).type == uik::Animation::Action::Type::Move);

        TEST_REQUIRE(widget->GetPosition() == uik::FPoint(55.0f, 55.0f));
        TEST_REQUIRE(widget->GetSize() == uik::FSize(55.0f, 55.0f));
    }

    // looping once without delay
    {
        uik::Window window;
        uik::PushButton btn;
        btn.SetName("test");
        btn.SetPosition(uik::FPoint(0.0f, 0.0f));
        btn.SetSize(uik::FSize(10.0f, 10.0f));
        btn.SetAnimationString(R"(
$OnOpen
move 100.0 100.0
duration 1.0
loops 1

        )");

        auto* widget = window.AddWidget(btn);

        uik::TransientState state;
        uik::AnimationStateArray animations;

        window.Open(state, &animations);
        TEST_REQUIRE(animations[0].GetState() == uik::Animation::State::Active);
        TEST_REQUIRE(animations[0].GetTime() == 0.0f);
        TEST_REQUIRE(animations[0].GetLoop() == 0);
        TEST_REQUIRE(widget->GetPosition() == uik::FPoint(0.0f, 0.0f));

        window.Update(state, 0.0, 0.5f, &animations);
        TEST_REQUIRE(animations[0].GetState() == uik::Animation::State::Active);
        TEST_REQUIRE(animations[0].GetTime() == 0.5f);
        TEST_REQUIRE(animations[0].GetLoop() == 0);
        TEST_REQUIRE(widget->GetPosition() == uik::FPoint(50.0f, 50.0f));

        window.Update(state, 0.5f, 0.5f, &animations);
        TEST_REQUIRE(animations[0].GetState() == uik::Animation::State::Inactive);
        TEST_REQUIRE(animations[0].GetTime() == 1.0f);
        TEST_REQUIRE(animations[0].GetLoop() == 1);
        TEST_REQUIRE(widget->GetPosition() == uik::FPoint(100.0f, 100.0f));
    }

    // looping twice without delay
    {
        uik::Window window;
        uik::PushButton btn;
        btn.SetName("test");
        btn.SetPosition(uik::FPoint(0.0f, 0.0f));
        btn.SetSize(uik::FSize(10.0f, 10.0f));
        btn.SetAnimationString(R"(
$OnOpen
move 100.0 100.0
duration 1.0
loops 2

        )");

        auto* widget = window.AddWidget(btn);

        uik::TransientState state;
        uik::AnimationStateArray animations;

        window.Open(state, &animations);
        TEST_REQUIRE(animations[0].GetState() == uik::Animation::State::Active);
        TEST_REQUIRE(animations[0].GetTime() == 0.0f);
        TEST_REQUIRE(animations[0].GetLoop() == 0);
        TEST_REQUIRE(widget->GetPosition() == uik::FPoint(0.0f, 0.0f));

        window.Update(state, 0.0, 0.5f, &animations);
        TEST_REQUIRE(animations[0].GetState() == uik::Animation::State::Active);
        TEST_REQUIRE(animations[0].GetTime() == 0.5f);
        TEST_REQUIRE(animations[0].GetLoop() == 0);
        TEST_REQUIRE(widget->GetPosition() == uik::FPoint(50.0f, 50.0f));

        window.Update(state, 0.5f, 0.5f, &animations);
        TEST_REQUIRE(animations[0].GetState() == uik::Animation::State::Active);
        TEST_REQUIRE(animations[0].GetTime() == 0.0f);
        TEST_REQUIRE(animations[0].GetLoop() == 1);
        TEST_REQUIRE(widget->GetPosition() == uik::FPoint(100.0f, 100.0f));

        window.Update(state, 1.0f, 0.5f, &animations);
        TEST_REQUIRE(animations[0].GetState() == uik::Animation::State::Active);
        TEST_REQUIRE(animations[0].GetTime() == 0.5f);
        TEST_REQUIRE(animations[0].GetLoop() == 1);
        TEST_REQUIRE(widget->GetPosition() == uik::FPoint(50.0f, 50.0f));

        window.Update(state, 1.5f, 0.5f, &animations);
        TEST_REQUIRE(animations[0].GetState() == uik::Animation::State::Inactive);
        TEST_REQUIRE(animations[0].GetTime() == 1.0f);
        TEST_REQUIRE(animations[0].GetLoop() == 2);
        TEST_REQUIRE(widget->GetPosition() == uik::FPoint(100.0f, 100.0f));
    }

    // loop once with delay
    {
        uik::Window window;
        uik::PushButton btn;
        btn.SetName("test");
        btn.SetPosition(uik::FPoint(10.0f, 10.0f));
        btn.SetSize(uik::FSize(10.0f, 10.0f));
        btn.SetAnimationString(R"(
$OnOpen
move 100.0 100.0
duration 1.0
delay 1.0
loops 1
        )");

        auto* widget = window.AddWidget(btn);

        uik::TransientState state;
        uik::AnimationStateArray animations;

        window.Open(state, &animations);
        TEST_REQUIRE(animations[0].GetState() == uik::Animation::State::Active);
        TEST_REQUIRE(animations[0].GetTime() == -1.0f);
        TEST_REQUIRE(widget->GetPosition() == uik::FPoint(10.0f, 10.0f));

        window.Update(state, 0.0f, 0.5f, &animations);
        TEST_REQUIRE(animations[0].GetState() == uik::Animation::State::Active);
        TEST_REQUIRE(animations[0].GetTime() == -0.5f);
        TEST_REQUIRE(widget->GetPosition() == uik::FPoint(10.0f, 10.0f));

        window.Update(state, 0.5f, 0.5f, &animations);
        TEST_REQUIRE(animations[0].GetState() == uik::Animation::State::Active);
        TEST_REQUIRE(animations[0].GetTime() == 0.0f);
        TEST_REQUIRE(widget->GetPosition() == uik::FPoint(10.0f, 10.0f));

        window.Update(state, 1.0f, 0.5f, &animations);
        TEST_REQUIRE(animations[0].GetState() == uik::Animation::State::Active);
        TEST_REQUIRE(animations[0].GetTime() == 0.5f);
        TEST_REQUIRE(widget->GetPosition() == uik::FPoint(55.0f, 55.0f));

        window.Update(state, 1.5f, 0.5f, &animations);
        TEST_REQUIRE(animations[0].GetState() == uik::Animation::State::Inactive);
        TEST_REQUIRE(animations[0].GetTime() == 1.0f);
        TEST_REQUIRE(widget->GetPosition() == uik::FPoint(100.0f, 100.0f));
    }
}

// Re-triggering animations under the same trigger while
// some animation is still running.
// For example if we have two animation sequences such as
//
// $OnMouseEnter
// do stuff
// duration 2
//
// $OnMouseEnter
// undo stuff
// delay 2
// duration 2
//
// And the idea is that the second sequence starts after the
// first one has finished and "undoes" the changes done by
// the first sequence the first sequence can't be started
// until the second one also has finished.
//
// Easiest fix is to simply not run any animation under any
// trigger if any other animation under the same trigger is
// still running.
void bug_restart_animation_too_soon()
{
    TEST_CASE(test::Type::Feature)

    uik::Window window;
    uik::PushButton btn;
    btn.SetName("test");
    btn.SetPosition(uik::FPoint(0.0f, 0.0f));
    btn.SetSize(uik::FSize(10.0f, 10.0f));
    btn.SetAnimationString(R"(
$OnMouseEnter
move 100.0 100.0
delay 0.0
duration 1.0

$OnMouseEnter
move 0.0 0.0
delay 2.0
duration 1.0
        )");

    auto* widget = window.AddWidget(btn);
    window.LinkChild(nullptr, widget);

    uik::TransientState state;
    uik::AnimationStateArray animations;
    window.Open(state, &animations);

    {
        uik::Window::MouseEvent mickey;
        mickey.window_mouse_pos = uik::FPoint(5.0f, 5.0f);
        auto actions = window.MouseMove(mickey, state);
        window.TriggerAnimations(actions, state, animations);

        // both are started
        TEST_REQUIRE(animations[0].GetState() == uik::Animation::State::Active);
        TEST_REQUIRE(animations[1].GetState() == uik::Animation::State::Active);

        mickey.window_mouse_pos = uik::FPoint(100.0f, 100.0f);
        window.MouseMove(mickey, state);
    }

    double time = 0.0f;

    window.Update(state, time, 0.5f, &animations);
    time += 0.5f;
    window.Update(state, time, 0.5f, &animations);
    time += 0.5f;
    window.Update(state, time, 0.5f, &animations);
    time += 0.5f;

    TEST_REQUIRE(animations[0].GetState() == uik::Animation::State::Inactive);
    TEST_REQUIRE(animations[1].GetState() == uik::Animation::State::Active);

    TEST_REQUIRE(widget->GetPosition() == uik::FPoint(100.0f, 100.0f));

    {
        uik::Window::MouseEvent mickey;
        mickey.window_mouse_pos = uik::FPoint(105.0f, 105.0f);
        auto actions = window.MouseMove(mickey, state);
        window.TriggerAnimations(actions, state, animations);

        // first sequence remains inactive
        TEST_REQUIRE(animations[0].GetState() == uik::Animation::State::Inactive);
        TEST_REQUIRE(animations[1].GetState() == uik::Animation::State::Active);

        mickey.window_mouse_pos = uik::FPoint(0.0f, 0.0f);
        window.MouseMove(mickey, state);
    }

    while (animations[1].GetState() == uik::Animation::State::Active)
    {
        window.Update(state, time, 0.5f, &animations);
        time += 0.5f;
    }
    TEST_REQUIRE(widget->GetPosition() == uik::FPoint(0.0f, 0.0f));
    TEST_REQUIRE(animations[0].GetState() == uik::Animation::State::Inactive);
    TEST_REQUIRE(animations[1].GetState() == uik::Animation::State::Inactive);

    // now should be ok to trigger again.
    {
        uik::Window::MouseEvent mickey;
        mickey.window_mouse_pos = uik::FPoint(5.0f, 5.0f);
        auto actions = window.MouseMove(mickey, state);
        window.TriggerAnimations(actions, state, animations);

        // both are started
        TEST_REQUIRE(animations[0].GetState() == uik::Animation::State::Active);
        TEST_REQUIRE(animations[1].GetState() == uik::Animation::State::Active);
    }

}

void unit_test_find_widget_rect()
{
    TEST_CASE(test::Type::Feature)

    {
        uik::Window win;

        uik::Form form;
        form.SetName("form");
        form.SetPosition(25.0f, 20.0f);
        form.SetSize(100.0f, 100.0f);

        uik::Label label;
        label.SetName("label");
        label.SetSize(50.0f, 50.0f);
        label.SetPosition(10.0f, 10.0f);

        auto* f = win.AddWidget(form);
        win.LinkChild(nullptr, f);

        auto* l = win.AddWidget(label);
        win.LinkChild(f, l);

        auto rect = win.FindWidgetRect(f);
        TEST_REQUIRE(rect.GetPosition() == uik::FPoint(25.0f, 20.0f));
        TEST_REQUIRE(rect.GetSize() == uik::FSize(100.0f, 100.0f));

        rect = win.FindWidgetRect(l);
        TEST_REQUIRE(rect.GetPosition() == uik::FPoint(35.0f, 30.0f));
        TEST_REQUIRE(rect.GetSize() == uik::FSize(50.0f, 50.0f));

        f->SetPosition(0.0f, 0.0f);
        f->SetSize(100.0f, 100.0f);

        rect = win.FindWidgetRect(f);
        TEST_REQUIRE(rect.GetPosition() == uik::FPoint(0.0f, 0.0f));
        TEST_REQUIRE(rect.GetSize() == uik::FSize(100.0f, 100.0f));

        rect = win.FindWidgetRect(l);
        TEST_REQUIRE(rect.GetPosition() == uik::FPoint(10.0f, 10.0f));
        TEST_REQUIRE(rect.GetSize() == uik::FSize(50.0f, 50.0f));


        l->SetPosition(100.0f, 100.0f);
        l->SetSize(50.0f, 50.0f);

        rect = win.FindWidgetRect(f);
        TEST_REQUIRE(rect.GetPosition() == uik::FPoint(0.0f, 0.0f));
        TEST_REQUIRE(rect.GetSize() == uik::FSize(100.0f, 100.0f));

        rect = win.FindWidgetRect(f, uik::Window::FindRectFlags::IncludeChildren);
        TEST_REQUIRE(rect.GetPosition() == uik::FPoint(0.0f, 0.0f));
        TEST_REQUIRE(rect.GetSize() == uik::FSize(150.0f, 150.0f));

        rect = win.FindWidgetRect(f, uik::Window::FindRectFlags::IncludeChildren |
                                     uik::Window::FindRectFlags::ClipChildren);
        TEST_REQUIRE(rect.GetPosition() == uik::FPoint(0.0f, 0.0f));
        TEST_REQUIRE(rect.GetSize() == uik::FSize(100.0f, 100.0f));


        l->SetVisible(false);

        rect = win.FindWidgetRect(f, uik::Window::FindRectFlags::IncludeChildren |
                                     uik::Window::FindRectFlags::ExcludeHidden);
        TEST_REQUIRE(rect.GetPosition() == uik::FPoint(0.0f, 0.0f));
        TEST_REQUIRE(rect.GetSize() == uik::FSize(100.0f, 100.0f));
    }
}

// looping state and start state are stale on restart (re-trigger)
void bug_incorrect_state_on_restart()
{
    TEST_CASE(test::Type::Feature)

    uik::Window window;
    uik::PushButton btn;
    btn.SetName("test");
    btn.SetPosition(uik::FPoint(0.0f, 0.0f));
    btn.SetSize(uik::FSize(10.0f, 10.0f));
    btn.SetAnimationString(R"(
$OnClick
move 100.0 100.0
delay 0.0
duration 1.0
loops 2
        )");

    auto* widget = window.AddWidget(btn);
    window.LinkChild(nullptr, widget);

    uik::TransientState state;
    uik::AnimationStateArray animations;
    window.Open(state, &animations);

    {
        uik::Window::MouseEvent press;
        press.window_mouse_pos = uik::FPoint(5.0f, 5.0f);
        press.button = uik::MouseButton::Left;
        press.time   = base::GetTime();
        window.TriggerAnimations(window.MousePress(press, state),
                                 state, animations);
        window.TriggerAnimations(window.MouseRelease(press, state),
                state, animations);

        TEST_REQUIRE(animations[0].GetState() == uik::Animation::State::Active);
    }

    {
        window.Update(state, 0.0f, 1.0f, &animations);
        TEST_REQUIRE(animations[0].GetState() == uik::Animation::State::Active);
        TEST_REQUIRE(animations[0].GetLoop() == 1);

        window.Update(state, 1.0f, 0.5f, &animations);
        TEST_REQUIRE(animations[0].GetState() == uik::Animation::State::Active);
        TEST_REQUIRE(animations[0].GetLoop()  == 1);
        TEST_REQUIRE(widget->GetPosition()    == uik::FPoint(50.0f, 50.0f));

        window.Update(state, 1.5f, 0.5f, &animations);
        TEST_REQUIRE(animations[0].GetState() == uik::Animation::State::Inactive);
        TEST_REQUIRE(animations[0].GetLoop()  == 2);
        TEST_REQUIRE(widget->GetPosition()    == uik::FPoint(100.0f, 100.0f));
    }

    // trigger again.
    {
        uik::Window::MouseEvent press;
        press.window_mouse_pos = uik::FPoint(105.0f, 105.0f);
        press.button = uik::MouseButton::Left;
        press.time   = base::GetTime();
        window.TriggerAnimations(window.MousePress(press, state),
                                 state, animations);
        window.TriggerAnimations(window.MouseRelease(press, state),
                                 state, animations);

        TEST_REQUIRE(animations[0].GetState() == uik::Animation::State::Active);
        TEST_REQUIRE(animations[0].GetLoop() == 0);
    }

    double time = 1.5f;
    // note that since we're now starting from the position
    // that was the ending position of the animation on previous
    // trigger there will actually not be any movement.
    // so we're checking on *not* moving here.
    while (animations[0].GetState() == uik::Animation::State::Active)
    {
        window.Update(state, time, 0.5f, &animations);
        time += 0.5f;
    }
    TEST_REQUIRE(widget->GetPosition() == uik::FPoint(100.0f, 100.0f));
}

void bug_clipmask_when_parent_invisible()
{
    // clipmask pop is not matched correctly when the parent is actually not visible.

    TEST_CASE(test::Type::Feature)

    uik::Window window;

    {
        uik::Form  form;
        form.SetName("form");
        form.SetSize(100.0f, 100.0f);
        form.SetVisible(false);
        window.AddWidget(std::move(form));
        window.LinkChild(nullptr, window.FindWidgetByName("form"));
    }

    {
        uik::PushButton btn;
        btn.SetName("button");
        window.AddWidget(std::move(btn));
        window.LinkChild(window.FindWidgetByName("form"),
                         window.FindWidgetByName("button"));

    }

    uik::TransientState state;

    Painter p;

    window.Paint(state, p);

}

EXPORT_TEST_MAIN(
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
    unit_test_keyboard_focus();
    unit_test_keyboard_radiobutton_select();
    unit_test_animation_parse();
    unit_test_widget_animation_on_open();
    unit_test_find_widget_rect();

    bug_restart_animation_too_soon();
    bug_incorrect_state_on_restart();
    bug_clipmask_when_parent_invisible();
    return 0;
}
) // TEST_MAIN
