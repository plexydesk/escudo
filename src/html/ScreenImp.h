/*
 * Copyright 2013 Esrille Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ORG_W3C_DOM_BOOTSTRAP_SCREENIMP_H_INCLUDED
#define ORG_W3C_DOM_BOOTSTRAP_SCREENIMP_H_INCLUDED

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <org/w3c/dom/html/Screen.h>

namespace org
{
namespace w3c
{
namespace dom
{
namespace bootstrap
{
class ScreenImp : public ObjectMixin<ScreenImp>
{
public:
    // For Media Queries
    unsigned int getDPI() const;
    unsigned int getDPPX() const;
    unsigned int getColor() const;
    unsigned int getColorIndex() const;
    unsigned int getMonochrome() const;
    unsigned int getScan() const;
    unsigned int getGrid() const;

    // Screen
    unsigned int getAvailWidth();
    unsigned int getAvailHeight();
    unsigned int getWidth();
    unsigned int getHeight();
    unsigned int getColorDepth();
    unsigned int getPixelDepth();
    // Object
    virtual Any message_(uint32_t selector, const char* id, int argc, Any* argv)
    {
        return html::Screen::dispatch(this, selector, id, argc, argv);
    }
    static const char* const getMetaData()
    {
        return html::Screen::getMetaData();
    }
};

typedef std::shared_ptr<ScreenImp> ScreenPtr;

}
}
}
}

#endif  // ORG_W3C_DOM_BOOTSTRAP_SCREENIMP_H_INCLUDED
