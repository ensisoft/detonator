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

#pragma once

#include "config.h"

#include <string>

#include "uikit/types.h"

namespace uik
{
    // Painter interface allows one to plug different widget paint
    // implementations into the ui system and allows the rest of
    // the system to stay abstract without having to know details
    // about actual paint operations and how they're implemented.
    // This simplifies for example the testing of widgets when a
    // simple test jig painter can be used to verify the inputs from
    // widgets based on their current expected state.
    class Painter
    {
    public:
        using WidgetId = std::string;
        using WidgetClass = std::string;
        // Combine some details about the widget's current
        // state for possibly altering the painting operations
        // in some way, by for example applying different
        // styling to the widget.
        struct PaintStruct {
            // The widget "klass" name specific to each type of widget.
            // For example "pushbutton" or "label". See widgets.cpp for
            // the actual widget type names.
            std::string klass;
            // Indicates whether widget/item is currently enabled or not.
            // when enabled is false pressed/focused/moused will also
            // be false since only enabled widget can have one of those
            // substates (and can rect to user input).
            bool enabled = true;
            // Indicates whether the widget/item is currently pressed.
            // pressed state occurs for example when a push button is
            // being pressed. i.e. the button is in pressed state
            // between mouse down/press and mouse up/release events.
            bool pressed = false;
            // Indicates whether the widget/item is currently focused, i.e.
            // has the *keyboard* focus or not.
            bool focused = false;
            // Indicates whether the widget/item is currently being "moused" i.e.
            // the mouse is over it or interacting with it.
            bool moused  = false;
            // Current time of the paint operation. todo: is this useful, remove?
            double time  = 0.0;
            // The widget's/item's rectangle relative to the window. The painter impls
            // must then map this rect to some shape with some area and location in
            // the actual render target / render surface.
            FRect rect;
            // Set the current clipping rectangle to restrict painter operations so
            // that only the pixels/content within the given rect will be affected.
            // This is used when widgets need to be clipped to stay within the insides
            // of their containing widget. For example when some container widget has
            // child widgets the child widgets are only shown for the part that is within
            // the container. Any extent that goes outside of this rect needs to be clipped.
            FRect clip;
        };
        virtual ~Painter() = default;

        // Each of the painting operations take the ID of the widget/window
        // in question. The ID is unique and thus be used to identify specific
        // widgets for having their styling/painting properties altered in some way.
        // Each widget can contain conceptually some sub-components such as up/down
        // buttons, text strings or item text strings.
        // For example a spinbox widget might have up/down buttons to change
        // the value. A combobox might only have a down button for opening the widget
        // and once it's open it'll show a drop down list with several down down items.
        // The widget paint operations are thus broken down into these primitive operations
        // which the widget implementations can then use to compose the widget.

        // Draw the widget background if any.
        virtual void DrawWidgetBackground(const WidgetId& id, const PaintStruct& ps) const = 0;
        // Draw the widget border if any.
        virtual void DrawWidgetBorder(const WidgetId& id, const PaintStruct& ps) const = 0;
        // Draw the widget text. This is used for texts such as labels, button texts, or
        // texts that are part of the widget's "static" interface. Widget items such as
        // combobox dropdown items or list box items are drawn using separate functionality.
        virtual void DrawWidgetText(const WidgetId& id, const PaintStruct& ps, const std::string& text, float line_height) const = 0;
        // Draw a focus rect to indicate that the widget has focus. This is a generic
        // "one" shot impl alternative to using the PaintStruct::focused flag. A painter
        // implementation can support either one (or even nothing).
        virtual void DrawFocusRect(const WidgetId& id, const PaintStruct& ps) const = 0;
        // Draw a checkbox.
        virtual void DrawCheckBox(const WidgetId& id, const PaintStruct& ps, bool checked) const = 0;

        enum class Button {
            ArrowUp, ArrowDown
        };
        virtual void DrawButton(const WidgetId& id, const PaintStruct& ps, Button btn) const = 0;

        virtual void DrawProgressBar(const WidgetId&, const PaintStruct& ps, float percentagen) const = 0;

        // todo: more sub widget draw ops.

        // Parse the style string associated with a widget. The painter can use
        // this information to realize widget specific styling and property attributes.
        // Should return true if styling was successfully parsed and understood or
        // false to indicate an error.
        virtual bool ParseStyle(const WidgetId& id, const std::string& style) = 0;
    private:
    };

} // namespace
