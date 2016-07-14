// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/dwrite_text_analysis_source_win.h"

#include "base/logging.h"

namespace content {

TextAnalysisSource::TextAnalysisSource() = default;
TextAnalysisSource::~TextAnalysisSource() = default;

HRESULT TextAnalysisSource::GetLocaleName(UINT32 text_position,
                                          UINT32* text_length,
                                          const WCHAR** locale_name) {
  if (text_position >= text_.length() || !text_length || !locale_name)
    return E_INVALIDARG;
  *text_length = text_.length() - text_position;
  *locale_name = locale_name_.c_str();
  return S_OK;
}

HRESULT TextAnalysisSource::GetNumberSubstitution(
    UINT32 text_position,
    UINT32* text_length,
    IDWriteNumberSubstitution** number_substitution) {
  if (text_position >= text_.length() || !text_length || !number_substitution)
    return E_INVALIDARG;
  *text_length = text_.length() - text_position;
  number_substitution_.CopyTo(number_substitution);
  return S_OK;
}

DWRITE_READING_DIRECTION TextAnalysisSource::GetParagraphReadingDirection() {
  return reading_direction_;
}

HRESULT TextAnalysisSource::GetTextAtPosition(UINT32 text_position,
                                              const WCHAR** text_string,
                                              UINT32* text_length) {
  if (!text_length || !text_string)
    return E_INVALIDARG;
  if (text_position >= text_.length()) {
    *text_string = nullptr;
    *text_length = 0;
    return S_OK;
  }
  *text_string = text_.c_str() + text_position;
  *text_length = text_.length() - text_position;
  return S_OK;
}

HRESULT TextAnalysisSource::GetTextBeforePosition(UINT32 text_position,
                                                  const WCHAR** text_string,
                                                  UINT32* text_length) {
  if (!text_length || !text_string)
    return E_INVALIDARG;
  if (text_position < 1 || text_position > text_.length()) {
    *text_string = nullptr;
    *text_length = 0;
    return S_OK;
  }
  *text_string = text_.c_str();
  *text_length = text_position;
  return S_OK;
}

HRESULT TextAnalysisSource::RuntimeClassInitialize(
    const base::string16& text,
    const base::string16& locale_name,
    IDWriteNumberSubstitution* number_substitution,
    DWRITE_READING_DIRECTION reading_direction) {
  DCHECK(number_substitution);
  text_ = text;
  locale_name_ = locale_name;
  number_substitution_ = number_substitution;
  reading_direction_ = reading_direction;
  return S_OK;
}

}  // namespace content
