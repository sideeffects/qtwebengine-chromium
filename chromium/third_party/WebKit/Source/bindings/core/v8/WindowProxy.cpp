/*
 * Copyright (C) 2008, 2009, 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bindings/core/v8/WindowProxy.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8DOMActivityLogger.h"
#include "bindings/core/v8/V8Document.h"
#include "bindings/core/v8/V8GCForContextDispose.h"
#include "bindings/core/v8/V8HTMLCollection.h"
#include "bindings/core/v8/V8HTMLDocument.h"
#include "bindings/core/v8/V8HiddenValue.h"
#include "bindings/core/v8/V8Initializer.h"
#include "bindings/core/v8/V8ObjectConstructor.h"
#include "bindings/core/v8/V8PagePopupControllerBinding.h"
#include "bindings/core/v8/V8Window.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/DocumentNameCollection.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/TraceEvent.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"
#include "wtf/OwnPtr.h"
#include "wtf/StringExtras.h"
#include "wtf/text/CString.h"
#include <algorithm>
#include <utility>
#include <v8-debug.h>
#include <v8.h>

namespace blink {

static void checkDocumentWrapper(v8::Local<v8::Object> wrapper, Document* document)
{
    ASSERT(V8Document::toImpl(wrapper) == document);
}

WindowProxy* WindowProxy::create(v8::Isolate* isolate, Frame* frame, DOMWrapperWorld& world)
{
    return new WindowProxy(frame, &world, isolate);
}

WindowProxy::WindowProxy(Frame* frame, PassRefPtr<DOMWrapperWorld> world, v8::Isolate* isolate)
    : m_frame(frame)
    , m_isolate(isolate)
    , m_world(world)
{
}

WindowProxy::~WindowProxy()
{
    // clearForClose() or clearForNavigation() must be invoked before destruction starts.
    ASSERT(!isContextInitialized());
}

DEFINE_TRACE(WindowProxy)
{
    visitor->trace(m_frame);
}

void WindowProxy::disposeContext(GlobalDetachmentBehavior behavior)
{
    if (!isContextInitialized())
        return;

    v8::HandleScope handleScope(m_isolate);
    v8::Local<v8::Context> context = m_scriptState->context();
    if (m_frame->isLocalFrame()) {
        LocalFrame* frame = toLocalFrame(m_frame);
        // The embedder could run arbitrary code in response to the willReleaseScriptContext callback, so all disposing should happen after it returns.
        frame->loader().client()->willReleaseScriptContext(context, m_world->worldId());
        MainThreadDebugger::instance()->contextWillBeDestroyed(m_scriptState.get());
    }

    m_document.clear();

    if (behavior == DetachGlobal)
        m_scriptState->detachGlobalObject();

    m_scriptState->disposePerContextData();

    // It's likely that disposing the context has created a lot of
    // garbage. Notify V8 about this so it'll have a chance of cleaning
    // it up when idle.
    V8GCForContextDispose::instance().notifyContextDisposed(m_frame->isMainFrame());
}

void WindowProxy::clearForClose()
{
    if (!isContextInitialized())
        return;

    disposeContext(DoNotDetachGlobal);
}

void WindowProxy::clearForNavigation()
{
    if (!isContextInitialized())
        return;

    ScriptState::Scope scope(m_scriptState.get());

    disposeContext(DetachGlobal);
}

v8::Local<v8::Object> WindowProxy::globalIfNotDetached()
{
    if (!isContextInitialized())
        return v8::Local<v8::Object>();
    ASSERT(m_scriptState->contextIsValid());
    ASSERT(m_global == m_scriptState->context()->Global());
    return m_global.newLocal(m_isolate);
}

v8::Local<v8::Object> WindowProxy::releaseGlobal()
{
    ASSERT(!isContextInitialized());
    // If a ScriptState was created, the context was initialized at some point.
    // Make sure the global object was detached from the proxy by calling clearForNavigation().
    if (m_scriptState)
        ASSERT(m_scriptState->isGlobalObjectDetached());
    v8::Local<v8::Object> global = m_global.newLocal(m_isolate);
    m_global.clear();
    return global;
}

void WindowProxy::setGlobal(v8::Local<v8::Object> global)
{
    m_global.set(m_isolate, global);

    // Initialize the window proxy now, to re-establish the connection between
    // the global object and the v8::Context. This is really only needed for a
    // RemoteDOMWindow, since it has no scripting environment of its own.
    // Without this, existing script references to a swapped in RemoteDOMWindow
    // would be broken until that RemoteDOMWindow was vended again through an
    // interface like window.frames.
    initializeIfNeeded();
}

// Create a new environment and setup the global object.
//
// The global object corresponds to a DOMWindow instance. However, to
// allow properties of the JS DOMWindow instance to be shadowed, we
// use a shadow object as the global object and use the JS DOMWindow
// instance as the prototype for that shadow object. The JS DOMWindow
// instance is undetectable from JavaScript code because the __proto__
// accessors skip that object.
//
// The shadow object and the DOMWindow instance are seen as one object
// from JavaScript. The JavaScript object that corresponds to a
// DOMWindow instance is the shadow object. When mapping a DOMWindow
// instance to a V8 object, we return the shadow object.
//
// To implement split-window, see
//   1) https://bugs.webkit.org/show_bug.cgi?id=17249
//   2) https://wiki.mozilla.org/Gecko:SplitWindow
//   3) https://bugzilla.mozilla.org/show_bug.cgi?id=296639
// we need to split the shadow object further into two objects:
// an outer window and an inner window. The inner window is the hidden
// prototype of the outer window. The inner window is the default
// global object of the context. A variable declared in the global
// scope is a property of the inner window.
//
// The outer window sticks to a LocalFrame, it is exposed to JavaScript
// via window.window, window.self, window.parent, etc. The outer window
// has a security token which is the domain. The outer window cannot
// have its own properties. window.foo = 'x' is delegated to the
// inner window.
//
// When a frame navigates to a new page, the inner window is cut off
// the outer window, and the outer window identify is preserved for
// the frame. However, a new inner window is created for the new page.
// If there are JS code holds a closure to the old inner window,
// it won't be able to reach the outer window via its global object.
bool WindowProxy::initializeIfNeeded()
{
    if (isContextInitialized())
        return true;

    return initialize();
}

bool WindowProxy::initialize()
{
    TRACE_EVENT0("v8", "WindowProxy::initialize");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("blink", "InitializeWindow");

    ScriptForbiddenScope::AllowUserAgentScript allowScript;

    v8::HandleScope handleScope(m_isolate);

    createContext();

    if (!isContextInitialized())
        return false;

    ScriptState::Scope scope(m_scriptState.get());
    v8::Local<v8::Context> context = m_scriptState->context();
    if (m_global.isEmpty()) {
        m_global.set(m_isolate, context->Global());
        if (m_global.isEmpty()) {
            disposeContext(DoNotDetachGlobal);
            return false;
        }
    }

    if (!setupWindowPrototypeChain()) {
        disposeContext(DoNotDetachGlobal);
        return false;
    }

    SecurityOrigin* origin = 0;
    if (m_world->isMainWorld()) {
        // ActivityLogger for main world is updated within updateDocument().
        updateDocument();
        origin = m_frame->securityContext()->getSecurityOrigin();
        // FIXME: Can this be removed when CSP moves to browser?
        ContentSecurityPolicy* csp = m_frame->securityContext()->contentSecurityPolicy();
        context->AllowCodeGenerationFromStrings(csp->allowEval(0, ContentSecurityPolicy::SuppressReport));
        context->SetErrorMessageForCodeGenerationFromStrings(v8String(m_isolate, csp->evalDisabledErrorMessage()));
    } else {
        updateActivityLogger();
        origin = m_world->isolatedWorldSecurityOrigin();
        setSecurityToken(origin);
    }
    if (m_frame->isLocalFrame()) {
        LocalFrame* frame = toLocalFrame(m_frame);
        MainThreadDebugger::instance()->contextCreated(m_scriptState.get(), frame, origin);
        frame->loader().client()->didCreateScriptContext(context, m_world->extensionGroup(), m_world->worldId());
    }
    // If Origin Trials have been registered before the V8 context was ready,
    // then inject them into the context now
    ExecutionContext* executionContext = m_scriptState->getExecutionContext();
    if (executionContext) {
        OriginTrialContext* originTrialContext = OriginTrialContext::from(executionContext);
        if (originTrialContext)
            originTrialContext->initializePendingFeatures();
    }
    return true;
}

void WindowProxy::createContext()
{
    // FIXME: This should be a null check of m_frame->client(), but there are still some edge cases
    // that this fails to catch during frame detach.
    if (m_frame->isLocalFrame() && !toLocalFrame(m_frame)->loader().documentLoader())
        return;

    // Create a new v8::Context with the window object as the global object
    // (aka the inner global).  Reuse the global proxy object (aka the outer
    // global) if it already exists.  See the comments in
    // setupWindowPrototypeChain for the structure of the prototype chain of
    // the global object.
    v8::Local<v8::ObjectTemplate> globalTemplate = V8Window::domTemplate(m_isolate, *m_world)->InstanceTemplate();
    if (globalTemplate.IsEmpty())
        return;

    // FIXME: It's not clear what the right thing to do for remote frames is.
    // The extensions registered don't generally seem to make sense for remote
    // frames, so skip it for now.
    Vector<const char*> extensionNames;
    if (m_frame->isLocalFrame()) {
        LocalFrame* frame = toLocalFrame(m_frame);
        // Dynamically tell v8 about our extensions now.
        const V8Extensions& extensions = ScriptController::registeredExtensions();
        extensionNames.reserveInitialCapacity(extensions.size());
        int extensionGroup = m_world->extensionGroup();
        int worldId = m_world->worldId();
        for (const auto* extension : extensions) {
            if (!frame->loader().client()->allowScriptExtension(extension->name(), extensionGroup, worldId))
                continue;

            extensionNames.append(extension->name());
        }
    }
    v8::ExtensionConfiguration extensionConfiguration(extensionNames.size(), extensionNames.data());

    v8::Local<v8::Context> context;
    {
        V8PerIsolateData::UseCounterDisabledScope useCounterDisabled(V8PerIsolateData::from(m_isolate));
        context = v8::Context::New(m_isolate, &extensionConfiguration, globalTemplate, m_global.newLocal(m_isolate));
    }
    if (context.IsEmpty())
        return;
    m_scriptState = ScriptState::create(context, m_world);
}

bool WindowProxy::setupWindowPrototypeChain()
{
    // Associate the window wrapper object and its prototype chain with the
    // corresponding native DOMWindow object.
    // The full structure of the global object's prototype chain is as follows:
    //
    // global proxy object [1]
    //   -- has prototype --> global object (window wrapper object) [2]
    //   -- has prototype --> Window.prototype
    //   -- has prototype --> WindowProperties [3]
    //   -- has prototype --> EventTarget.prototype
    //   -- has prototype --> Object.prototype
    //   -- has prototype --> null
    //
    // [1] Global proxy object is as known as "outer global object".  It's an
    //   empty object and remains after navigation.  When navigated, points to
    //   a different global object as the prototype object.
    // [2] Global object is as known as "inner global object" or "window wrapper
    //   object".  The prototype chain between global proxy object and global
    //   object is NOT observable from user JavaScript code.  All other
    //   prototype chains are observable.  Global proxy object and global object
    //   together appear to be the same single JavaScript object.  See also:
    //     https://wiki.mozilla.org/Gecko:SplitWindow
    //   global object (= window wrapper object) provides most of Window's DOM
    //   attributes and operations.  Also global variables defined by user
    //   JavaScript are placed on this object.  When navigated, a new global
    //   object is created together with a new v8::Context, but the global proxy
    //   object doesn't change.
    // [3] WindowProperties is a named properties object of Window interface.

    DOMWindow* window = m_frame->domWindow();
    const WrapperTypeInfo* wrapperTypeInfo = window->wrapperTypeInfo();

    v8::Local<v8::Context> context = m_scriptState->context();
    // The global proxy object.  Note this is not the global object.
    v8::Local<v8::Object> globalProxy = context->Global();
    // The global object, aka window wrapper object.
    v8::Local<v8::Object> windowWrapper = globalProxy->GetPrototype().As<v8::Object>();
    windowWrapper = V8DOMWrapper::associateObjectWithWrapper(m_isolate, window, wrapperTypeInfo, windowWrapper);
    // The prototype object of Window interface.
    v8::Local<v8::Object> windowPrototype = windowWrapper->GetPrototype().As<v8::Object>();
    RELEASE_ASSERT(!windowPrototype.IsEmpty());
    V8DOMWrapper::setNativeInfo(windowPrototype, wrapperTypeInfo, window);
    // The named properties object of Window interface.
    v8::Local<v8::Object> windowProperties = windowPrototype->GetPrototype().As<v8::Object>();
    RELEASE_ASSERT(!windowProperties.IsEmpty());
    V8DOMWrapper::setNativeInfo(windowProperties, wrapperTypeInfo, window);

    // TODO(keishi): Remove installPagePopupController and implement
    // PagePopupController in another way.
    V8PagePopupControllerBinding::installPagePopupController(context, windowWrapper);
    return true;
}

void WindowProxy::updateDocumentWrapper(v8::Local<v8::Object> wrapper)
{
    ASSERT(m_world->isMainWorld());
    m_document.set(m_isolate, wrapper);
}

void WindowProxy::updateDocumentProperty()
{
    if (!m_world->isMainWorld())
        return;

    if (m_frame->isRemoteFrame()) {
        return;
    }

    ScriptState::Scope scope(m_scriptState.get());
    v8::Local<v8::Context> context = m_scriptState->context();
    LocalFrame* frame = toLocalFrame(m_frame);
    v8::Local<v8::Value> documentWrapper = toV8(frame->document(), context->Global(), context->GetIsolate());
    if (documentWrapper.IsEmpty())
        return;
    ASSERT(documentWrapper == m_document.newLocal(m_isolate) || m_document.isEmpty());
    if (m_document.isEmpty())
        updateDocumentWrapper(v8::Local<v8::Object>::Cast(documentWrapper));
    checkDocumentWrapper(m_document.newLocal(m_isolate), frame->document());

    ASSERT(documentWrapper->IsObject());
    // TODO(jochen): Don't replace the accessor with a data value. We need a way to tell v8 that the accessor's return value won't change after this point.
    if (!v8CallBoolean(context->Global()->ForceSet(context, v8AtomicString(m_isolate, "document"), documentWrapper, static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete))))
        return;

    // We also stash a reference to the document on the inner global object so that
    // LocalDOMWindow objects we obtain from JavaScript references are guaranteed to have
    // live Document objects.
    V8HiddenValue::setHiddenValue(m_scriptState.get(), v8::Local<v8::Object>::Cast(context->Global()->GetPrototype()), V8HiddenValue::document(m_isolate), documentWrapper);
}

void WindowProxy::updateActivityLogger()
{
    m_scriptState->perContextData()->setActivityLogger(V8DOMActivityLogger::activityLogger(
        m_world->worldId(), m_frame->isLocalFrame() && toLocalFrame(m_frame)->document() ? toLocalFrame(m_frame)->document()->baseURI() : KURL()));
}

void WindowProxy::setSecurityToken(SecurityOrigin* origin)
{
    // If two tokens are equal, then the SecurityOrigins canAccess each other.
    // If two tokens are not equal, then we have to call canAccess.
    // Note: we can't use the HTTPOrigin if it was set from the DOM.
    String token;
    // There are several situations where v8 needs to do a full canAccess check,
    // so set an empty security token instead:
    // - document.domain was modified
    // - the frame is showing the initial empty document
    // - the frame is remote
    bool delaySet = m_frame->isRemoteFrame() || (m_world->isMainWorld() && (origin->domainWasSetInDOM() || toLocalFrame(m_frame)->loader().stateMachine()->isDisplayingInitialEmptyDocument()));
    if (origin && !delaySet)
        token = origin->toString();

    // An empty or "null" token means we always have to call
    // canAccess. The toString method on securityOrigins returns the
    // string "null" for empty security origins and for security
    // origins that should only allow access to themselves. In this
    // case, we use the global object as the security token to avoid
    // calling canAccess when a script accesses its own objects.
    v8::HandleScope handleScope(m_isolate);
    v8::Local<v8::Context> context = m_scriptState->context();
    if (token.isEmpty() || token == "null") {
        context->UseDefaultSecurityToken();
        return;
    }

    if (m_world->isPrivateScriptIsolatedWorld()) {
        token = "private-script://" + token;
    } else if (m_world->isIsolatedWorld()) {
        SecurityOrigin* frameSecurityOrigin = m_frame->securityContext()->getSecurityOrigin();
        String frameSecurityToken = frameSecurityOrigin->toString();
        // We need to check the return value of domainWasSetInDOM() on the
        // frame's SecurityOrigin because, if that's the case, only
        // SecurityOrigin::m_domain would have been modified.
        // m_domain is not used by SecurityOrigin::toString(), so we would end
        // up generating the same token that was already set.
        if (frameSecurityOrigin->domainWasSetInDOM() || frameSecurityToken.isEmpty() || frameSecurityToken == "null") {
            context->UseDefaultSecurityToken();
            return;
        }
        token = frameSecurityToken + token;
    }

    CString utf8Token = token.utf8();
    // NOTE: V8 does identity comparison in fast path, must use a symbol
    // as the security token.
    context->SetSecurityToken(v8AtomicString(m_isolate, utf8Token.data(), utf8Token.length()));
}

void WindowProxy::updateDocument()
{
    ASSERT(m_world->isMainWorld());
    if (!isGlobalInitialized())
        return;
    if (!isContextInitialized())
        return;
    updateActivityLogger();
    updateDocumentProperty();
    updateSecurityOrigin(m_frame->securityContext()->getSecurityOrigin());
}

static v8::Local<v8::Value> getNamedProperty(HTMLDocument* htmlDocument, const AtomicString& key, v8::Local<v8::Object> creationContext, v8::Isolate* isolate)
{
    if (!htmlDocument->hasNamedItem(key) && !htmlDocument->hasExtraNamedItem(key))
        return v8Undefined();

    DocumentNameCollection* items = htmlDocument->documentNamedItems(key);
    if (items->isEmpty())
        return v8Undefined();

    if (items->hasExactlyOneItem()) {
        HTMLElement* element = items->item(0);
        ASSERT(element);
        Frame* frame = isHTMLIFrameElement(*element) ? toHTMLIFrameElement(*element).contentFrame() : 0;
        if (frame)
            return toV8(frame->domWindow(), creationContext, isolate);
        return toV8(element, creationContext, isolate);
    }
    return toV8(items, creationContext, isolate);
}

static void getter(v8::Local<v8::Name> property, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    if (!property->IsString())
        return;
    // FIXME: Consider passing StringImpl directly.
    AtomicString name = toCoreAtomicString(property.As<v8::String>());
    HTMLDocument* htmlDocument = V8HTMLDocument::toImpl(info.Holder());
    ASSERT(htmlDocument);
    v8::Local<v8::Value> result = getNamedProperty(htmlDocument, name, info.Holder(), info.GetIsolate());
    if (!result.IsEmpty()) {
        v8SetReturnValue(info, result);
        return;
    }
    v8::Local<v8::Value> value;
    if (info.Holder()->GetRealNamedPropertyInPrototypeChain(info.GetIsolate()->GetCurrentContext(), property.As<v8::String>()).ToLocal(&value))
        v8SetReturnValue(info, value);
}

void WindowProxy::namedItemAdded(HTMLDocument* document, const AtomicString& name)
{
    ASSERT(m_world->isMainWorld());

    if (!isContextInitialized() || !m_scriptState->contextIsValid())
        return;

    ScriptState::Scope scope(m_scriptState.get());
    ASSERT(!m_document.isEmpty());
    v8::Local<v8::Context> context = m_scriptState->context();
    v8::Local<v8::Object> documentHandle = m_document.newLocal(m_isolate);
    checkDocumentWrapper(documentHandle, document);
    documentHandle->SetAccessor(context, v8String(m_isolate, name), getter);
}

void WindowProxy::namedItemRemoved(HTMLDocument* document, const AtomicString& name)
{
    ASSERT(m_world->isMainWorld());

    if (!isContextInitialized())
        return;

    if (document->hasNamedItem(name) || document->hasExtraNamedItem(name))
        return;

    ScriptState::Scope scope(m_scriptState.get());
    ASSERT(!m_document.isEmpty());
    v8::Local<v8::Object> documentHandle = m_document.newLocal(m_isolate);
    checkDocumentWrapper(documentHandle, document);
    documentHandle->Delete(m_isolate->GetCurrentContext(), v8String(m_isolate, name));
}

void WindowProxy::updateSecurityOrigin(SecurityOrigin* origin)
{
    if (!isContextInitialized())
        return;
    setSecurityToken(origin);
}

} // namespace blink
