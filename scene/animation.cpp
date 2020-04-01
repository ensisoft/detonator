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

#include "base/logging.h"
#include "base/assert.h"
#include "graphics/drawable.h"
#include "graphics/material.h"
#include "graphics/drawing.h"
#include "graphics/painter.h"
#include "graphics/transform.h"
#include "animation.h"
#include "gfxfactory.h"

namespace scene
{

void Animation::Component::Draw(gfx::Painter& painter, gfx::Transform& transform) const
{
    if (!mDrawable || !mMaterial)
        return;

    mMaterial->SetRuntime(mTime - mStartTime);


    transform.Translate(mPosition);

    painter.Draw(*mDrawable, transform, *mMaterial);

}

bool Animation::Component::Update(float dt)
{
    mTime += dt;
    if (mTime < mStartTime)
        return true;
    if (mTime - mStartTime > mLifetime)
        return false;

    if (mDrawable)
        mDrawable->Update(dt);

    return true;
}

void Animation::Component::Reset()
{
    mTime = 0.0f;

}

void Animation::Draw(gfx::Painter& painter, gfx::Transform& transform) const
{
    for (const auto& component : mComponents)
    {
        transform.Push();
        component.Draw(painter, transform);
        transform.Pop();
    }
}

bool Animation::Update(float dt)
{
    bool alive = false;

    for (auto& component : mComponents)
    {
        if (component.Update(dt))
            alive = true;
    }
    return alive;
}

bool Animation::IsExpired() const
{
    return false;
}

void Animation::Reset()
{
    for (auto& component : mComponents)
    {
        component.Reset();
    }
}

} // namespace
