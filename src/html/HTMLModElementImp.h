/*
 * Copyright 2010-2013 Esrille Inc.
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

#ifndef ORG_W3C_DOM_BOOTSTRAP_HTMLMODELEMENTIMP_H_INCLUDED
#define ORG_W3C_DOM_BOOTSTRAP_HTMLMODELEMENTIMP_H_INCLUDED

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <org/w3c/dom/html/HTMLModElement.h>
#include "HTMLElementImp.h"

#include <org/w3c/dom/html/HTMLElement.h>

namespace org
{
namespace w3c
{
namespace dom
{
namespace bootstrap
{
class HTMLModElementImp : public ObjectMixin<HTMLModElementImp, HTMLElementImp>
{
public:
    HTMLModElementImp(DocumentImp* ownerDocument, const std::u16string& localName) :
        ObjectMixin(ownerDocument, localName)
    {
    }

    // Node - override
    virtual Node cloneNode(bool deep = true) {
        auto node = std::make_shared<HTMLModElementImp>(*this);
        node->cloneAttributes(this);
        if (deep)
            node->cloneChildren(this);
        return node;
    }

    // HTMLModElement
    std::u16string getCite();
    void setCite(const std::u16string& cite);
    std::u16string getDateTime();
    void setDateTime(const std::u16string& dateTime);
    // Object
    virtual Any message_(uint32_t selector, const char* id, int argc, Any* argv)
    {
        return html::HTMLModElement::dispatch(this, selector, id, argc, argv);
    }
    static const char* const getMetaData()
    {
        return html::HTMLModElement::getMetaData();
    }
};

}
}
}
}

#endif  // ORG_W3C_DOM_BOOTSTRAP_HTMLMODELEMENTIMP_H_INCLUDED
