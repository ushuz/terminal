// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "../inc/VtIoModes.hpp"
#include "../renderer/vt/vtrenderer.hpp"
#include "VtInputThread.hpp"
#include "PtySignalInputThread.hpp"

class ConsoleArguments;

namespace Microsoft::Console::Render
{
    class VtEngine;
}

namespace Microsoft::Console::VirtualTerminal
{
    class VtIo
    {
    public:
        VtIo();

        [[nodiscard]] HRESULT Initialize(const ConsoleArguments* const pArgs);

        [[nodiscard]] HRESULT CreateAndStartSignalThread() noexcept;
        [[nodiscard]] HRESULT CreateIoHandlers() noexcept;

        bool IsUsingVt() const;
        bool PassthroughMode() const noexcept;

        [[nodiscard]] HRESULT StartIfNeeded();

        [[nodiscard]] static HRESULT ParseIoMode(const std::wstring& VtMode, _Out_ VtIoMode& ioMode);
        [[nodiscard]] HRESULT SuppressResizeRepaint();
        [[nodiscard]] HRESULT SetCursorPosition(const til::point coordCursor);
        [[nodiscard]] HRESULT SwitchScreenBuffer(const bool useAltBuffer);
        void SendCloseEvent();

        void CloseInput();
        void CloseOutput();

        void BeginResize();
        void EndResize();

#ifdef UNIT_TESTING
        void EnableConptyModeForTests(std::unique_ptr<Microsoft::Console::Render::VtEngine> vtRenderEngine);
#endif

        bool IsResizeQuirkEnabled() const;

        [[nodiscard]] HRESULT ManuallyClearScrollback() const noexcept;

        void CreatePseudoWindow();
        void SetWindowVisibility(bool showOrHide) noexcept;

        using ConcludePassthroughParam = std::pair<const VtIo*, SCREEN_INFORMATION*>;
        static void ConcludePassthrough(const ConcludePassthroughParam& p) noexcept;
        using PassthroughModeCleanup = wil::unique_any<ConcludePassthroughParam, decltype(&ConcludePassthrough), &ConcludePassthrough>;

        [[nodiscard]] PassthroughModeCleanup PrepareForPassthrough(SCREEN_INFORMATION& screenInfo) const;
        void Passthrough(const std::wstring_view& text) const;

    private:
        // After CreateIoHandlers is called, these will be invalid.
        wil::unique_hfile _hInput;
        wil::unique_hfile _hOutput;
        // After CreateAndStartSignalThread is called, this will be invalid.
        wil::unique_hfile _hSignal;
        VtIoMode _IoMode;

        bool _initialized;

        bool _lookingForCursorPosition;

        bool _resizeQuirk{ false };
        bool _passthroughModeArg{ false };
        bool _passthroughMode{ false };
        bool _closeEventSent{ false };

        std::unique_ptr<Microsoft::Console::Render::VtEngine> _pVtRenderEngine;
        std::unique_ptr<Microsoft::Console::VtInputThread> _pVtInputThread;
        std::unique_ptr<Microsoft::Console::PtySignalInputThread> _pPtySignalInputThread;

        [[nodiscard]] HRESULT _Initialize(const HANDLE InHandle, const HANDLE OutHandle, const std::wstring& VtMode, _In_opt_ const HANDLE SignalHandle);

#ifdef UNIT_TESTING
        friend class VtIoTests;
#endif
    };
}
