/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "core/frame/FrameHost.h"

#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameView.h"
#include "core/frame/PageScaleConstraints.h"
#include "core/frame/PageScaleConstraintsSet.h"
#include "core/frame/TopControls.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/page/Page.h"
#include "core/page/scrolling/OverscrollController.h"
#include "core/page/scrolling/RootScroller.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"

namespace blink {

FrameHost* FrameHost::create(Page& page)
{
    return new FrameHost(page);
}

FrameHost::FrameHost(Page& page)
    : m_page(&page)
    , m_rootScroller(RootScroller::create(*this))
    , m_topControls(TopControls::create(*this))
    , m_pageScaleConstraintsSet(PageScaleConstraintsSet::create())
    , m_visualViewport(VisualViewport::create(*this))
    , m_overscrollController(OverscrollController::create(
        *m_visualViewport,
        m_page->chromeClient()))
    , m_eventHandlerRegistry(new EventHandlerRegistry(*this))
    , m_consoleMessageStorage(ConsoleMessageStorage::create())
    , m_subframeCount(0)
{
}

// Explicitly in the .cpp to avoid default constructor in .h
FrameHost::~FrameHost()
{
}

Page& FrameHost::page()
{
    return *m_page;
}

const Page& FrameHost::page() const
{
    return *m_page;
}

Settings& FrameHost::settings()
{
    return m_page->settings();
}

const Settings& FrameHost::settings() const
{
    return m_page->settings();
}

ChromeClient& FrameHost::chromeClient()
{
    return m_page->chromeClient();
}

const ChromeClient& FrameHost::chromeClient() const
{
    return m_page->chromeClient();
}

UseCounter& FrameHost::useCounter()
{
    return m_page->useCounter();
}

const UseCounter& FrameHost::useCounter() const
{
    return m_page->useCounter();
}

Deprecation& FrameHost::deprecation()
{
    return m_page->deprecation();
}

const Deprecation& FrameHost::deprecation() const
{
    return m_page->deprecation();
}

float FrameHost::deviceScaleFactor() const
{
    return m_page->deviceScaleFactor();
}

RootScroller* FrameHost::rootScroller()
{
    // RootScroller only makes sense if we're in the process where the main
    // frame is local.
    if (!m_page->mainFrame() || !m_page->mainFrame()->isLocalFrame())
        return nullptr;

    return m_rootScroller;
}

const RootScroller* FrameHost::rootScroller() const
{
    // RootScroller only makes sense if we're in the process where the main
    // frame is local.
    if (!m_page->mainFrame() || !m_page->mainFrame()->isLocalFrame())
        return nullptr;

    return m_rootScroller;
}

TopControls& FrameHost::topControls()
{
    return *m_topControls;
}

const TopControls& FrameHost::topControls() const
{
    return *m_topControls;
}

OverscrollController& FrameHost::overscrollController()
{
    return *m_overscrollController;
}

const OverscrollController& FrameHost::overscrollController() const
{
    return *m_overscrollController;
}

VisualViewport& FrameHost::visualViewport()
{
    return *m_visualViewport;
}

const VisualViewport& FrameHost::visualViewport() const
{
    return *m_visualViewport;
}

PageScaleConstraintsSet& FrameHost::pageScaleConstraintsSet()
{
    return *m_pageScaleConstraintsSet;
}

const PageScaleConstraintsSet& FrameHost::pageScaleConstraintsSet() const
{
    return *m_pageScaleConstraintsSet;
}

EventHandlerRegistry& FrameHost::eventHandlerRegistry()
{
    return *m_eventHandlerRegistry;
}

const EventHandlerRegistry& FrameHost::eventHandlerRegistry() const
{
    return *m_eventHandlerRegistry;
}

ConsoleMessageStorage& FrameHost::consoleMessageStorage()
{
    return *m_consoleMessageStorage;
}

const ConsoleMessageStorage& FrameHost::consoleMessageStorage() const
{
    return *m_consoleMessageStorage;
}

DEFINE_TRACE(FrameHost)
{
    visitor->trace(m_page);
    visitor->trace(m_rootScroller);
    visitor->trace(m_topControls);
    visitor->trace(m_visualViewport);
    visitor->trace(m_overscrollController);
    visitor->trace(m_eventHandlerRegistry);
    visitor->trace(m_consoleMessageStorage);
}

#if ENABLE(ASSERT)
void checkFrameCountConsistency(int expectedFrameCount, Frame* frame)
{
    ASSERT(expectedFrameCount >= 0);

    int actualFrameCount = 0;
    for (; frame; frame = frame->tree().traverseNext())
        ++actualFrameCount;

    ASSERT(expectedFrameCount == actualFrameCount);
}
#endif

int FrameHost::subframeCount() const
{
#if ENABLE(ASSERT)
    checkFrameCountConsistency(m_subframeCount + 1, m_page->mainFrame());
#endif
    return m_subframeCount;
}

void FrameHost::setDefaultPageScaleLimits(float minScale, float maxScale)
{
    PageScaleConstraints newDefaults = pageScaleConstraintsSet().defaultConstraints();
    newDefaults.minimumScale = minScale;
    newDefaults.maximumScale = maxScale;

    if (newDefaults == pageScaleConstraintsSet().defaultConstraints())
        return;

    pageScaleConstraintsSet().setDefaultConstraints(newDefaults);
    pageScaleConstraintsSet().computeFinalConstraints();
    pageScaleConstraintsSet().setNeedsReset(true);

    if (!page().mainFrame() || !page().mainFrame()->isLocalFrame())
        return;

    FrameView* rootView = page().deprecatedLocalMainFrame()->view();

    if (!rootView)
        return;

    rootView->setNeedsLayout();
}

void FrameHost::setUserAgentPageScaleConstraints(const PageScaleConstraints& newConstraints)
{
    if (newConstraints == pageScaleConstraintsSet().userAgentConstraints())
        return;

    pageScaleConstraintsSet().setUserAgentConstraints(newConstraints);

    if (!page().mainFrame() || !page().mainFrame()->isLocalFrame())
        return;

    FrameView* rootView = page().deprecatedLocalMainFrame()->view();

    if (!rootView)
        return;

    rootView->setNeedsLayout();
}

} // namespace blink
