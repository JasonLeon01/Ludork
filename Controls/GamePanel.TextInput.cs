using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

namespace Ludork.Controls;

public sealed partial class GamePanel
{
    private const uint CharacterMessage = 0x0102;
    private const uint UnicodeCharacterMessage = 0x0109;
    private const uint ImeStartCompositionMessage = 0x010D;
    private const uint ImeEndCompositionMessage = 0x010E;
    private const uint ImeCompositionMessage = 0x010F;
    private const uint ImeSetContextMessage = 0x0281;
    private const uint CompositionString = 0x0008;
    private const uint CompositionCursor = 0x0080;
    private readonly HashSet<int> imeConsumedScanCodes = [];
    private nint originalInputContext;
    private nint textInputContext;
    private string? textInputSession;
    private char pendingHighSurrogate;
    private int pendingSurrogateRepeats;
    private bool composing;
    private NativeRectangle textCaretRectangle;

    public void ApplyTextInput(RuntimeTextInputMessage message)
    {
        if (!OperatingSystem.IsWindows() || NativeHandle == nint.Zero)
            return;
        if (message.Action == "end")
        {
            if (textInputSession == message.Session)
                resetTextInputSession();
            return;
        }
        if (message.Action == "begin")
        {
            if (!canForwardOrdinaryInput(NativeHandle) || GetFocus() != NativeHandle)
                return;
            resetTextInputSession();
            textInputSession = message.Session;
            ImmAssociateContext(NativeHandle, textInputContext);
        }
        else if (message.Action != "update" || textInputSession != message.Session)
        {
            return;
        }
        textCaretRectangle = new()
        {
            Left = (int)message.X,
            Top = (int)message.Y,
            Right = (int)(message.X + message.Width),
            Bottom = (int)(message.Y + message.Height),
        };
        updateTextInputPosition();
    }

    public void ResetTextInput()
    {
        pendingEvents.Clear();
        resetTextInputSession();
    }

    private void resetTextInputSession()
    {
        textInputSession = null;
        composing = false;
        pendingHighSurrogate = '\0';
        pendingSurrogateRepeats = 0;
        imeConsumedScanCodes.Clear();
        if (!OperatingSystem.IsWindows() || NativeHandle == nint.Zero)
            return;
        if (textInputContext != nint.Zero)
            ImmNotifyIME(textInputContext, 0x0015, 0x0004, 0);
        ImmAssociateContext(NativeHandle, nint.Zero);
    }

    private void initializeTextInput()
    {
        originalInputContext = ImmAssociateContext(NativeHandle, nint.Zero);
        textInputContext = ImmCreateContext();
    }

    private void destroyTextInput()
    {
        resetTextInputSession();
        ImmAssociateContext(NativeHandle, originalInputContext);
        if (textInputContext != nint.Zero)
            ImmDestroyContext(textInputContext);
        originalInputContext = nint.Zero;
        textInputContext = nint.Zero;
    }

    private bool processTextInputMessage(nint handle, uint message, nint wParam, nint lParam, out nint result)
    {
        result = nint.Zero;
        if (message == UnicodeCharacterMessage && wParam == new nint(0xFFFF))
        {
            result = new nint(1);
            return true;
        }
        if (message == ImeSetContextMessage && textInputSession is not null)
        {
            nint flags = new(lParam.ToInt64() & ~0x80000000L);
            result = previousWindowProcedure == nint.Zero
                ? DefWindowProcW(handle, message, wParam, flags)
                : CallWindowProcW(previousWindowProcedure, handle, message, wParam, flags);
            return true;
        }
        if (!canForwardOrdinaryInput(handle) || GetFocus() != handle)
            return false;
        if (message == CharacterMessage)
        {
            enqueueCharacter((char)wParam, Math.Max(1, (int)(lParam.ToInt64() & 0xFFFF)));
            return true;
        }
        if (message == UnicodeCharacterMessage)
        {
            pendingHighSurrogate = '\0';
            pendingSurrogateRepeats = 0;
            enqueueUnicode((int)wParam, Math.Max(1, (int)(lParam.ToInt64() & 0xFFFF)));
            return true;
        }
        if (textInputSession is null)
            return false;
        if (message == ImeStartCompositionMessage)
        {
            setTextComposition(true);
            updateTextInputPosition();
        }
        else if (message == ImeCompositionMessage)
        {
            long flags = lParam.ToInt64();
            if ((flags & (CompositionString | CompositionCursor)) != 0)
            {
                setTextComposition(true);
                enqueueTextPreedit();
            }
            else if (flags == 0)
            {
                enqueue(new("TextPreedit", Session: textInputSession, Text: "", PreeditCaret: 0));
            }
            updateTextInputPosition();
        }
        else if (message == ImeEndCompositionMessage)
        {
            enqueue(new("TextPreedit", Session: textInputSession, Text: "", PreeditCaret: 0));
            setTextComposition(false);
        }
        return false;
    }

    private void enqueueCharacter(char character, int repeats)
    {
        if (char.IsHighSurrogate(character))
        {
            pendingHighSurrogate = character;
            pendingSurrogateRepeats = repeats;
            return;
        }
        if (char.IsLowSurrogate(character))
        {
            if (pendingHighSurrogate != '\0')
                enqueueUnicode(char.ConvertToUtf32(pendingHighSurrogate, character), Math.Min(pendingSurrogateRepeats, repeats));
        }
        else
        {
            enqueueUnicode(character, repeats);
        }
        pendingHighSurrogate = '\0';
        pendingSurrogateRepeats = 0;
    }

    private void enqueueUnicode(int scalar, int repeats)
    {
        if (!Rune.IsValid(scalar) || Rune.IsControl(new Rune(scalar)))
            return;
        for (int repeat = 0; repeat < repeats; ++repeat)
            enqueue(new("TextEntered", Session: textInputSession, Unicode: scalar));
    }

    private void setTextComposition(bool value)
    {
        if (composing == value)
            return;
        composing = value;
        enqueue(new("TextComposition", Session: textInputSession, Composing: value));
    }

    private void enqueueTextPreedit()
    {
        int byteCount = ImmGetCompositionStringW(textInputContext, CompositionString, null, 0);
        if (byteCount < 0)
            return;
        byte[] bytes = new byte[byteCount];
        if (byteCount > 0)
        {
            int copied = ImmGetCompositionStringW(textInputContext, CompositionString, bytes, (uint)byteCount);
            if (copied < 0)
                return;
            byteCount = Math.Min(byteCount, copied);
        }
        string text = Encoding.Unicode.GetString(bytes, 0, byteCount & ~1);
        int cursor = Math.Clamp(ImmGetCompositionStringW(textInputContext, CompositionCursor, null, 0), 0, text.Length);
        if (cursor > 0 && cursor < text.Length && char.IsHighSurrogate(text[cursor - 1]) && char.IsLowSurrogate(text[cursor]))
            --cursor;
        enqueue(new("TextPreedit", Session: textInputSession, Text: text, PreeditCaret: Encoding.UTF8.GetByteCount(text.AsSpan(0, cursor))));
    }

    private bool consumeTextInputKeyDown(nint wParam, nint lParam)
    {
        int scanCode = getTextInputScanCode(lParam);
        if (textInputSession is not null && (composing || (int)wParam == 0xE5))
        {
            imeConsumedScanCodes.Add(scanCode);
            return true;
        }
        return imeConsumedScanCodes.Contains(scanCode);
    }

    private void releaseTextInputKey(nint lParam)
    {
        imeConsumedScanCodes.Remove(getTextInputScanCode(lParam));
    }

    private static int getTextInputScanCode(nint lParam)
    {
        return (int)((lParam.ToInt64() >> 16) & 0x1FF);
    }

    private void updateTextInputPosition()
    {
        if (textInputContext == nint.Zero || textInputSession is null)
            return;
        NativeCompositionForm composition = new()
        {
            Style = 0x0002,
            Position = new(textCaretRectangle.Left, textCaretRectangle.Bottom),
        };
        ImmSetCompositionWindow(textInputContext, ref composition);
        NativeCandidateForm candidate = new()
        {
            Style = 0x0080,
            Position = composition.Position,
            Area = textCaretRectangle,
        };
        ImmSetCandidateWindow(textInputContext, ref candidate);
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeRectangle
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeCompositionForm
    {
        public uint Style;
        public NativePoint Position;
        public NativeRectangle Area;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeCandidateForm
    {
        public uint Index;
        public uint Style;
        public NativePoint Position;
        public NativeRectangle Area;
    }

    [DllImport("imm32.dll")]
    private static extern nint ImmAssociateContext(nint handle, nint context);

    [DllImport("imm32.dll")]
    private static extern nint ImmCreateContext();

    [DllImport("imm32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool ImmDestroyContext(nint context);

    [DllImport("imm32.dll")]
    private static extern int ImmGetCompositionStringW(nint context, uint index, [Out] byte[]? buffer, uint bufferLength);

    [DllImport("imm32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool ImmNotifyIME(nint context, uint action, uint index, uint value);

    [DllImport("imm32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool ImmSetCompositionWindow(nint context, ref NativeCompositionForm form);

    [DllImport("imm32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool ImmSetCandidateWindow(nint context, ref NativeCandidateForm form);
}
