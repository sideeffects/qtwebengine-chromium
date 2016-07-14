// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_CFWL_EVENT_H_
#define XFA_FWL_CORE_CFWL_EVENT_H_

#include "core/fxcrt/include/fx_coordinates.h"
#include "core/fxcrt/include/fx_string.h"
#include "core/fxcrt/include/fx_system.h"
#include "xfa/fwl/core/cfwl_message.h"
#include "xfa/fwl/core/fwl_error.h"

enum class CFWL_EventType {
  None = 0,

  CheckStateChanged,
  CheckWord,
  Click,
  Close,
  CloseUp,
  ContextMenu,
  DataSelected,
  DateChanged,
  Draw,
  DrawItem,
  DropDown,
  EditChanged,
  GetSuggestedWords,
  HoverChanged,
  Idle,
  Key,
  KillFocus,
  MenuCommand,
  Mouse,
  MouseWheel,
  PostDropDown,
  PreDropDown,
  PreSelfAdaption,
  Scroll,
  SelectChanged,
  SetFocus,
  SizeChanged,
  TextChanged,
  TextFull,
  Validate
};

enum FWLEventMask {
  FWL_EVENT_MOUSE_MASK = 1 << 0,
  FWL_EVENT_MOUSEWHEEL_MASK = 1 << 1,
  FWL_EVENT_KEY_MASK = 1 << 2,
  FWL_EVENT_FOCUSCHANGED_MASK = 1 << 3,
  FWL_EVENT_DRAW_MASK = 1 << 4,
  FWL_EVENT_CLOSE_MASK = 1 << 5,
  FWL_EVENT_SIZECHANGED_MASK = 1 << 6,
  FWL_EVENT_IDLE_MASK = 1 << 7,
  FWL_EVENT_CONTROL_MASK = 1 << 8,
  FWL_EVENT_ALL_MASK = 0xFF
};

class CFX_Graphics;
class IFWL_Widget;

class CFWL_Event {
 public:
  CFWL_Event()
      : m_pSrcTarget(nullptr), m_pDstTarget(nullptr), m_dwRefCount(1) {}
  virtual ~CFWL_Event() {}

  virtual FWL_Error GetClassName(CFX_WideString& wsClass) const {
    return FWL_Error::Succeeded;
  }
  virtual CFWL_EventType GetClassID() const { return CFWL_EventType::None; }

  uint32_t Release() {
    m_dwRefCount--;
    uint32_t dwRefCount = m_dwRefCount;
    if (!m_dwRefCount)
      delete this;
    return dwRefCount;
  }

  IFWL_Widget* m_pSrcTarget;
  IFWL_Widget* m_pDstTarget;

 private:
  uint32_t m_dwRefCount;
};

#define BEGIN_FWL_EVENT_DEF(classname, eventType)                   \
  class classname : public CFWL_Event {                             \
   public:                                                          \
    classname() : CFWL_Event() {}                                   \
    virtual FWL_Error GetClassName(CFX_WideString& wsClass) const { \
      wsClass = L## #classname;                                     \
      return FWL_Error::Succeeded;                                  \
    }                                                               \
    virtual CFWL_EventType GetClassID() const { return eventType; }

#define END_FWL_EVENT_DEF \
  }                       \
  ;  // NOLINT

BEGIN_FWL_EVENT_DEF(CFWL_EvtMouse, CFWL_EventType::Mouse)
FX_FLOAT m_fx;
FX_FLOAT m_fy;
uint32_t m_dwFlags;
FWL_MouseCommand m_dwCmd;
END_FWL_EVENT_DEF

BEGIN_FWL_EVENT_DEF(CFWL_EvtMouseWheel, CFWL_EventType::MouseWheel)
FX_FLOAT m_fx;
FX_FLOAT m_fy;
FX_FLOAT m_fDeltaX;
FX_FLOAT m_fDeltaY;
uint32_t m_dwFlags;
END_FWL_EVENT_DEF

BEGIN_FWL_EVENT_DEF(CFWL_EvtKey, CFWL_EventType::Key)
uint32_t m_dwKeyCode;
uint32_t m_dwFlags;
FWL_KeyCommand m_dwCmd;
END_FWL_EVENT_DEF

BEGIN_FWL_EVENT_DEF(CFWL_EvtSetFocus, CFWL_EventType::SetFocus)
IFWL_Widget* m_pSetFocus;
END_FWL_EVENT_DEF

BEGIN_FWL_EVENT_DEF(CFWL_EvtKillFocus, CFWL_EventType::KillFocus)
IFWL_Widget* m_pKillFocus;
END_FWL_EVENT_DEF

BEGIN_FWL_EVENT_DEF(CFWL_EvtDraw, CFWL_EventType::Draw)
CFX_Graphics* m_pGraphics;
IFWL_Widget* m_pWidget;
END_FWL_EVENT_DEF

BEGIN_FWL_EVENT_DEF(CFWL_EvtClick, CFWL_EventType::Click)
END_FWL_EVENT_DEF

BEGIN_FWL_EVENT_DEF(CFWL_EvtScroll, CFWL_EventType::Scroll)
uint32_t m_iScrollCode;
FX_FLOAT m_fPos;
FX_BOOL* m_pRet;
END_FWL_EVENT_DEF

BEGIN_FWL_EVENT_DEF(CFWL_EvtClose, CFWL_EventType::Close)
END_FWL_EVENT_DEF

BEGIN_FWL_EVENT_DEF(CFWL_EvtContextMenu, CFWL_EventType::ContextMenu)
FX_FLOAT m_fPosX;
FX_FLOAT m_fPosY;
IFWL_Widget* m_pOwner;
END_FWL_EVENT_DEF

BEGIN_FWL_EVENT_DEF(CFWL_EvtMenuCommand, CFWL_EventType::MenuCommand)
int32_t m_iCommand;
void* m_pData;
END_FWL_EVENT_DEF

BEGIN_FWL_EVENT_DEF(CFWL_EvtSizeChanged, CFWL_EventType::SizeChanged)
IFWL_Widget* m_pWidget;
CFX_RectF m_rtOld;
CFX_RectF m_rtNew;
END_FWL_EVENT_DEF

BEGIN_FWL_EVENT_DEF(CFWL_EvtIdle, CFWL_EventType::Idle)
END_FWL_EVENT_DEF

#endif  // XFA_FWL_CORE_CFWL_EVENT_H_
