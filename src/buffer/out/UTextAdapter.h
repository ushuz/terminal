#pragma once

#include <icu.h>

class TextBuffer;

namespace Microsoft::Buffer::ICU
{
    UText* UTextFromTextBuffer(UText* ut, const TextBuffer& textBuffer, UErrorCode* status) noexcept;
    til::point_span BufferRangeFromUText(UText* ut, int64_t nativeIndexBeg, int64_t nativeIndexEnd);

    using UniqueURegex = wistd::unique_ptr<URegularExpression, wil::function_deleter<decltype(&uregex_close), &uregex_close>>;

    UniqueURegex URegularExpressionCreate(const std::wstring_view& pattern, UErrorCode* error);
}
