// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_BASEWIDGET_FWL_LISTBOXIMP_H_
#define XFA_FWL_BASEWIDGET_FWL_LISTBOXIMP_H_

#include <memory>

#include "xfa/fwl/basewidget/ifwl_combobox.h"
#include "xfa/fwl/basewidget/ifwl_edit.h"
#include "xfa/fwl/basewidget/ifwl_listbox.h"
#include "xfa/fwl/basewidget/ifwl_scrollbar.h"
#include "xfa/fwl/core/fwl_widgetimp.h"

class CFWL_ListBoxImpDelegate;
class CFWL_MsgKillFocus;
class CFWL_MsgMouse;
class CFWL_MsgMouseWheel;

class CFWL_ListBoxImp : public CFWL_WidgetImp {
 public:
  CFWL_ListBoxImp(const CFWL_WidgetImpProperties& properties,
                  IFWL_Widget* pOuter);
  ~CFWL_ListBoxImp() override;

  // CFWL_WidgetImp
  FWL_Error GetClassName(CFX_WideString& wsClass) const override;
  FWL_Type GetClassID() const override;
  FWL_Error Initialize() override;
  FWL_Error Finalize() override;
  FWL_Error GetWidgetRect(CFX_RectF& rect, FX_BOOL bAutoSize = FALSE) override;
  FWL_Error Update() override;
  FWL_WidgetHit HitTest(FX_FLOAT fx, FX_FLOAT fy) override;
  FWL_Error DrawWidget(CFX_Graphics* pGraphics,
                       const CFX_Matrix* pMatrix = nullptr) override;
  FWL_Error SetThemeProvider(IFWL_ThemeProvider* pThemeProvider) override;

  int32_t CountSelItems();
  FWL_HLISTITEM GetSelItem(int32_t nIndexSel);
  int32_t GetSelIndex(int32_t nIndex);
  FWL_Error SetSelItem(FWL_HLISTITEM hItem, FX_BOOL bSelect = TRUE);
  FWL_Error GetItemText(FWL_HLISTITEM hItem, CFX_WideString& wsText);
  FWL_Error GetScrollPos(FX_FLOAT& fPos, FX_BOOL bVert = TRUE);
  FWL_Error* Sort(IFWL_ListBoxCompare* pCom);

 protected:
  friend class CFWL_ListBoxImpDelegate;

  FWL_HLISTITEM GetItem(FWL_HLISTITEM hItem, uint32_t dwKeyCode);
  void SetSelection(FWL_HLISTITEM hStart,
                    FWL_HLISTITEM hEnd,
                    FX_BOOL bSelected);
  void SetSelectionDirect(FWL_HLISTITEM hItem, FX_BOOL bSelect);
  FX_BOOL IsItemSelected(FWL_HLISTITEM hItem);
  void ClearSelection();
  void SelectAll();
  FWL_HLISTITEM GetFocusedItem();
  void SetFocusItem(FWL_HLISTITEM hItem);
  FWL_HLISTITEM GetItemAtPoint(FX_FLOAT fx, FX_FLOAT fy);
  FX_BOOL GetItemCheckRect(FWL_HLISTITEM hItem, CFX_RectF& rtCheck);
  FX_BOOL SetItemChecked(FWL_HLISTITEM hItem, FX_BOOL bChecked);
  FX_BOOL GetItemChecked(FWL_HLISTITEM hItem);
  FX_BOOL ScrollToVisible(FWL_HLISTITEM hItem);
  void DrawBkground(CFX_Graphics* pGraphics,
                    IFWL_ThemeProvider* pTheme,
                    const CFX_Matrix* pMatrix = NULL);
  void DrawItems(CFX_Graphics* pGraphics,
                 IFWL_ThemeProvider* pTheme,
                 const CFX_Matrix* pMatrix = NULL);
  void DrawItem(CFX_Graphics* pGraphics,
                IFWL_ThemeProvider* pTheme,
                FWL_HLISTITEM hItem,
                int32_t Index,
                const CFX_RectF& rtItem,
                const CFX_Matrix* pMatrix = NULL);
  void DrawStatic(CFX_Graphics* pGraphics, IFWL_ThemeProvider* pTheme);
  CFX_SizeF CalcSize(FX_BOOL bAutoSize = FALSE);
  void GetItemSize(CFX_SizeF& size,
                   FWL_HLISTITEM hItem,
                   FX_FLOAT fWidth,
                   FX_FLOAT fHeight,
                   FX_BOOL bAutoSize = FALSE);
  FX_FLOAT GetMaxTextWidth();
  FX_FLOAT GetScrollWidth();
  FX_FLOAT GetItemHeigt();
  void InitScrollBar(FX_BOOL bVert = TRUE);
  FX_BOOL IsShowScrollBar(FX_BOOL bVert);
  void ProcessSelChanged();

  CFX_RectF m_rtClient;
  CFX_RectF m_rtStatic;
  CFX_RectF m_rtConent;
  std::unique_ptr<IFWL_ScrollBar> m_pHorzScrollBar;
  std::unique_ptr<IFWL_ScrollBar> m_pVertScrollBar;
  uint32_t m_dwTTOStyles;
  int32_t m_iTTOAligns;
  FWL_HLISTITEM m_hAnchor;
  FX_FLOAT m_fItemHeight;
  FX_FLOAT m_fScorllBarWidth;
  FX_BOOL m_bLButtonDown;
  IFWL_ThemeProvider* m_pScrollBarTP;
};
class CFWL_ListBoxImpDelegate : public CFWL_WidgetImpDelegate {
 public:
  CFWL_ListBoxImpDelegate(CFWL_ListBoxImp* pOwner);
  void OnProcessMessage(CFWL_Message* pMessage) override;
  void OnProcessEvent(CFWL_Event* pEvent) override;
  void OnDrawWidget(CFX_Graphics* pGraphics,
                    const CFX_Matrix* pMatrix = NULL) override;

 protected:
  void OnFocusChanged(CFWL_Message* pMsg, FX_BOOL bSet = TRUE);
  void OnLButtonDown(CFWL_MsgMouse* pMsg);
  void OnLButtonUp(CFWL_MsgMouse* pMsg);
  void OnMouseWheel(CFWL_MsgMouseWheel* pMsg);
  void OnKeyDown(CFWL_MsgKey* pMsg);
  void OnVK(FWL_HLISTITEM hItem, FX_BOOL bShift, FX_BOOL bCtrl);
  FX_BOOL OnScroll(IFWL_ScrollBar* pScrollBar, uint32_t dwCode, FX_FLOAT fPos);
  void DispatchSelChangedEv();
  CFWL_ListBoxImp* m_pOwner;
};

#endif  // XFA_FWL_BASEWIDGET_FWL_LISTBOXIMP_H_
