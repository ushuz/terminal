#include "precomp.h"

#include "UTextAdapter.h"

#include "textBuffer.hpp"

#define utextFieldLength(ut) ut->a
#define utextFieldRow(ut) ut->b
#define utextFieldRowCount(ut) ut->c

static UText* U_CALLCONV utextClone(UText* dest, const UText* src, UBool deep, UErrorCode* status) noexcept
{
    if (deep)
    {
        *status = U_UNSUPPORTED_ERROR;
        return dest;
    }

    dest = utext_setup(dest, 0, status);
    if (U_SUCCESS(*status))
    {
        memcpy(dest, src, sizeof(UText));
    }

    return dest;
}

static int64_t U_CALLCONV utextLength(UText* ut) noexcept
try
{
    auto length = utextFieldLength(ut);

    if (!length)
    {
        const auto& textBuffer = *static_cast<const TextBuffer*>(ut->context);

        for (int32_t i = 0, c = ut->c; i < c; ++i)
        {
            length += textBuffer.GetRowByOffset(i).GetText().size();
        }

        utextFieldLength(ut) = length;
    }

    return length;
}
catch (...)
{
    return 0;
}

static UBool U_CALLCONV utextAccess(UText* ut, int64_t nativeIndex, UBool forward) noexcept
try
{
    if (!forward)
    {
        // Even after reading the ICU documentation I'm a little unclear on how to handle the forward flag.
        // I _think_ it's basically nothing but "nativeIndex--" for us, but I didn't want to verify it
        // because right now we never use any ICU functions that require backwards text access anyways.
        return false;
    }

    const auto& textBuffer = *static_cast<const TextBuffer*>(ut->context);
    auto start = ut->chunkNativeStart;
    auto limit = ut->chunkNativeLimit;
    auto y = utextFieldRow(ut);
    std::wstring_view text;

    if (nativeIndex >= start && nativeIndex < limit)
    {
        return true;
    }

    if (nativeIndex < start)
    {
        for (;;)
        {
            --y;
            if (y < 0)
            {
                return false;
            }

            text = textBuffer.GetRowByOffset(y).GetText();
            limit = start;
            start -= text.size();

            if (nativeIndex >= start)
            {
                break;
            }
        }
    }
    else
    {
        const auto rowCount = utextFieldRowCount(ut);

        for (;;)
        {
            ++y;
            if (y >= rowCount)
            {
                return false;
            }

            text = textBuffer.GetRowByOffset(y).GetText();
            start = limit;
            limit += text.size();

            if (nativeIndex < limit)
            {
                break;
            }
        }
    }

    utextFieldRow(ut) = y;
    ut->chunkNativeStart = start;
    ut->chunkNativeLimit = limit;
    ut->chunkOffset = gsl::narrow_cast<int32_t>(nativeIndex - start);
    ut->chunkLength = gsl::narrow_cast<int32_t>(text.size());
    ut->chunkContents = reinterpret_cast<const char16_t*>(text.data());
    ut->nativeIndexingLimit = ut->chunkLength;
    return true;
}
catch (...)
{
    return false;
}

static int32_t U_CALLCONV utextExtract(UText* ut, int64_t nativeStart, int64_t nativeLimit, char16_t* dest, int32_t destCapacity, UErrorCode* status) noexcept
try
{
    if (U_FAILURE(*status))
    {
        return 0;
    }
    if (destCapacity < 0 || (dest == nullptr && destCapacity > 0) || nativeStart > nativeLimit)
    {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    if (!utextAccess(ut, nativeStart, true))
    {
        return 0;
    }

    nativeLimit = std::min(ut->chunkNativeLimit, nativeLimit);

    if (destCapacity <= 0)
    {
        return gsl::narrow_cast<int32_t>(nativeLimit - nativeStart);
    }

    const auto& textBuffer = *static_cast<const TextBuffer*>(ut->context);
    const auto y = utextFieldRow(ut);
    const auto text = textBuffer.GetRowByOffset(y).GetText();

    const auto offset = ut->chunkNativeStart - nativeStart;
    const auto length = std::min(gsl::narrow_cast<size_t>(destCapacity), gsl::narrow_cast<size_t>(nativeLimit - ut->chunkNativeStart));
    memcpy(dest, text.data() + offset, length * sizeof(char16_t));

    if (length < destCapacity)
    {
        dest[length] = 0;
    }

    return gsl::narrow_cast<int32_t>(length);
}
catch (...)
{
    // The only thing that can fail is GetRowByOffset() which in turn can only fail when VirtualAlloc() fails.
    *status = U_MEMORY_ALLOCATION_ERROR;
    return 0;
}

static constexpr UTextFuncs ucstrFuncs{
    .tableSize = sizeof(UTextFuncs),
    .clone = utextClone,
    .nativeLength = utextLength,
    .access = utextAccess,
    .extract = utextExtract,
};

UText* Microsoft::Buffer::ICU::UTextFromTextBuffer(UText* ut, const TextBuffer& textBuffer, UErrorCode* status) noexcept
{
    ut = utext_setup(ut, 0, status);
    if (U_FAILURE(*status))
    {
        return nullptr;
    }

    ut->providerProperties = (1 << UTEXT_PROVIDER_LENGTH_IS_EXPENSIVE) | (1 << UTEXT_PROVIDER_STABLE_CHUNKS);
    ut->pFuncs = &ucstrFuncs;
    ut->context = &textBuffer;
    utextFieldRow(ut) = -1; // the utextAccess() below will advance this to row 0.
    utextFieldRowCount(ut) = textBuffer.GetSize().Height();
    utextAccess(ut, 0, true);
    return ut;
}

til::point_span Microsoft::Buffer::ICU::BufferRangeFromUText(UText* ut, int64_t nativeIndexBeg, int64_t nativeIndexEnd)
{
    // The parameters are given as a half-open [beg,end) range, but the point_span we return in closed [beg,end].
    nativeIndexEnd--;

    const auto& textBuffer = *static_cast<const TextBuffer*>(ut->context);
    til::point_span ret;

    if (!utextAccess(ut, nativeIndexBeg, true))
    {
        return {};
    }

    ret.start.x = textBuffer.GetRowByOffset(utextFieldRow(ut)).GetLeftAlignedColumnAtChar(nativeIndexBeg - ut->chunkNativeStart);
    ret.start.y = utextFieldRow(ut);
    
    if (!utextAccess(ut, nativeIndexEnd, true))
    {
        return {};
    }

    ret.end.x = textBuffer.GetRowByOffset(utextFieldRow(ut)).GetLeftAlignedColumnAtChar(nativeIndexEnd - ut->chunkNativeStart);
    ret.end.y = utextFieldRow(ut);
    return ret;
}

Microsoft::Buffer::ICU::UniqueURegex Microsoft::Buffer::ICU::URegularExpressionCreate(const std::wstring_view& pattern, UErrorCode* error)
{
    const auto re = uregex_open(reinterpret_cast<const char16_t*>(pattern.data()), static_cast<int32_t>(pattern.size()), UREGEX_LITERAL | UREGEX_CASE_INSENSITIVE, nullptr, error);
    // ICU describes the time unit as being dependent on CPU performance and "typically [in] the order of milliseconds",
    // but this claim seems highly outdated already. On my CPU from 2021, a limit of 4096 equals roughly 600ms.
    uregex_setTimeLimit(re, 4096, error);
    uregex_setStackLimit(re, 4 * 1024 * 1024, error);
    return UniqueURegex{ re };
}
