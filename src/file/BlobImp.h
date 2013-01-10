// Generated by esidl 0.3.0.
// This file is expected to be modified for the Web IDL interface
// implementation.  Permission to use, copy, modify and distribute
// this file in any software license is hereby granted.

#ifndef ORG_W3C_DOM_BOOTSTRAP_BLOBIMP_H_INCLUDED
#define ORG_W3C_DOM_BOOTSTRAP_BLOBIMP_H_INCLUDED

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <org/w3c/dom/file/Blob.h>

#include <org/w3c/dom/file/Blob.h>
#include <org/w3c/dom/file/BlobPropertyBag.h>
#include <org/w3c/dom/ArrayBuffer.h>
#include <org/w3c/dom/ArrayBufferView.h>

namespace org
{
namespace w3c
{
namespace dom
{
namespace bootstrap
{
class BlobImp : public ObjectMixin<BlobImp>
{
public:
    // Blob
    unsigned long long getSize();
    std::u16string getType();
    file::Blob slice();
    file::Blob slice(long long start);
    file::Blob slice(long long start, long long end);
    file::Blob slice(long long start, long long end, const std::u16string& contentType);
    void close();
    // Object
    virtual Any message_(uint32_t selector, const char* id, int argc, Any* argv)
    {
        return file::Blob::dispatch(this, selector, id, argc, argv);
    }
    static const char* const getMetaData()
    {
        return file::Blob::getMetaData();
    }
};

}
}
}
}

#endif  // ORG_W3C_DOM_BOOTSTRAP_BLOBIMP_H_INCLUDED