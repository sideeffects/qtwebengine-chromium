// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/include/xfa_ffdoc.h"

#include <algorithm>

#include "core/fpdfapi/fpdf_parser/include/cpdf_array.h"
#include "core/fpdfapi/fpdf_parser/include/cpdf_document.h"
#include "core/fpdfdoc/include/fpdf_doc.h"
#include "core/fxcrt/include/fx_ext.h"
#include "core/fxcrt/include/fx_memory.h"
#include "xfa/fde/xml/fde_xml_imp.h"
#include "xfa/fgas/crt/fgas_algorithm.h"
#include "xfa/fwl/core/fwl_noteimp.h"
#include "xfa/fxfa/app/xfa_ffnotify.h"
#include "xfa/fxfa/include/xfa_checksum.h"
#include "xfa/fxfa/include/xfa_ffapp.h"
#include "xfa/fxfa/include/xfa_ffdocview.h"
#include "xfa/fxfa/include/xfa_ffwidget.h"
#include "xfa/fxfa/include/xfa_fontmgr.h"
#include "xfa/fxfa/parser/xfa_document.h"
#include "xfa/fxfa/parser/xfa_document_serialize.h"
#include "xfa/fxfa/parser/xfa_parser.h"
#include "xfa/fxfa/parser/xfa_parser_imp.h"
#include "xfa/fxfa/parser/xfa_parser_imp.h"

CXFA_FFDoc::CXFA_FFDoc(CXFA_FFApp* pApp, IXFA_DocProvider* pDocProvider)
    : m_pDocProvider(pDocProvider),
      m_pDocument(nullptr),
      m_pStream(nullptr),
      m_pApp(pApp),
      m_pNotify(nullptr),
      m_pPDFDoc(nullptr),
      m_dwDocType(XFA_DOCTYPE_Static),
      m_bOwnStream(TRUE) {}
CXFA_FFDoc::~CXFA_FFDoc() {
  CloseDoc();
}
uint32_t CXFA_FFDoc::GetDocType() {
  return m_dwDocType;
}
int32_t CXFA_FFDoc::StartLoad() {
  m_pNotify = new CXFA_FFNotify(this);
  CXFA_DocumentParser* pDocParser = new CXFA_DocumentParser(m_pNotify);
  int32_t iStatus = pDocParser->StartParse(m_pStream);
  m_pDocument = pDocParser->GetDocument();
  return iStatus;
}
FX_BOOL XFA_GetPDFContentsFromPDFXML(CFDE_XMLNode* pPDFElement,
                                     uint8_t*& pByteBuffer,
                                     int32_t& iBufferSize) {
  CFDE_XMLElement* pDocumentElement = NULL;
  for (CFDE_XMLNode* pXMLNode =
           pPDFElement->GetNodeItem(CFDE_XMLNode::FirstChild);
       pXMLNode; pXMLNode = pXMLNode->GetNodeItem(CFDE_XMLNode::NextSibling)) {
    if (pXMLNode->GetType() == FDE_XMLNODE_Element) {
      CFX_WideString wsTagName;
      CFDE_XMLElement* pXMLElement = static_cast<CFDE_XMLElement*>(pXMLNode);
      pXMLElement->GetTagName(wsTagName);
      if (wsTagName == FX_WSTRC(L"document")) {
        pDocumentElement = pXMLElement;
        break;
      }
    }
  }
  if (!pDocumentElement) {
    return FALSE;
  }
  CFDE_XMLElement* pChunkElement = NULL;
  for (CFDE_XMLNode* pXMLNode =
           pDocumentElement->GetNodeItem(CFDE_XMLNode::FirstChild);
       pXMLNode; pXMLNode = pXMLNode->GetNodeItem(CFDE_XMLNode::NextSibling)) {
    if (pXMLNode->GetType() == FDE_XMLNODE_Element) {
      CFX_WideString wsTagName;
      CFDE_XMLElement* pXMLElement = static_cast<CFDE_XMLElement*>(pXMLNode);
      pXMLElement->GetTagName(wsTagName);
      if (wsTagName == FX_WSTRC(L"chunk")) {
        pChunkElement = pXMLElement;
        break;
      }
    }
  }
  if (!pChunkElement) {
    return FALSE;
  }
  CFX_WideString wsPDFContent;
  pChunkElement->GetTextData(wsPDFContent);
  iBufferSize =
      FX_Base64DecodeW(wsPDFContent.c_str(), wsPDFContent.GetLength(), NULL);
  pByteBuffer = FX_Alloc(uint8_t, iBufferSize + 1);
  pByteBuffer[iBufferSize] = '0';  // FIXME: I bet this is wrong.
  FX_Base64DecodeW(wsPDFContent.c_str(), wsPDFContent.GetLength(), pByteBuffer);
  return TRUE;
}
void XFA_XPDPacket_MergeRootNode(CXFA_Node* pOriginRoot, CXFA_Node* pNewRoot) {
  CXFA_Node* pChildNode = pNewRoot->GetNodeItem(XFA_NODEITEM_FirstChild);
  while (pChildNode) {
    CXFA_Node* pOriginChild =
        pOriginRoot->GetFirstChildByName(pChildNode->GetNameHash());
    if (pOriginChild) {
      pChildNode = pChildNode->GetNodeItem(XFA_NODEITEM_NextSibling);
    } else {
      CXFA_Node* pNextSibling =
          pChildNode->GetNodeItem(XFA_NODEITEM_NextSibling);
      pNewRoot->RemoveChild(pChildNode);
      pOriginRoot->InsertChild(pChildNode);
      pChildNode = pNextSibling;
      pNextSibling = NULL;
    }
  }
}
int32_t CXFA_FFDoc::DoLoad(IFX_Pause* pPause) {
  int32_t iStatus = m_pDocument->GetParser()->DoParse(pPause);
  if (iStatus == XFA_PARSESTATUS_Done && !m_pPDFDoc) {
    CXFA_Node* pPDFNode = ToNode(m_pDocument->GetXFAObject(XFA_HASHCODE_Pdf));
    if (!pPDFNode) {
      return XFA_PARSESTATUS_SyntaxErr;
    }
    CFDE_XMLNode* pPDFXML = pPDFNode->GetXMLMappingNode();
    if (pPDFXML->GetType() != FDE_XMLNODE_Element) {
      return XFA_PARSESTATUS_SyntaxErr;
    }
    int32_t iBufferSize = 0;
    uint8_t* pByteBuffer = NULL;
    IFX_FileRead* pXFAReader = NULL;
    if (XFA_GetPDFContentsFromPDFXML(pPDFXML, pByteBuffer, iBufferSize)) {
      pXFAReader = FX_CreateMemoryStream(pByteBuffer, iBufferSize, TRUE);
    } else {
      CFX_WideString wsHref;
      static_cast<CFDE_XMLElement*>(pPDFXML)->GetString(L"href", wsHref);
      if (!wsHref.IsEmpty()) {
        pXFAReader = GetDocProvider()->OpenLinkedFile(this, wsHref);
      }
    }
    if (!pXFAReader) {
      return XFA_PARSESTATUS_SyntaxErr;
    }
    CPDF_Document* pPDFDocument =
        GetDocProvider()->OpenPDF(this, pXFAReader, TRUE);
    ASSERT(!m_pPDFDoc);
    if (!OpenDoc(pPDFDocument)) {
      return XFA_PARSESTATUS_SyntaxErr;
    }
    IXFA_Parser* pParser = IXFA_Parser::Create(m_pDocument, TRUE);
    if (!pParser) {
      return XFA_PARSESTATUS_SyntaxErr;
    }
    CXFA_Node* pRootNode = NULL;
    if (pParser->StartParse(m_pStream) == XFA_PARSESTATUS_Ready &&
        pParser->DoParse(NULL) == XFA_PARSESTATUS_Done) {
      pRootNode = pParser->GetRootNode();
    }
    if (pRootNode && m_pDocument->GetRoot()) {
      XFA_XPDPacket_MergeRootNode(m_pDocument->GetRoot(), pRootNode);
      iStatus = XFA_PARSESTATUS_Done;
    } else {
      iStatus = XFA_PARSESTATUS_StatusErr;
    }
    pParser->Release();
    pParser = NULL;
  }
  return iStatus;
}
void CXFA_FFDoc::StopLoad() {
  m_pApp->GetXFAFontMgr()->LoadDocFonts(this);
  m_dwDocType = XFA_DOCTYPE_Static;
  CXFA_Node* pConfig = ToNode(m_pDocument->GetXFAObject(XFA_HASHCODE_Config));
  if (!pConfig) {
    return;
  }
  CXFA_Node* pAcrobat = pConfig->GetFirstChildByClass(XFA_ELEMENT_Acrobat);
  if (!pAcrobat) {
    return;
  }
  CXFA_Node* pAcrobat7 = pAcrobat->GetFirstChildByClass(XFA_ELEMENT_Acrobat7);
  if (!pAcrobat7) {
    return;
  }
  CXFA_Node* pDynamicRender =
      pAcrobat7->GetFirstChildByClass(XFA_ELEMENT_DynamicRender);
  if (!pDynamicRender) {
    return;
  }
  CFX_WideString wsType;
  if (pDynamicRender->TryContent(wsType) && wsType == FX_WSTRC(L"required")) {
    m_dwDocType = XFA_DOCTYPE_Dynamic;
  }
}

CXFA_FFDocView* CXFA_FFDoc::CreateDocView(uint32_t dwView) {
  if (!m_TypeToDocViewMap[dwView])
    m_TypeToDocViewMap[dwView].reset(new CXFA_FFDocView(this));

  return m_TypeToDocViewMap[dwView].get();
}

CXFA_FFDocView* CXFA_FFDoc::GetDocView(CXFA_LayoutProcessor* pLayout) {
  for (const auto& pair : m_TypeToDocViewMap) {
    if (pair.second->GetXFALayout() == pLayout)
      return pair.second.get();
  }
  return nullptr;
}

CXFA_FFDocView* CXFA_FFDoc::GetDocView() {
  auto it = m_TypeToDocViewMap.begin();
  return it != m_TypeToDocViewMap.end() ? it->second.get() : nullptr;
}

FX_BOOL CXFA_FFDoc::OpenDoc(IFX_FileRead* pStream, FX_BOOL bTakeOverFile) {
  m_bOwnStream = bTakeOverFile;
  m_pStream = pStream;
  return TRUE;
}
FX_BOOL CXFA_FFDoc::OpenDoc(CPDF_Document* pPDFDoc) {
  if (pPDFDoc == NULL) {
    return FALSE;
  }
  CPDF_Dictionary* pRoot = pPDFDoc->GetRoot();
  if (pRoot == NULL) {
    return FALSE;
  }
  CPDF_Dictionary* pAcroForm = pRoot->GetDictBy("AcroForm");
  if (pAcroForm == NULL) {
    return FALSE;
  }
  CPDF_Object* pElementXFA = pAcroForm->GetDirectObjectBy("XFA");
  if (pElementXFA == NULL) {
    return FALSE;
  }
  CFX_ArrayTemplate<CPDF_Stream*> xfaStreams;
  if (pElementXFA->IsArray()) {
    CPDF_Array* pXFAArray = (CPDF_Array*)pElementXFA;
    for (size_t i = 0; i < pXFAArray->GetCount() / 2; i++) {
      if (CPDF_Stream* pStream = pXFAArray->GetStreamAt(i * 2 + 1))
        xfaStreams.Add(pStream);
    }
  } else if (pElementXFA->IsStream()) {
    xfaStreams.Add((CPDF_Stream*)pElementXFA);
  }
  if (xfaStreams.GetSize() < 1) {
    return FALSE;
  }
  IFX_FileRead* pFileRead = new CXFA_FileRead(xfaStreams);
  m_pPDFDoc = pPDFDoc;
  if (m_pStream) {
    m_pStream->Release();
    m_pStream = NULL;
  }
  m_pStream = pFileRead;
  m_bOwnStream = TRUE;
  return TRUE;
}
FX_BOOL CXFA_FFDoc::CloseDoc() {
  for (const auto& pair : m_TypeToDocViewMap)
    pair.second->RunDocClose();

  if (m_pDocument)
    m_pDocument->ClearLayoutData();

  m_TypeToDocViewMap.clear();

  if (m_pDocument) {
    m_pDocument->GetParser()->Release();
    m_pDocument = nullptr;
  }

  delete m_pNotify;
  m_pNotify = nullptr;

  m_pApp->GetXFAFontMgr()->ReleaseDocFonts(this);

  if (m_dwDocType != XFA_DOCTYPE_XDP && m_pStream && m_bOwnStream) {
    m_pStream->Release();
    m_pStream = nullptr;
  }

  for (const auto& pair : m_HashToDibDpiMap)
    delete pair.second.pDibSource;

  m_HashToDibDpiMap.clear();

  FWL_GetApp()->GetNoteDriver()->ClearEventTargets(FALSE);
  return TRUE;
}
void CXFA_FFDoc::SetDocType(uint32_t dwType) {
  m_dwDocType = dwType;
}
CPDF_Document* CXFA_FFDoc::GetPDFDoc() {
  return m_pPDFDoc;
}

CFX_DIBitmap* CXFA_FFDoc::GetPDFNamedImage(const CFX_WideStringC& wsName,
                                           int32_t& iImageXDpi,
                                           int32_t& iImageYDpi) {
  if (!m_pPDFDoc)
    return nullptr;

  uint32_t dwHash = FX_HashCode_GetW(wsName, false);
  auto it = m_HashToDibDpiMap.find(dwHash);
  if (it != m_HashToDibDpiMap.end()) {
    iImageXDpi = it->second.iImageXDpi;
    iImageYDpi = it->second.iImageYDpi;
    return static_cast<CFX_DIBitmap*>(it->second.pDibSource);
  }

  CPDF_Dictionary* pRoot = m_pPDFDoc->GetRoot();
  if (!pRoot)
    return nullptr;

  CPDF_Dictionary* pNames = pRoot->GetDictBy("Names");
  if (!pNames)
    return nullptr;

  CPDF_Dictionary* pXFAImages = pNames->GetDictBy("XFAImages");
  if (!pXFAImages)
    return nullptr;

  CPDF_NameTree nametree(pXFAImages);
  CFX_ByteString bsName = PDF_EncodeText(wsName.c_str(), wsName.GetLength());
  CPDF_Object* pObject = nametree.LookupValue(bsName);
  if (!pObject) {
    for (size_t i = 0; i < nametree.GetCount(); i++) {
      CFX_ByteString bsTemp;
      CPDF_Object* pTempObject = nametree.LookupValue(i, bsTemp);
      if (bsTemp == bsName) {
        pObject = pTempObject;
        break;
      }
    }
  }

  CPDF_Stream* pStream = ToStream(pObject);
  if (!pStream)
    return nullptr;

  CPDF_StreamAcc streamAcc;
  streamAcc.LoadAllData(pStream);

  IFX_FileRead* pImageFileRead =
      FX_CreateMemoryStream((uint8_t*)streamAcc.GetData(), streamAcc.GetSize());

  CFX_DIBitmap* pDibSource = XFA_LoadImageFromBuffer(
      pImageFileRead, FXCODEC_IMAGE_UNKNOWN, iImageXDpi, iImageYDpi);
  m_HashToDibDpiMap[dwHash] = {pDibSource, iImageXDpi, iImageYDpi};
  pImageFileRead->Release();
  return pDibSource;
}

CFDE_XMLElement* CXFA_FFDoc::GetPackageData(const CFX_WideStringC& wsPackage) {
  uint32_t packetHash = FX_HashCode_GetW(wsPackage, false);
  CXFA_Node* pNode = ToNode(m_pDocument->GetXFAObject(packetHash));
  if (!pNode) {
    return NULL;
  }
  CFDE_XMLNode* pXMLNode = pNode->GetXMLMappingNode();
  return (pXMLNode && pXMLNode->GetType() == FDE_XMLNODE_Element)
             ? static_cast<CFDE_XMLElement*>(pXMLNode)
             : NULL;
}

FX_BOOL CXFA_FFDoc::SavePackage(const CFX_WideStringC& wsPackage,
                                IFX_FileWrite* pFile,
                                CXFA_ChecksumContext* pCSContext) {
  std::unique_ptr<CXFA_DataExporter> pExport(
      new CXFA_DataExporter(m_pDocument));
  uint32_t packetHash = FX_HashCode_GetW(wsPackage, false);
  CXFA_Node* pNode = packetHash == XFA_HASHCODE_Xfa
                         ? m_pDocument->GetRoot()
                         : ToNode(m_pDocument->GetXFAObject(packetHash));
  if (!pNode)
    return pExport->Export(pFile);

  CFX_ByteString bsChecksum;
  if (pCSContext)
    pCSContext->GetChecksum(bsChecksum);

  return pExport->Export(pFile, pNode, 0,
                         bsChecksum.GetLength() ? bsChecksum.c_str() : nullptr);
}

FX_BOOL CXFA_FFDoc::ImportData(IFX_FileRead* pStream, FX_BOOL bXDP) {
  std::unique_ptr<CXFA_DataImporter> importer(
      new CXFA_DataImporter(m_pDocument));
  return importer->ImportData(pStream);
}
