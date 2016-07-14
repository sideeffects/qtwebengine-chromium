// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IntersectionObserver_h
#define IntersectionObserver_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/IntersectionObservation.h"
#include "core/dom/IntersectionObserverEntry.h"
#include "platform/Length.h"
#include "platform/heap/Handle.h"
#include "wtf/HashSet.h"
#include "wtf/Vector.h"

namespace blink {

class Element;
class ExceptionState;
class LayoutObject;
class IntersectionObserverCallback;
class IntersectionObserverInit;

class IntersectionObserver final : public GarbageCollectedFinalized<IntersectionObserver>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();

public:
    static IntersectionObserver* create(const IntersectionObserverInit&, IntersectionObserverCallback&, ExceptionState&);
    static void resumeSuspendedObservers();

    // API methods
    void observe(Element*);
    void unobserve(Element*);
    void disconnect();
    HeapVector<Member<IntersectionObserverEntry>> takeRecords();
    Element* root() const;
    String rootMargin() const;
    const Vector<float>& thresholds() const { return m_thresholds; }

    Node* rootNode() const { return m_root.get(); }
    LayoutObject* rootLayoutObject() const;
    const Length& topMargin() const { return m_topMargin; }
    const Length& rightMargin() const { return m_rightMargin; }
    const Length& bottomMargin() const { return m_bottomMargin; }
    const Length& leftMargin() const { return m_leftMargin; }
    void computeIntersectionObservations();
    void enqueueIntersectionObserverEntry(IntersectionObserverEntry&);
    void applyRootMargin(LayoutRect&) const;
    unsigned firstThresholdGreaterThan(float ratio) const;
    void deliver();
    void removeObservation(IntersectionObservation&);
    bool hasEntries() const { return m_entries.size(); }
    const HeapHashSet<WeakMember<IntersectionObservation>>& observations() const { return m_observations; }

    DECLARE_TRACE();

private:
    explicit IntersectionObserver(IntersectionObserverCallback&, Node&, const Vector<Length>& rootMargin, const Vector<float>& thresholds);
    void clearWeakMembers(Visitor*);

    Member<IntersectionObserverCallback> m_callback;
    WeakMember<Node> m_root;
    HeapHashSet<WeakMember<IntersectionObservation>> m_observations;
    HeapVector<Member<IntersectionObserverEntry>> m_entries;
    Vector<float> m_thresholds;
    Length m_topMargin;
    Length m_rightMargin;
    Length m_bottomMargin;
    Length m_leftMargin;
};

} // namespace blink

#endif // IntersectionObserver_h
