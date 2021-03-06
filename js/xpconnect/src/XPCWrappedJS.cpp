/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Class that wraps JS objects to appear as XPCOM objects. */

#include "xpcprivate.h"
#include "jsprf.h"
#include "nsCCUncollectableMarker.h"
#include "nsCxPusher.h"
#include "nsContentUtils.h"
#include "nsThreadUtils.h"

using namespace mozilla;

// NOTE: much of the fancy footwork is done in xpcstubs.cpp


// nsXPCWrappedJS lifetime.
//
// An nsXPCWrappedJS is either rooting its JS object or is subject to finalization.
// The subject-to-finalization state lets wrappers support
// nsSupportsWeakReference in the case where the underlying JS object
// is strongly owned, but the wrapper itself is only weakly owned.
//
// A wrapper is rooting its JS object whenever its refcount is greater than 1. In
// this state, root wrappers are always added to the cycle collector graph. The
// wrapper keeps around an extra refcount, added in the constructor, to support
// the possibility of an eventual transition to the subject-to-finalization state.
// This extra refcount is ignored by the cycle collector, which traverses the "self"
// edge for this refcount.
//
// When the refcount of a rooting wrapper drops to 1, if there is no weak reference
// to the wrapper (which can only happen for the root wrapper), it is immediately
// Destroy()'d. Otherwise, it becomes subject to finalization.
//
// When a wrapper is subject to finalization, the wrapper has a refcount of 1. It is
// now owned exclusively by its JS object. Either a weak reference will be turned into
// a strong ref which will bring its refcount up to 2 and change the wrapper back to
// the rooting state, or it will stay alive until the JS object dies. If the JS object
// dies, then when XPCJSRuntime::FinalizeCallback calls FindDyingJSObjects
// it will find the wrapper and call Release() in it, destroying the wrapper.
// Otherwise, the wrapper will stay alive, even if it no longer has a weak reference
// to it.
//
// When the wrapper is subject to finalization, it is kept alive by an implicit reference
// from the JS object which is invisible to the cycle collector, so the cycle collector
// does not traverse any children of wrappers that are subject to finalization. This will
// result in a leak if a wrapper in the non-rooting state has an aggregated native that
// keeps alive the wrapper's JS object.  See bug 947049.


// If traversing wrappedJS wouldn't release it, nor cause any other objects to be
// added to the graph, there is no need to add it to the graph at all.
bool
nsXPCWrappedJS::CanSkip()
{
    if (!nsCCUncollectableMarker::sGeneration)
        return false;

    if (IsSubjectToFinalization())
        return true;

    // If this wrapper holds a gray object, need to trace it.
    JSObject *obj = GetJSObjectPreserveColor();
    if (obj && xpc_IsGrayGCThing(obj))
        return false;

    // For non-root wrappers, check if the root wrapper will be
    // added to the CC graph.
    if (!IsRootWrapper())
        return mRoot->CanSkip();

    // For the root wrapper, check if there is an aggregated
    // native object that will be added to the CC graph.
    if (!IsAggregatedToNative())
        return true;

    nsISupports* agg = GetAggregatedNativeObject();
    nsXPCOMCycleCollectionParticipant* cp = nullptr;
    CallQueryInterface(agg, &cp);
    nsISupports* canonical = nullptr;
    agg->QueryInterface(NS_GET_IID(nsCycleCollectionISupports),
                        reinterpret_cast<void**>(&canonical));
    return cp && canonical && cp->CanSkipThis(canonical);
}

NS_IMETHODIMP
NS_CYCLE_COLLECTION_CLASSNAME(nsXPCWrappedJS)::Traverse
   (void *p, nsCycleCollectionTraversalCallback &cb)
{
    nsISupports *s = static_cast<nsISupports*>(p);
    MOZ_ASSERT(CheckForRightISupports(s), "not the nsISupports pointer we expect");
    nsXPCWrappedJS *tmp = Downcast(s);

    nsrefcnt refcnt = tmp->mRefCnt.get();
    if (cb.WantDebugInfo()) {
        char name[72];
        if (tmp->GetClass())
            JS_snprintf(name, sizeof(name), "nsXPCWrappedJS (%s)",
                        tmp->GetClass()->GetInterfaceName());
        else
            JS_snprintf(name, sizeof(name), "nsXPCWrappedJS");
        cb.DescribeRefCountedNode(refcnt, name);
    } else {
        NS_IMPL_CYCLE_COLLECTION_DESCRIBE(nsXPCWrappedJS, refcnt)
    }

    // A wrapper that is subject to finalization will only die when its JS object dies.
    if (tmp->IsSubjectToFinalization())
        return NS_OK;

    // Don't let the extra reference for nsSupportsWeakReference keep a wrapper that is
    // not subject to finalization alive.
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "self");
    cb.NoteXPCOMChild(s);

    if (tmp->IsValid()) {
        MOZ_ASSERT(refcnt > 1);
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mJSObj");
        cb.NoteJSChild(tmp->GetJSObjectPreserveColor());
    }

    if (tmp->IsRootWrapper()) {
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "aggregated native");
        cb.NoteXPCOMChild(tmp->GetAggregatedNativeObject());
    } else {
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "root");
        cb.NoteXPCOMChild(ToSupports(tmp->GetRootWrapper()));
    }

    return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXPCWrappedJS)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsXPCWrappedJS)
    tmp->Unlink();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

// XPCJSRuntime keeps a table of WJS, so we can remove them from
// the purple buffer in between CCs.
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsXPCWrappedJS)
    return true;
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsXPCWrappedJS)
    return tmp->CanSkip();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsXPCWrappedJS)
    return tmp->CanSkip();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_IMETHODIMP
nsXPCWrappedJS::AggregatedQueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    MOZ_ASSERT(IsAggregatedToNative(), "bad AggregatedQueryInterface call");

    if (!IsValid())
        return NS_ERROR_UNEXPECTED;

    // Put this here rather that in DelegatedQueryInterface because it needs
    // to be in QueryInterface before the possible delegation to 'outer', but
    // we don't want to do this check twice in one call in the normal case:
    // once in QueryInterface and once in DelegatedQueryInterface.
    if (aIID.Equals(NS_GET_IID(nsIXPConnectWrappedJS))) {
        NS_ADDREF(this);
        *aInstancePtr = (void*) static_cast<nsIXPConnectWrappedJS*>(this);
        return NS_OK;
    }

    return mClass->DelegatedQueryInterface(this, aIID, aInstancePtr);
}

NS_IMETHODIMP
nsXPCWrappedJS::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (nullptr == aInstancePtr) {
        NS_PRECONDITION(0, "null pointer");
        return NS_ERROR_NULL_POINTER;
    }

    if ( aIID.Equals(NS_GET_IID(nsXPCOMCycleCollectionParticipant)) ) {
        *aInstancePtr = NS_CYCLE_COLLECTION_PARTICIPANT(nsXPCWrappedJS);
        return NS_OK;
    }

    if (aIID.Equals(NS_GET_IID(nsCycleCollectionISupports))) {
        *aInstancePtr =
            NS_CYCLE_COLLECTION_CLASSNAME(nsXPCWrappedJS)::Upcast(this);
        return NS_OK;
    }

    if (!IsValid())
        return NS_ERROR_UNEXPECTED;

    // Always check for this first so that our 'outer' can get this interface
    // from us without recurring into a call to the outer's QI!
    if (aIID.Equals(NS_GET_IID(nsIXPConnectWrappedJS))) {
        NS_ADDREF(this);
        *aInstancePtr = (void*) static_cast<nsIXPConnectWrappedJS*>(this);
        return NS_OK;
    }

    nsISupports* outer = GetAggregatedNativeObject();
    if (outer)
        return outer->QueryInterface(aIID, aInstancePtr);

    // else...

    return mClass->DelegatedQueryInterface(this, aIID, aInstancePtr);
}


// For a description of nsXPCWrappedJS lifetime and reference counting, see
// the comment at the top of this file.

nsrefcnt
nsXPCWrappedJS::AddRef(void)
{
    if (!MOZ_LIKELY(NS_IsMainThread()))
        MOZ_CRASH();

    MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");
    nsISupports *base = NS_CYCLE_COLLECTION_CLASSNAME(nsXPCWrappedJS)::Upcast(this);
    nsrefcnt cnt = mRefCnt.incr(base);
    NS_LOG_ADDREF(this, cnt, "nsXPCWrappedJS", sizeof(*this));

    if (2 == cnt && IsValid()) {
        GetJSObject(); // Unmark gray JSObject.
        mClass->GetRuntime()->AddWrappedJSRoot(this);
    }

    return cnt;
}

nsrefcnt
nsXPCWrappedJS::Release(void)
{
    if (!MOZ_LIKELY(NS_IsMainThread()))
        MOZ_CRASH();
    MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");
    NS_ASSERT_OWNINGTHREAD(nsXPCWrappedJS);

    bool shouldDelete = false;
    nsISupports *base = NS_CYCLE_COLLECTION_CLASSNAME(nsXPCWrappedJS)::Upcast(this);
    nsrefcnt cnt = mRefCnt.decr(base, &shouldDelete);
    NS_LOG_RELEASE(this, cnt, "nsXPCWrappedJS");

    if (0 == cnt) {
        if (MOZ_UNLIKELY(shouldDelete)) {
            mRefCnt.stabilizeForDeletion();
            DeleteCycleCollectable();
        } else {
            mRefCnt.incr(base);
            Destroy();
            mRefCnt.decr(base);
        }
    } else if (1 == cnt) {
        if (IsValid())
            RemoveFromRootSet();

        // If we are not a root wrapper being used from a weak reference,
        // then the extra ref is not needed and we can let outselves be
        // deleted.
        if (!HasWeakReferences())
            return Release();

        MOZ_ASSERT(IsRootWrapper(), "Only root wrappers should have weak references");
    }
    return cnt;
}

NS_IMETHODIMP_(void)
nsXPCWrappedJS::DeleteCycleCollectable(void)
{
    delete this;
}

void
nsXPCWrappedJS::TraceJS(JSTracer* trc)
{
    MOZ_ASSERT(mRefCnt >= 2 && IsValid(), "must be strongly referenced");
    JS_SET_TRACING_DETAILS(trc, GetTraceName, this, 0);
    JS_CallHeapObjectTracer(trc, &mJSObj, "nsXPCWrappedJS::mJSObj");
}

// static
void
nsXPCWrappedJS::GetTraceName(JSTracer* trc, char *buf, size_t bufsize)
{
    const nsXPCWrappedJS* self = static_cast<const nsXPCWrappedJS*>
                                            (trc->debugPrintArg);
    JS_snprintf(buf, bufsize, "nsXPCWrappedJS[%s,0x%p:0x%p].mJSObj",
                self->GetClass()->GetInterfaceName(), self, self->mXPTCStub);
}

NS_IMETHODIMP
nsXPCWrappedJS::GetWeakReference(nsIWeakReference** aInstancePtr)
{
    if (!IsRootWrapper())
        return mRoot->GetWeakReference(aInstancePtr);

    return nsSupportsWeakReference::GetWeakReference(aInstancePtr);
}

JSObject*
nsXPCWrappedJS::GetJSObject()
{
    if (mJSObj) {
        JS::ExposeObjectToActiveJS(mJSObj);
    }
    return mJSObj;
}

// static
nsresult
nsXPCWrappedJS::GetNewOrUsed(JS::HandleObject jsObj,
                             REFNSIID aIID,
                             nsXPCWrappedJS** wrapperResult)
{
    // Do a release-mode assert against accessing nsXPCWrappedJS off-main-thread.
    if (!MOZ_LIKELY(NS_IsMainThread()))
        MOZ_CRASH();

    AutoJSContext cx;
    JSObject2WrappedJSMap* map;
    nsXPCWrappedJS* root = nullptr;
    nsXPCWrappedJS* wrapper = nullptr;
    nsXPCWrappedJSClass* clazz = nullptr;
    XPCJSRuntime* rt = nsXPConnect::GetRuntimeInstance();
    bool release_root = false;

    map = rt->GetWrappedJSMap();
    if (!map) {
        MOZ_ASSERT(map,"bad map");
        return NS_ERROR_FAILURE;
    }

    nsXPCWrappedJSClass::GetNewOrUsed(cx, aIID, &clazz);
    if (!clazz)
        return NS_ERROR_FAILURE;
    // from here on we need to return through 'return_wrapper'

    // always find the root JSObject
    JS::RootedObject rootJSObj(cx, clazz->GetRootJSObject(cx, jsObj));
    if (!rootJSObj)
        goto return_wrapper;

    root = map->Find(rootJSObj);
    if (root) {
        if ((nullptr != (wrapper = root->Find(aIID))) ||
            (nullptr != (wrapper = root->FindInherited(aIID)))) {
            NS_ADDREF(wrapper);
            goto return_wrapper;
        }
    }

    if (!root) {
        // build the root wrapper
        if (rootJSObj == jsObj) {
            // the root will do double duty as the interface wrapper
            wrapper = root = new nsXPCWrappedJS(cx, jsObj, clazz, nullptr);
            if (!root)
                goto return_wrapper;

            map->Add(cx, root);

            goto return_wrapper;
        } else {
            // just a root wrapper
            nsXPCWrappedJSClass* rootClazz = nullptr;
            nsXPCWrappedJSClass::GetNewOrUsed(cx, NS_GET_IID(nsISupports),
                                              &rootClazz);
            if (!rootClazz)
                goto return_wrapper;

            root = new nsXPCWrappedJS(cx, rootJSObj, rootClazz, nullptr);
            NS_RELEASE(rootClazz);

            if (!root)
                goto return_wrapper;

            release_root = true;

            map->Add(cx, root);
        }
    }

    // at this point we have a root and may need to build the specific wrapper
    MOZ_ASSERT(root,"bad root");
    MOZ_ASSERT(clazz,"bad clazz");

    if (!wrapper) {
        wrapper = new nsXPCWrappedJS(cx, jsObj, clazz, root);
        if (!wrapper)
            goto return_wrapper;
    }

    wrapper->mNext = root->mNext;
    root->mNext = wrapper;

return_wrapper:
    if (clazz)
        NS_RELEASE(clazz);

    if (release_root)
        NS_RELEASE(root);

    if (!wrapper)
        return NS_ERROR_FAILURE;

    *wrapperResult = wrapper;
    return NS_OK;
}

nsXPCWrappedJS::nsXPCWrappedJS(JSContext* cx,
                               JSObject* aJSObj,
                               nsXPCWrappedJSClass* aClass,
                               nsXPCWrappedJS* root)
    : mJSObj(aJSObj),
      mClass(aClass),
      mRoot(root ? root : MOZ_THIS_IN_INITIALIZER_LIST()),
      mNext(nullptr),
      mOuter(nullptr)
{
    InitStub(GetClass()->GetIID());

    // There is an extra AddRef to support weak references to wrappers
    // that are subject to finalization. See the top of the file for more
    // details.
    NS_ADDREF_THIS();
    NS_ADDREF_THIS();

    NS_ADDREF(aClass);

    if (!IsRootWrapper())
        NS_ADDREF(mRoot);

}

nsXPCWrappedJS::~nsXPCWrappedJS()
{
    Destroy();
}

void
nsXPCWrappedJS::Destroy()
{
    MOZ_ASSERT(1 == int32_t(mRefCnt), "should be stabilized for deletion");

    if (IsRootWrapper()) {
        XPCJSRuntime* rt = nsXPConnect::GetRuntimeInstance();
        JSObject2WrappedJSMap* map = rt->GetWrappedJSMap();
        if (map)
            map->Remove(this);
    }
    Unlink();
}

void
nsXPCWrappedJS::Unlink()
{
    if (IsValid()) {
        XPCJSRuntime* rt = nsXPConnect::GetRuntimeInstance();
        if (rt) {
            if (IsRootWrapper()) {
                JSObject2WrappedJSMap* map = rt->GetWrappedJSMap();
                if (map)
                    map->Remove(this);
            }

            if (mRefCnt > 1)
                RemoveFromRootSet();
        }

        mJSObj = nullptr;
    }

    if (IsRootWrapper()) {
        ClearWeakReferences();
    } else if (mRoot) {
        // unlink this wrapper
        nsXPCWrappedJS* cur = mRoot;
        while (1) {
            if (cur->mNext == this) {
                cur->mNext = mNext;
                break;
            }
            cur = cur->mNext;
            MOZ_ASSERT(cur, "failed to find wrapper in its own chain");
        }
        // let the root go
        NS_RELEASE(mRoot);
    }

    NS_IF_RELEASE(mClass);
    if (mOuter) {
        XPCJSRuntime* rt = nsXPConnect::GetRuntimeInstance();
        if (rt->GCIsRunning()) {
            nsContentUtils::DeferredFinalize(mOuter);
            mOuter = nullptr;
        } else {
            NS_RELEASE(mOuter);
        }
    }
}

nsXPCWrappedJS*
nsXPCWrappedJS::Find(REFNSIID aIID)
{
    if (aIID.Equals(NS_GET_IID(nsISupports)))
        return mRoot;

    for (nsXPCWrappedJS* cur = mRoot; cur; cur = cur->mNext) {
        if (aIID.Equals(cur->GetIID()))
            return cur;
    }

    return nullptr;
}

// check if asking for an interface that some wrapper in the chain inherits from
nsXPCWrappedJS*
nsXPCWrappedJS::FindInherited(REFNSIID aIID)
{
    MOZ_ASSERT(!aIID.Equals(NS_GET_IID(nsISupports)), "bad call sequence");

    for (nsXPCWrappedJS* cur = mRoot; cur; cur = cur->mNext) {
        bool found;
        if (NS_SUCCEEDED(cur->GetClass()->GetInterfaceInfo()->
                         HasAncestor(&aIID, &found)) && found)
            return cur;
    }

    return nullptr;
}

NS_IMETHODIMP
nsXPCWrappedJS::GetInterfaceInfo(nsIInterfaceInfo** info)
{
    MOZ_ASSERT(GetClass(), "wrapper without class");
    MOZ_ASSERT(GetClass()->GetInterfaceInfo(), "wrapper class without interface");

    // Since failing to get this info will crash some platforms(!), we keep
    // mClass valid at shutdown time.

    if (!(*info = GetClass()->GetInterfaceInfo()))
        return NS_ERROR_UNEXPECTED;
    NS_ADDREF(*info);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCWrappedJS::CallMethod(uint16_t methodIndex,
                           const XPTMethodDescriptor* info,
                           nsXPTCMiniVariant* params)
{
    // Do a release-mode assert against accessing nsXPCWrappedJS off-main-thread.
    if (!MOZ_LIKELY(NS_IsMainThread()))
        MOZ_CRASH();

    if (!IsValid())
        return NS_ERROR_UNEXPECTED;
    return GetClass()->CallMethod(this, methodIndex, info, params);
}

NS_IMETHODIMP
nsXPCWrappedJS::GetInterfaceIID(nsIID** iid)
{
    NS_PRECONDITION(iid, "bad param");

    *iid = (nsIID*) nsMemory::Clone(&(GetIID()), sizeof(nsIID));
    return *iid ? NS_OK : NS_ERROR_UNEXPECTED;
}

void
nsXPCWrappedJS::SystemIsBeingShutDown()
{
    // XXX It turns out that it is better to leak here then to do any Releases
    // and have them propagate into all sorts of mischief as the system is being
    // shutdown. This was learned the hard way :(

    // mJSObj == nullptr is used to indicate that the wrapper is no longer valid
    // and that calls should fail without trying to use any of the
    // xpconnect mechanisms. 'IsValid' is implemented by checking this pointer.

    // NOTE: that mClass is retained so that GetInterfaceInfo can continue to
    // work (and avoid crashing some platforms).

    // Use of unsafeGet() is to avoid triggering post barriers in shutdown, as
    // this will access the chunk containing mJSObj, which may have been freed
    // at this point.
    *mJSObj.unsafeGet() = nullptr;

    // Notify other wrappers in the chain.
    if (mNext)
        mNext->SystemIsBeingShutDown();
}

/***************************************************************************/

/* readonly attribute nsISimpleEnumerator enumerator; */
NS_IMETHODIMP
nsXPCWrappedJS::GetEnumerator(nsISimpleEnumerator * *aEnumerate)
{
    XPCCallContext ccx(NATIVE_CALLER);
    if (!ccx.IsValid())
        return NS_ERROR_UNEXPECTED;

    return nsXPCWrappedJSClass::BuildPropertyEnumerator(ccx, GetJSObject(),
                                                        aEnumerate);
}

/* nsIVariant getProperty (in AString name); */
NS_IMETHODIMP
nsXPCWrappedJS::GetProperty(const nsAString & name, nsIVariant **_retval)
{
    XPCCallContext ccx(NATIVE_CALLER);
    if (!ccx.IsValid())
        return NS_ERROR_UNEXPECTED;

    return nsXPCWrappedJSClass::
        GetNamedPropertyAsVariant(ccx, GetJSObject(), name, _retval);
}

/***************************************************************************/

NS_IMETHODIMP
nsXPCWrappedJS::DebugDump(int16_t depth)
{
#ifdef DEBUG
    XPC_LOG_ALWAYS(("nsXPCWrappedJS @ %x with mRefCnt = %d", this, mRefCnt.get()));
        XPC_LOG_INDENT();

        XPC_LOG_ALWAYS(("%s wrapper around JSObject @ %x", \
                        IsRootWrapper() ? "ROOT":"non-root", mJSObj.get()));
        char* name;
        GetClass()->GetInterfaceInfo()->GetName(&name);
        XPC_LOG_ALWAYS(("interface name is %s", name));
        if (name)
            nsMemory::Free(name);
        char * iid = GetClass()->GetIID().ToString();
        XPC_LOG_ALWAYS(("IID number is %s", iid ? iid : "invalid"));
        if (iid)
            NS_Free(iid);
        XPC_LOG_ALWAYS(("nsXPCWrappedJSClass @ %x", mClass));

        if (!IsRootWrapper())
            XPC_LOG_OUTDENT();
        if (mNext) {
            if (IsRootWrapper()) {
                XPC_LOG_ALWAYS(("Additional wrappers for this object..."));
                XPC_LOG_INDENT();
            }
            mNext->DebugDump(depth);
            if (IsRootWrapper())
                XPC_LOG_OUTDENT();
        }
        if (IsRootWrapper())
            XPC_LOG_OUTDENT();
#endif
    return NS_OK;
}
