/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2011, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef Node_h
#define Node_h

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/NodeOrString.h"
#include "core/CoreExport.h"
#include "core/dom/MutationObserver.h"
#include "core/dom/SimulatedClickOptions.h"
#include "core/dom/TreeScope.h"
#include "core/editing/EditingBoundary.h"
#include "core/events/EventTarget.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"

// This needs to be here because Document.h also depends on it.
#define DUMP_NODE_STATISTICS 0

namespace blink {

class Attribute;
class ClassCollection;
class ContainerNode;
class DOMTokenList;
class Document;
class Element;
class ElementShadow;
class Event;
class EventDispatchMediator;
class EventListener;
class ExceptionState;
class FloatPoint;
class LocalFrame;
class HTMLInputElement;
class HTMLQualifiedName;
class HTMLSlotElement;
class IntRect;
class EventDispatchHandlingState;
class KeyboardEvent;
class NSResolver;
class NameNodeList;
class NamedNodeMap;
class NodeEventContext;
class NodeList;
class NodeListsNodeData;
class NodeRareData;
class PlatformGestureEvent;
class PlatformKeyboardEvent;
class PlatformMouseEvent;
class PlatformWheelEvent;
class PointerEvent;
class QualifiedName;
class RadioNodeList;
class RegisteredEventListener;
class LayoutBox;
class LayoutBoxModelObject;
class LayoutObject;
class ComputedStyle;
class SVGQualifiedName;
class ShadowRoot;
template <typename NodeType> class StaticNodeTypeList;
using StaticNodeList = StaticNodeTypeList<Node>;
class StyleChangeReasonForTracing;
class TagCollection;
class Text;
class TouchEvent;

const int nodeStyleChangeShift = 19;

enum StyleChangeType {
    NoStyleChange = 0,
    LocalStyleChange = 1 << nodeStyleChangeShift,
    SubtreeStyleChange = 2 << nodeStyleChangeShift,
    NeedsReattachStyleChange = 3 << nodeStyleChangeShift,
};

enum class CustomElementState {
    Uncustomized = 0,
    Custom = 1,
    Undefined = 2,
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, CustomElementState);

class NodeRareDataBase {
public:
    LayoutObject* layoutObject() const { return m_layoutObject; }
    void setLayoutObject(LayoutObject* layoutObject) { m_layoutObject = layoutObject; }

protected:
    NodeRareDataBase(LayoutObject* layoutObject)
        : m_layoutObject(layoutObject)
    { }

protected:
    // LayoutObjects are fully owned by their DOM node. See LayoutObject's
    // LIFETIME documentation section.
    LayoutObject* m_layoutObject;
};

class Node;
WILL_NOT_BE_EAGERLY_TRACED_CLASS(Node);

// This class represents a DOM node in the DOM tree.
// https://dom.spec.whatwg.org/#interface-node
class CORE_EXPORT Node : public EventTarget {
    DEFINE_WRAPPERTYPEINFO();
    friend class TreeScope;
    friend class TreeScopeAdopter;
public:
    enum NodeType {
        ELEMENT_NODE = 1,
        ATTRIBUTE_NODE = 2,
        TEXT_NODE = 3,
        CDATA_SECTION_NODE = 4,
        PROCESSING_INSTRUCTION_NODE = 7,
        COMMENT_NODE = 8,
        DOCUMENT_NODE = 9,
        DOCUMENT_TYPE_NODE = 10,
        DOCUMENT_FRAGMENT_NODE = 11,
    };

    // Entity, EntityReference, Notation, and XPathNamespace nodes are impossible to create in Blink.
    // But for compatibility reasons we want these enum values exist in JS, and this enum makes the bindings
    // generation not complain about ENTITY_REFERENCE_NODE being missing from the implementation
    // while not requiring all switch(NodeType) blocks to include this deprecated constant.
    enum DeprecatedNodeType {
        ENTITY_REFERENCE_NODE = 5,
        ENTITY_NODE = 6,
        NOTATION_NODE = 12,
        XPATH_NAMESPACE_NODE = 13,
    };

    enum DocumentPosition {
        DOCUMENT_POSITION_EQUIVALENT = 0x00,
        DOCUMENT_POSITION_DISCONNECTED = 0x01,
        DOCUMENT_POSITION_PRECEDING = 0x02,
        DOCUMENT_POSITION_FOLLOWING = 0x04,
        DOCUMENT_POSITION_CONTAINS = 0x08,
        DOCUMENT_POSITION_CONTAINED_BY = 0x10,
        DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC = 0x20,
    };

    // Override operator new to allocate Node subtype objects onto
    // a dedicated heap.
    GC_PLUGIN_IGNORE("crbug.com/443854")
    void* operator new(size_t size)
    {
        return allocateObject(size, false);
    }
    static void* allocateObject(size_t size, bool isEager)
    {
        ThreadState* state = ThreadStateFor<ThreadingTrait<Node>::Affinity>::state();
        const char typeName[] = "blink::Node";
        return ThreadHeap::allocateOnArenaIndex(state, size, isEager ? BlinkGC::EagerSweepArenaIndex : BlinkGC::NodeArenaIndex, GCInfoTrait<EventTarget>::index(), typeName);
    }

    static void dumpStatistics();

    ~Node() override;

    // DOM methods & attributes for Node

    bool hasTagName(const HTMLQualifiedName&) const;
    bool hasTagName(const SVGQualifiedName&) const;
    virtual String nodeName() const = 0;
    virtual String nodeValue() const;
    virtual void setNodeValue(const String&);
    virtual NodeType getNodeType() const = 0;
    ContainerNode* parentNode() const;
    Element* parentElement() const;
    ContainerNode* parentElementOrShadowRoot() const;
    ContainerNode* parentElementOrDocumentFragment() const;
    Node* previousSibling() const { return m_previous; }
    Node* nextSibling() const { return m_next; }
    NodeList* childNodes();
    Node* firstChild() const;
    Node* lastChild() const;
    Node& treeRoot() const;
    Node& shadowIncludingRoot() const;

    void prepend(const HeapVector<NodeOrString>&, ExceptionState&);
    void append(const HeapVector<NodeOrString>&, ExceptionState&);
    void before(const HeapVector<NodeOrString>&, ExceptionState&);
    void after(const HeapVector<NodeOrString>&, ExceptionState&);
    void replaceWith(const HeapVector<NodeOrString>&, ExceptionState&);
    void remove(ExceptionState& = ASSERT_NO_EXCEPTION);

    Node* pseudoAwareNextSibling() const;
    Node* pseudoAwarePreviousSibling() const;
    Node* pseudoAwareFirstChild() const;
    Node* pseudoAwareLastChild() const;

    const KURL& baseURI() const;

    Node* insertBefore(Node* newChild, Node* refChild, ExceptionState& = ASSERT_NO_EXCEPTION);
    Node* replaceChild(Node* newChild, Node* oldChild, ExceptionState& = ASSERT_NO_EXCEPTION);
    Node* removeChild(Node* child, ExceptionState& = ASSERT_NO_EXCEPTION);
    Node* appendChild(Node* newChild, ExceptionState& = ASSERT_NO_EXCEPTION);

    bool hasChildren() const { return firstChild(); }
    virtual Node* cloneNode(bool deep) = 0;
    void normalize();

    bool isEqualNode(Node*) const;
    bool isSameNode(Node* other) const { return this == other; }
    bool isDefaultNamespace(const AtomicString& namespaceURI) const;
    const AtomicString& lookupPrefix(const AtomicString& namespaceURI) const;
    const AtomicString& lookupNamespaceURI(const String& prefix) const;

    String textContent(bool convertBRsToNewlines = false) const;
    void setTextContent(const String&);

    bool supportsAltText();

    // Other methods (not part of DOM)

    bool isElementNode() const { return getFlag(IsElementFlag); }
    bool isContainerNode() const { return getFlag(IsContainerFlag); }
    bool isTextNode() const { return getFlag(IsTextFlag); }
    bool isHTMLElement() const { return getFlag(IsHTMLFlag); }
    bool isSVGElement() const { return getFlag(IsSVGFlag); }

    bool isPseudoElement() const { return getPseudoId() != PseudoIdNone; }
    bool isBeforePseudoElement() const { return getPseudoId() == PseudoIdBefore; }
    bool isAfterPseudoElement() const { return getPseudoId() == PseudoIdAfter; }
    bool isFirstLetterPseudoElement() const { return getPseudoId() == PseudoIdFirstLetter; }
    virtual PseudoId getPseudoId() const { return PseudoIdNone; }

    bool isCustomElement() const { return getFlag(CustomElementFlag); }
    CustomElementState getCustomElementState() const;
    void setCustomElementState(CustomElementState);
    bool isV0CustomElement() const { return getFlag(V0CustomElementFlag); }
    enum V0CustomElementState {
        V0NotCustomElement  = 0,
        V0WaitingForUpgrade = 1 << 0,
        V0Upgraded          = 1 << 1
    };
    V0CustomElementState getV0CustomElementState() const
    {
        return isV0CustomElement()
            ? (getFlag(V0CustomElementUpgradedFlag) ? V0Upgraded : V0WaitingForUpgrade)
            : V0NotCustomElement;
    }
    void setV0CustomElementState(V0CustomElementState newState);

    virtual bool isMediaControlElement() const { return false; }
    virtual bool isMediaControls() const { return false; }
    virtual bool isTextTrackContainer() const { return false; }
    virtual bool isVTTElement() const { return false; }
    virtual bool isAttributeNode() const { return false; }
    virtual bool isCharacterDataNode() const { return false; }
    virtual bool isFrameOwnerElement() const { return false; }

    // StyledElements allow inline style (style="border: 1px"), presentational attributes (ex. color),
    // class names (ex. class="foo bar") and other non-basic styling features. They and also control
    // if this element can participate in style sharing.
    //
    // FIXME: The only things that ever go through StyleResolver that aren't StyledElements are
    // PseudoElements and VTTElements. It's possible we can just eliminate all the checks
    // since those elements will never have class names, inline style, or other things that
    // this apparently guards against.
    bool isStyledElement() const { return isHTMLElement() || isSVGElement(); }

    bool isDocumentNode() const;
    bool isTreeScope() const;
    bool isDocumentFragment() const { return getFlag(IsDocumentFragmentFlag); }
    bool isShadowRoot() const { return isDocumentFragment() && isTreeScope(); }
    bool isInsertionPoint() const { return getFlag(IsInsertionPointFlag); }

    bool canParticipateInFlatTree() const;
    bool isSlotOrActiveInsertionPoint() const;
    bool slottable() const { return isElementNode() || isTextNode(); }
    AtomicString slotName() const;

    static AtomicString normalizeSlotName(const AtomicString&);

    bool hasCustomStyleCallbacks() const { return getFlag(HasCustomStyleCallbacksFlag); }

    // If this node is in a shadow tree, returns its shadow host. Otherwise, returns nullptr.
    // TODO(kochi): crbug.com/507413 shadowHost() can return nullptr even when it is in a
    // shadow tree but its root is detached from its host. This can happen when handling
    // queued events (e.g. during execCommand()).
    Element* shadowHost() const;
    // crbug.com/569532: containingShadowRoot() can return nullptr even if isInShadowTree() returns true.
    // This can happen when handling queued events (e.g. during execCommand())
    ShadowRoot* containingShadowRoot() const;
    ShadowRoot* youngestShadowRoot() const;

    // Returns nullptr, a child of ShadowRoot, or a legacy shadow root.
    Node* nonBoundaryShadowTreeRootNode();

    // Node's parent, shadow tree host.
    ContainerNode* parentOrShadowHostNode() const;
    Element* parentOrShadowHostElement() const;
    void setParentOrShadowHostNode(ContainerNode*);

    // Knows about all kinds of hosts.
    ContainerNode* parentOrShadowHostOrTemplateHostNode() const;

    // Returns the parent node, but nullptr if the parent node is a ShadowRoot.
    ContainerNode* nonShadowBoundaryParentNode() const;

    // Returns the enclosing event parent Element (or self) that, when clicked, would trigger a navigation.
    Element* enclosingLinkEventParentOrSelf() const;

    // These low-level calls give the caller responsibility for maintaining the integrity of the tree.
    void setPreviousSibling(Node* previous) { m_previous = previous; }
    void setNextSibling(Node* next) { m_next = next; }

    virtual bool canContainRangeEndPoint() const { return false; }

    bool isRootEditableElement() const;
    Element* rootEditableElement() const;
    Element* rootEditableElement(EditableType) const;

    // For <link> and <style> elements.
    virtual bool sheetLoaded() { return true; }
    enum LoadedSheetErrorStatus {
        NoErrorLoadingSubresource,
        ErrorOccurredLoadingSubresource
    };
    virtual void notifyLoadedSheetAndAllCriticalSubresources(LoadedSheetErrorStatus) { }
    virtual void startLoadingDynamicSheet() { ASSERT_NOT_REACHED(); }

    bool hasName() const { DCHECK(!isTextNode()); return getFlag(HasNameOrIsEditingTextFlag); }

    bool isUserActionElement() const { return getFlag(IsUserActionElementFlag); }
    void setUserActionElement(bool flag) { setFlag(flag, IsUserActionElementFlag); }

    bool active() const { return isUserActionElement() && isUserActionElementActive(); }
    bool inActiveChain() const { return isUserActionElement() && isUserActionElementInActiveChain(); }
    bool hovered() const { return isUserActionElement() && isUserActionElementHovered(); }
    // Note: As a shadow host whose root with delegatesFocus=false may become focused state when
    // an inner element gets focused, in that case more than one elements in a document can return
    // true for |focused()|.  Use Element::isFocusedElementInDocument() or Document::focusedElement()
    // to check which element is exactly focused.
    bool focused() const { return isUserActionElement() && isUserActionElementFocused(); }

    bool needsAttach() const { return getStyleChangeType() == NeedsReattachStyleChange; }
    bool needsStyleRecalc() const { return getStyleChangeType() != NoStyleChange; }
    StyleChangeType getStyleChangeType() const { return static_cast<StyleChangeType>(m_nodeFlags & StyleChangeMask); }
    bool childNeedsStyleRecalc() const { return getFlag(ChildNeedsStyleRecalcFlag); }
    bool isLink() const { return getFlag(IsLinkFlag); }
    bool isEditingText() const { DCHECK(isTextNode()); return getFlag(HasNameOrIsEditingTextFlag); }

    void setHasName(bool f) { DCHECK(!isTextNode()); setFlag(f, HasNameOrIsEditingTextFlag); }
    void setChildNeedsStyleRecalc() { setFlag(ChildNeedsStyleRecalcFlag); }
    void clearChildNeedsStyleRecalc() { clearFlag(ChildNeedsStyleRecalcFlag); }

    void setNeedsStyleRecalc(StyleChangeType, const StyleChangeReasonForTracing&);
    void clearNeedsStyleRecalc();

#if DCHECK_IS_ON()
    bool needsDistributionRecalc() const;
#endif

    bool childNeedsDistributionRecalc() const { return getFlag(ChildNeedsDistributionRecalcFlag); }
    void setChildNeedsDistributionRecalc()  { setFlag(ChildNeedsDistributionRecalcFlag); }
    void clearChildNeedsDistributionRecalc()  { clearFlag(ChildNeedsDistributionRecalcFlag); }
    void markAncestorsWithChildNeedsDistributionRecalc();

    bool childNeedsStyleInvalidation() const { return getFlag(ChildNeedsStyleInvalidationFlag); }
    void setChildNeedsStyleInvalidation()  { setFlag(ChildNeedsStyleInvalidationFlag); }
    void clearChildNeedsStyleInvalidation()  { clearFlag(ChildNeedsStyleInvalidationFlag); }
    void markAncestorsWithChildNeedsStyleInvalidation();
    bool needsStyleInvalidation() const { return getFlag(NeedsStyleInvalidationFlag); }
    void clearNeedsStyleInvalidation() { clearFlag(NeedsStyleInvalidationFlag); }
    void setNeedsStyleInvalidation();

    void updateDistribution();

    bool svgFilterNeedsLayerUpdate() const { return getFlag(SVGFilterNeedsLayerUpdateFlag); }
    void setSVGFilterNeedsLayerUpdate() { setFlag(SVGFilterNeedsLayerUpdateFlag); }
    void clearSVGFilterNeedsLayerUpdate() { clearFlag(SVGFilterNeedsLayerUpdateFlag); }

    void setIsLink(bool f);

    bool hasEventTargetData() const { return getFlag(HasEventTargetDataFlag); }
    void setHasEventTargetData(bool flag) { setFlag(flag, HasEventTargetDataFlag); }

    bool isV8CollectableDuringMinorGC() const { return getFlag(V8CollectableDuringMinorGCFlag); }
    void markV8CollectableDuringMinorGC() { setFlag(true, V8CollectableDuringMinorGCFlag); }
    void clearV8CollectableDuringMinorGC() { setFlag(false, V8CollectableDuringMinorGCFlag); }

    virtual void setFocus(bool flag);
    virtual void setActive(bool flag = true);
    virtual void setHovered(bool flag = true);

    virtual short tabIndex() const;

    virtual Node* focusDelegate();
    // This is called only when the node is focused.
    virtual bool shouldHaveFocusAppearance() const;

    // Whether the node is inert. This can't be in Element because text nodes
    // must be recognized as inert to prevent text selection.
    bool isInert() const;

    enum UserSelectAllTreatment {
        UserSelectAllDoesNotAffectEditability,
        UserSelectAllIsAlwaysNonEditable
    };
    bool isContentEditable(UserSelectAllTreatment = UserSelectAllDoesNotAffectEditability) const;
    bool isContentRichlyEditable() const;

    bool hasEditableStyle(EditableType editableType = ContentIsEditable, UserSelectAllTreatment treatment = UserSelectAllIsAlwaysNonEditable) const
    {
        switch (editableType) {
        case ContentIsEditable:
            return hasEditableStyle(Editable, treatment);
        case HasEditableAXRole:
            return isEditableToAccessibility(Editable);
        }
        ASSERT_NOT_REACHED();
        return false;
    }

    bool layoutObjectIsRichlyEditable(EditableType editableType = ContentIsEditable) const
    {
        switch (editableType) {
        case ContentIsEditable:
            return hasEditableStyle(RichlyEditable, UserSelectAllIsAlwaysNonEditable);
        case HasEditableAXRole:
            return isEditableToAccessibility(RichlyEditable);
        }
        ASSERT_NOT_REACHED();
        return false;
    }

    virtual LayoutRect boundingBox() const;
    IntRect pixelSnappedBoundingBox() const { return pixelSnappedIntRect(boundingBox()); }

    unsigned nodeIndex() const;

    // Returns the DOM ownerDocument attribute. This method never returns null, except in the case
    // of a Document node.
    Document* ownerDocument() const;

    // Returns the document associated with this node. A Document node returns itself.
    Document& document() const
    {
        return treeScope().document();
    }

    TreeScope& treeScope() const
    {
        DCHECK(m_treeScope);
        return *m_treeScope;
    }

    bool inActiveDocument() const;

    // Returns true if this node is associated with a shadow-including document and is in its associated document's
    // node tree, false otherwise.
    bool inShadowIncludingDocument() const
    {
        return getFlag(InDocumentFlag);
    }
    bool isInShadowTree() const { return getFlag(IsInShadowTreeFlag); }
    bool isInTreeScope() const { return getFlag(static_cast<NodeFlags>(InDocumentFlag | IsInShadowTreeFlag)); }

    ElementShadow* parentElementShadow() const;
    bool isInV1ShadowTree() const;
    bool isInV0ShadowTree() const;
    bool isChildOfV1ShadowHost() const;
    bool isChildOfV0ShadowHost() const;
    bool isSlotAssignable() const { return isTextNode() || isElementNode(); }

    bool isDocumentTypeNode() const { return getNodeType() == DOCUMENT_TYPE_NODE; }
    virtual bool childTypeAllowed(NodeType) const { return false; }
    unsigned countChildren() const;

    bool isDescendantOf(const Node*) const;
    bool contains(const Node*) const;
    bool isShadowIncludingInclusiveAncestorOf(const Node*) const;
    bool containsIncludingHostElements(const Node&) const;
    Node* commonAncestor(const Node&, ContainerNode* (*parent)(const Node&)) const;

    // Used to determine whether range offsets use characters or node indices.
    bool offsetInCharacters() const;
    // Number of DOM 16-bit units contained in node. Note that laid out text length can be different - e.g. because of
    // css-transform:capitalize breaking up precomposed characters and ligatures.
    virtual int maxCharacterOffset() const;

    // Whether or not a selection can be started in this object
    virtual bool canStartSelection() const;

    // -----------------------------------------------------------------------------
    // Integration with layout tree

    // As layoutObject() includes a branch you should avoid calling it repeatedly in hot code paths.
    // Note that if a Node has a layoutObject, it's parentNode is guaranteed to have one as well.
    LayoutObject* layoutObject() const { return hasRareData() ? m_data.m_rareData->layoutObject() : m_data.m_layoutObject; }
    void setLayoutObject(LayoutObject* layoutObject)
    {
        if (hasRareData())
            m_data.m_rareData->setLayoutObject(layoutObject);
        else
            m_data.m_layoutObject = layoutObject;
    }

    // Use these two methods with caution.
    LayoutBox* layoutBox() const;
    LayoutBoxModelObject* layoutBoxModelObject() const;

    struct AttachContext {
        STACK_ALLOCATED();
        ComputedStyle* resolvedStyle = nullptr;
        bool performingReattach = false;
        bool clearInvalidation = false;

        AttachContext() { }
    };

    // Attaches this node to the layout tree. This calculates the style to be applied to the node and creates an
    // appropriate LayoutObject which will be inserted into the tree (except when the style has display: none). This
    // makes the node visible in the FrameView.
    virtual void attach(const AttachContext& = AttachContext());

    // Detaches the node from the layout tree, making it invisible in the rendered view. This method will remove
    // the node's layout object from the layout tree and delete it.
    virtual void detach(const AttachContext& = AttachContext());

    void reattach(const AttachContext& = AttachContext());
    void lazyReattachIfAttached();

    // Returns true if recalcStyle should be called on the object, if there is such a method (on Document and Element).
    bool shouldCallRecalcStyle(StyleRecalcChange);

    // Wrapper for nodes that don't have a layoutObject, but still cache the style (like HTMLOptionElement).
    ComputedStyle* mutableComputedStyle() const;
    const ComputedStyle* computedStyle() const;
    const ComputedStyle* parentComputedStyle() const;

    const ComputedStyle& computedStyleRef() const;

    const ComputedStyle* ensureComputedStyle(PseudoId pseudoElementSpecifier = PseudoIdNone) { return virtualEnsureComputedStyle(pseudoElementSpecifier); }

    // -----------------------------------------------------------------------------
    // Notification of document structure changes (see ContainerNode.h for more notification methods)
    //
    // At first, WebKit notifies the node that it has been inserted into the document. This is called during document parsing, and also
    // when a node is added through the DOM methods insertBefore(), appendChild() or replaceChild(). The call happens _after_ the node has been added to the tree.
    // This is similar to the DOMNodeInsertedIntoDocument DOM event, but does not require the overhead of event
    // dispatching.
    //
    // WebKit notifies this callback regardless if the subtree of the node is a document tree or a floating subtree.
    // Implementation can determine the type of subtree by seeing insertionPoint->inShadowIncludingDocument().
    // For a performance reason, notifications are delivered only to ContainerNode subclasses if the insertionPoint is out of document.
    //
    // There are another callback named didNotifySubtreeInsertionsToDocument(), which is called after all the descendant is notified,
    // if this node was inserted into the document tree. Only a few subclasses actually need this. To utilize this, the node should
    // return InsertionShouldCallDidNotifySubtreeInsertions from insertedInto().
    //
    enum InsertionNotificationRequest {
        InsertionDone,
        InsertionShouldCallDidNotifySubtreeInsertions
    };

    virtual InsertionNotificationRequest insertedInto(ContainerNode* insertionPoint);
    virtual void didNotifySubtreeInsertionsToDocument() { }

    // Notifies the node that it is no longer part of the tree.
    //
    // This is a dual of insertedInto(), and is similar to the DOMNodeRemovedFromDocument DOM event, but does not require the overhead of event
    // dispatching, and is called _after_ the node is removed from the tree.
    //
    virtual void removedFrom(ContainerNode* insertionPoint);

    // FIXME(dominicc): This method is not debug-only--it is used by
    // Tracing--rename it to something indicative.
    String debugName() const;

#ifndef NDEBUG
    virtual void formatForDebugger(char* buffer, unsigned length) const;

    void showNode(const char* prefix = "") const;
    void showTreeForThis() const;
    void showTreeForThisInFlatTree() const;
    void showNodePathForThis() const;
    void showTreeAndMark(const Node* markedNode1, const char* markedLabel1, const Node* markedNode2 = nullptr, const char* markedLabel2 = nullptr) const;
    void showTreeAndMarkInFlatTree(const Node* markedNode1, const char* markedLabel1, const Node* markedNode2 = nullptr, const char* markedLabel2 = nullptr) const;
    void showTreeForThisAcrossFrame() const;
#endif

    NodeListsNodeData* nodeLists();
    void clearNodeLists();

    virtual bool willRespondToMouseMoveEvents();
    virtual bool willRespondToMouseClickEvents();
    virtual bool willRespondToTouchEvents();

    enum ShadowTreesTreatment {
        TreatShadowTreesAsDisconnected,
        TreatShadowTreesAsComposed
    };

    unsigned short compareDocumentPosition(const Node*, ShadowTreesTreatment = TreatShadowTreesAsDisconnected) const;

    Node* toNode() final;

    const AtomicString& interfaceName() const override;
    ExecutionContext* getExecutionContext() const final;

    void removeAllEventListeners() override;
    void removeAllEventListenersRecursively();

    // Handlers to do/undo actions on the target node before an event is dispatched to it and after the event
    // has been dispatched.  The data pointer is handed back by the preDispatch and passed to postDispatch.
    virtual EventDispatchHandlingState* preDispatchEventHandler(Event*) { return nullptr; }
    virtual void postDispatchEventHandler(Event*, EventDispatchHandlingState*) { }

    void dispatchScopedEvent(Event*);

    virtual void handleLocalEvents(Event&);

    void dispatchSubtreeModifiedEvent();
    DispatchEventResult dispatchDOMActivateEvent(int detail, Event* underlyingEvent);

    DispatchEventResult dispatchMouseEvent(const PlatformMouseEvent&, const AtomicString& eventType, int clickCount = 0, Node* relatedTarget = nullptr);

    void dispatchSimulatedClick(Event* underlyingEvent, SimulatedClickMouseEventOptions = SendNoEvents, SimulatedClickCreationScope = SimulatedClickCreationScope::FromUserAgent);

    void dispatchInputEvent();

    // Perform the default action for an event.
    virtual void defaultEventHandler(Event*);
    virtual void willCallDefaultEventHandler(const Event&);

    EventTargetData* eventTargetData() override;
    EventTargetData& ensureEventTargetData() override;

    void getRegisteredMutationObserversOfType(HeapHashMap<Member<MutationObserver>, MutationRecordDeliveryOptions>&, MutationObserver::MutationType, const QualifiedName* attributeName);
    void registerMutationObserver(MutationObserver&, MutationObserverOptions, const HashSet<AtomicString>& attributeFilter);
    void unregisterMutationObserver(MutationObserverRegistration*);
    void registerTransientMutationObserver(MutationObserverRegistration*);
    void unregisterTransientMutationObserver(MutationObserverRegistration*);
    void notifyMutationObserversNodeWillDetach();

    unsigned connectedSubframeCount() const;
    void incrementConnectedSubframeCount();
    void decrementConnectedSubframeCount();

    StaticNodeList* getDestinationInsertionPoints();
    HTMLSlotElement* assignedSlot() const;
    HTMLSlotElement* assignedSlotForBinding();

    void setAlreadySpellChecked(bool flag) { setFlag(flag, AlreadySpellCheckedFlag); }
    bool isAlreadySpellChecked() { return getFlag(AlreadySpellCheckedFlag); }

    bool isFinishedParsingChildren() const { return getFlag(IsFinishedParsingChildrenFlag); }

    void updateAssignmentForInsertedInto(ContainerNode*);

    DECLARE_VIRTUAL_TRACE();

    DECLARE_VIRTUAL_TRACE_WRAPPERS();

    unsigned lengthOfContents() const;

    v8::Local<v8::Object> wrap(v8::Isolate*, v8::Local<v8::Object> creationContext) override;
    v8::Local<v8::Object> associateWithWrapper(v8::Isolate*, const WrapperTypeInfo*, v8::Local<v8::Object> wrapper) override WARN_UNUSED_RETURN;

private:
    enum NodeFlags {
        HasRareDataFlag = 1,

        // Node type flags. These never change once created.
        IsTextFlag = 1 << 1,
        IsContainerFlag = 1 << 2,
        IsElementFlag = 1 << 3,
        IsHTMLFlag = 1 << 4,
        IsSVGFlag = 1 << 5,
        IsDocumentFragmentFlag = 1 << 6,
        IsInsertionPointFlag = 1 << 7,

        // Changes based on if the element should be treated like a link,
        // ex. When setting the href attribute on an <a>.
        IsLinkFlag = 1 << 8,

        // Changes based on :hover, :active and :focus state.
        IsUserActionElementFlag = 1 << 9,

        // Tree state flags. These change when the element is added/removed
        // from a DOM tree.
        InDocumentFlag = 1 << 10,
        IsInShadowTreeFlag = 1 << 11,

        // Set by the parser when the children are done parsing.
        IsFinishedParsingChildrenFlag = 1 << 12,

        // Flags related to recalcStyle.
        SVGFilterNeedsLayerUpdateFlag = 1 << 13,
        HasCustomStyleCallbacksFlag = 1 << 14,
        ChildNeedsStyleInvalidationFlag = 1 << 15,
        NeedsStyleInvalidationFlag = 1 << 16,
        ChildNeedsDistributionRecalcFlag = 1 << 17,
        ChildNeedsStyleRecalcFlag = 1 << 18,
        StyleChangeMask = 1 << nodeStyleChangeShift | 1 << (nodeStyleChangeShift + 1),

        CustomElementFlag = 1 << 21,
        CustomElementCustomFlag = 1 << 22,

        HasNameOrIsEditingTextFlag = 1 << 23,
        HasWeakReferencesFlag = 1 << 24,
        V8CollectableDuringMinorGCFlag = 1 << 25,
        HasEventTargetDataFlag = 1 << 26,
        AlreadySpellCheckedFlag = 1 << 27,

        V0CustomElementFlag = 1 << 28,
        V0CustomElementUpgradedFlag = 1 << 29,

        DefaultNodeFlags = IsFinishedParsingChildrenFlag | NeedsReattachStyleChange
    };

    // 3 bits remaining.

    bool getFlag(NodeFlags mask) const { return m_nodeFlags & mask; }
    void setFlag(bool f, NodeFlags mask) { m_nodeFlags = (m_nodeFlags & ~mask) | (-(int32_t)f & mask); }
    void setFlag(NodeFlags mask) { m_nodeFlags |= mask; }
    void clearFlag(NodeFlags mask) { m_nodeFlags &= ~mask; }

protected:
    enum ConstructionType {
        CreateOther = DefaultNodeFlags,
        CreateText = DefaultNodeFlags | IsTextFlag,
        CreateContainer = DefaultNodeFlags | ChildNeedsStyleRecalcFlag | IsContainerFlag,
        CreateElement = CreateContainer | IsElementFlag,
        CreateShadowRoot = CreateContainer | IsDocumentFragmentFlag | IsInShadowTreeFlag,
        CreateDocumentFragment = CreateContainer | IsDocumentFragmentFlag,
        CreateHTMLElement = CreateElement | IsHTMLFlag,
        CreateSVGElement = CreateElement | IsSVGFlag,
        CreateDocument = CreateContainer | InDocumentFlag,
        CreateInsertionPoint = CreateHTMLElement | IsInsertionPointFlag,
        CreateEditingText = CreateText | HasNameOrIsEditingTextFlag,
    };

    Node(TreeScope*, ConstructionType);

    virtual void didMoveToNewDocument(Document& oldDocument);

    void addedEventListener(const AtomicString& eventType, RegisteredEventListener&) override;
    void removedEventListener(const AtomicString& eventType, const RegisteredEventListener&) override;
    DispatchEventResult dispatchEventInternal(Event*) override;

    static void reattachWhitespaceSiblingsIfNeeded(Text* start);

    bool hasRareData() const { return getFlag(HasRareDataFlag); }

    NodeRareData* rareData() const;
    NodeRareData& ensureRareData();

    void setHasCustomStyleCallbacks() { setFlag(true, HasCustomStyleCallbacksFlag); }

    void setTreeScope(TreeScope* scope) { m_treeScope = scope; }

    // isTreeScopeInitialized() can be false
    // - in the destruction of Document or ShadowRoot where m_treeScope is set to null or
    // - in the Node constructor called by these two classes where m_treeScope is set by TreeScope ctor.
    bool isTreeScopeInitialized() const { return m_treeScope; }

    void markAncestorsWithChildNeedsStyleRecalc();

    void setIsFinishedParsingChildren(bool value) { setFlag(value, IsFinishedParsingChildrenFlag); }

private:
    // Gets nodeName without caching AtomicStrings. Used by
    // debugName. Compositor may call debugName from the "impl" thread
    // during "commit". The main thread is stopped at that time, but
    // it is not safe to cache AtomicStrings because those are
    // per-thread.
    virtual String debugNodeName() const;

    enum EditableLevel { Editable, RichlyEditable };
    bool hasEditableStyle(EditableLevel, UserSelectAllTreatment = UserSelectAllIsAlwaysNonEditable) const;
    bool isEditableToAccessibility(EditableLevel) const;

    bool isUserActionElementActive() const;
    bool isUserActionElementInActiveChain() const;
    bool isUserActionElementHovered() const;
    bool isUserActionElementFocused() const;

    void recalcDistribution();

    void setStyleChange(StyleChangeType);

    virtual ComputedStyle* nonLayoutObjectComputedStyle() const { return nullptr; }

    virtual const ComputedStyle* virtualEnsureComputedStyle(PseudoId = PseudoIdNone);

    void trackForDebugging();

    HeapVector<Member<MutationObserverRegistration>>* mutationObserverRegistry();
    HeapHashSet<Member<MutationObserverRegistration>>* transientMutationObserverRegistry();

    uint32_t m_nodeFlags;
    Member<ContainerNode> m_parentOrShadowHostNode;
    Member<TreeScope> m_treeScope;
    Member<Node> m_previous;
    Member<Node> m_next;
    // When a node has rare data we move the layoutObject into the rare data.
    union DataUnion {
        DataUnion() : m_layoutObject(nullptr) { }
        // LayoutObjects are fully owned by their DOM node. See LayoutObject's
        // LIFETIME documentation section.
        LayoutObject* m_layoutObject;
        NodeRareDataBase* m_rareData;
    } m_data;
};

inline void Node::setParentOrShadowHostNode(ContainerNode* parent)
{
    DCHECK(isMainThread());
    m_parentOrShadowHostNode = parent;
}

inline ContainerNode* Node::parentOrShadowHostNode() const
{
    DCHECK(isMainThread());
    return m_parentOrShadowHostNode;
}

inline ContainerNode* Node::parentNode() const
{
    return isShadowRoot() ? nullptr : parentOrShadowHostNode();
}

inline void Node::lazyReattachIfAttached()
{
    if (getStyleChangeType() == NeedsReattachStyleChange)
        return;
    if (!inActiveDocument())
        return;

    AttachContext context;
    context.performingReattach = true;

    detach(context);
    markAncestorsWithChildNeedsStyleRecalc();
}

inline bool Node::shouldCallRecalcStyle(StyleRecalcChange change)
{
    return change >= Inherit || needsStyleRecalc() || childNeedsStyleRecalc();
}

inline bool isTreeScopeRoot(const Node* node)
{
    return !node || node->isDocumentNode() || node->isShadowRoot();
}

inline bool isTreeScopeRoot(const Node& node)
{
    return node.isDocumentNode() || node.isShadowRoot();
}

// See the comment at the declaration of ScriptWrappable::fromNode in
// bindings/core/v8/ScriptWrappable.h about why this method is defined here.
inline ScriptWrappable* ScriptWrappable::fromNode(Node* node)
{
    return node;
}

// Allow equality comparisons of Nodes by reference or pointer, interchangeably.
DEFINE_COMPARISON_OPERATORS_WITH_REFERENCES_REFCOUNTED(Node)


#define DEFINE_NODE_TYPE_CASTS(thisType, predicate) \
    template<typename T> inline thisType* to##thisType(const RefPtr<T>& node) { return to##thisType(node.get()); } \
    DEFINE_TYPE_CASTS(thisType, Node, node, node->predicate, node.predicate)

// This requires isClassName(const Node&).
#define DEFINE_NODE_TYPE_CASTS_WITH_FUNCTION(thisType) \
    template<typename T> inline thisType* to##thisType(const RefPtr<T>& node) { return to##thisType(node.get()); } \
    DEFINE_TYPE_CASTS(thisType, Node, node, is##thisType(*node), is##thisType(node))

#define DECLARE_NODE_FACTORY(T) \
    static T* create(Document&)
#define DEFINE_NODE_FACTORY(T) \
T* T::create(Document& document) \
{ \
    return new T(document); \
}

CORE_EXPORT std::ostream& operator<<(std::ostream&, const Node&);
CORE_EXPORT std::ostream& operator<<(std::ostream&, const Node*);

} // namespace blink

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showNode(const blink::Node*);
void showTree(const blink::Node*);
void showNodePath(const blink::Node*);
#endif

#endif // Node_h
